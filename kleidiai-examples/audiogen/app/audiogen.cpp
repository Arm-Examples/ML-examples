/*
 * SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its
 * affiliates <open-source-office@arm.com>
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "litert/cc/litert_compiled_model.h"
#include "litert/cc/litert_environment.h"
#include "litert/cc/litert_layout.h"
#include "litert/cc/litert_options.h"
#include "litert/cc/litert_ranked_tensor_type.h"
#include "litert/cc/litert_tensor_buffer.h"
#include "tflite/delegates/xnnpack/xnnpack_delegate.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <iterator>
#include <random>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sentencepiece_processor.h>

constexpr int32_t k_audio_sr = 44100;
constexpr int32_t k_audio_num_channels = 2;
constexpr int32_t k_bits_per_sample = 32;

constexpr size_t k_seed_default = 99;
constexpr size_t k_audio_len_sec_default = 10;
constexpr size_t k_num_steps_default = 8;

// -- Update the tensor index based on your model configuration.
constexpr size_t k_t5_ids_in_idx = 0;
constexpr size_t k_t5_attnmask_in_idx = 1;
constexpr size_t k_t5_audio_len_in_idx = 2;
constexpr size_t k_t5_crossattn_out_idx = 0;
constexpr size_t k_t5_globalcond_out_idx = 2;

constexpr size_t k_dit_crossattn_in_idx = 2;
constexpr size_t k_dit_globalcond_in_idx = 1;
constexpr size_t k_dit_x_in_idx = 3;
constexpr size_t k_dit_t_in_idx = 0;
constexpr size_t k_dit_out_idx = 0;

// -- Fill sigmas params
constexpr float k_logsnr_max = -6.0f;
constexpr float k_sigma_min = 0.0f;
constexpr float k_sigma_max = 1.0f;

// -- XNNPack Flags base
constexpr uint32_t k_xnnpack_flags_base =
    TFLITE_XNNPACK_DELEGATE_FLAG_QS8 |
    TFLITE_XNNPACK_DELEGATE_FLAG_QU8 |
    TFLITE_XNNPACK_DELEGATE_FLAG_DYNAMIC_FULLY_CONNECTED |
    TFLITE_XNNPACK_DELEGATE_FLAG_VARIABLE_OPERATORS |
    TFLITE_XNNPACK_DELEGATE_FLAG_ENABLE_LATEST_OPERATORS |
    TFLITE_XNNPACK_DELEGATE_FLAG_ENABLE_SUBGRAPH_RESHAPING;

#define AUDIOGEN_CHECK(x)                                 \
    if (!(x)) {                                                 \
        fprintf(stderr, "Error at %s:%d\n", __FILE__, __LINE__);\
        exit(1);                                                \
    }

static inline long time_in_ms() {
    using namespace std::chrono;
    auto now = time_point_cast<milliseconds>(steady_clock::now());
    return now.time_since_epoch().count();
}

static void print_usage(const char *name) {
    fprintf(stderr,
        "Usage: %s -m <models_base_path> -p <prompt> -t <num_threads> [-s <seed> -l <audio_len>]\n\n"
        "Options:\n"
        "  -m <models_base_path>   Path to model files\n"
        "  -p <prompt>             Input prompt text (e.g., warm arpeggios on house beats 120BPM with drums effect)\n"
        "  -t <num_threads>        Number of CPU threads to use\n"
        "  -s <seed>               (Optional) Random seed for reproducibility. Different seeds generate different audio samples (Default: %zu)\n"
        "  -i <input_audio_path>   (Optional) Add input audio file for style transfer"
        "  -x <sigma_max>          (Optional) Hyper parameter to tweak noise level"
        "  -l <audio_len_sec>      (Optional) Length of generated audio (Default: %zu s)\n"
        "  -n <num_steps>          (Optional) Number of steps (Default: %zu)\n"
        "  -o <output_file>        (Optional) Output audio file name (Default: <prompt>_<seed>.wav)\n"
        "  -h                      Show this help message\n",
        name,
        k_seed_default,
        k_audio_len_sec_default,
        k_num_steps_default);
}

static std::string get_filename(std::string prompt, size_t seed) {
    // Convert spaces to underscores
    std::replace(prompt.begin(), prompt.end(), ' ', '_');

    // Convert to lowercase
    std::transform(prompt.begin(), prompt.end(), prompt.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return prompt + "_" + std::to_string(seed) + ".wav";
}

static std::vector<int32_t> convert_prompt_to_ids(const std::string& prompt, const std::string& spiece_model_path) {
    sentencepiece::SentencePieceProcessor sp;

    AUDIOGEN_CHECK(sp.Load(spiece_model_path.c_str()).ok());

    std::vector<std::string> pieces;
    std::vector<int32_t> ids;

    sp.Encode(prompt, &pieces);  // Token strings
    sp.Encode(prompt, &ids);     // Token IDs

    // Make sure we have 1 at the end
    if(ids[ids.size() - 1] != 1) {
        ids.push_back(1);
    }
    return ids;
}

template <typename T>
static T get_litert_value(litert::Expected<T>&& value) {
    AUDIOGEN_CHECK(value);
    if constexpr (std::is_reference_v<T>) {
        return *value;
    } else {
        return std::move(*value);
    }
}

static size_t get_num_elems(const litert::RankedTensorType& type) {
    return get_litert_value(type.Layout().NumElements());
}

static litert::Options create_cpu_options(size_t num_threads, uint32_t xnnpack_flags) {
    auto options = get_litert_value(litert::Options::Create());
    AUDIOGEN_CHECK(options.SetHardwareAccelerators(litert::HwAccelerators::kCpu));
    auto& cpu_options = get_litert_value(options.GetCpuOptions());
    AUDIOGEN_CHECK(cpu_options.SetNumThreads(static_cast<int>(num_threads)));
    AUDIOGEN_CHECK(cpu_options.SetXNNPackFlags(xnnpack_flags));
    return options;
}

static void read_wav(const std::string& path, std::vector<float>& left_ch, std::vector<float>& right_ch) {
    // You can use this command to convert the file to the expected format:
    // ffmpeg -i input_audio.mp3 -ar 44100 -ac 2 -c:a pcm_f32le -f wav output.wav

    constexpr uint16_t wave_format_pcm        = 0x0001;
    constexpr uint16_t wave_format_ieee_float = 0x0003;
    constexpr uint16_t wave_format_extensible = 0xFFFE;

    std::ifstream input_stream(path, std::ios::binary);

    AUDIOGEN_CHECK(input_stream);

    char riff[4], wave[4], fmt[4];
    uint32_t riff_size, fmt_chunk_sz;
    uint16_t audio_format, audio_num_channels;
    uint32_t audio_sr, byte_rate, data_chunk_sz;
    uint16_t block_align, audio_bits_per_sample;

    std::vector<float> data_chunk;

    std::streampos riff_base = input_stream.tellg();
    input_stream.read(riff, 4);
    input_stream.read(reinterpret_cast<char*>(&riff_size), 4);
    AUDIOGEN_CHECK(bool(riff_size));
    input_stream.read(wave, 4);
    if(std::string(riff, 4) != "RIFF" || std::string(wave, 4) != "WAVE") {
        fprintf(stderr,
            "BAD file, or unsupported format, use this ffmpeg command to convert your file:\n"
            "ffmpeg -i input_audio.mp3 -ar 44100 -ac 2 -c:a pcm_f32le -f wav output.wav\n\n");
        exit(EXIT_FAILURE);
    }

    input_stream.read(fmt, 4);
    AUDIOGEN_CHECK(std::string(fmt, 4) == "fmt ");
    input_stream.read(reinterpret_cast<char*>(&fmt_chunk_sz), 4);
    AUDIOGEN_CHECK(fmt_chunk_sz >= 16);

    input_stream.read(reinterpret_cast<char*>(&audio_format), 2);
    input_stream.read(reinterpret_cast<char*>(&audio_num_channels), 2);
    input_stream.read(reinterpret_cast<char*>(&audio_sr), 4);
    input_stream.read(reinterpret_cast<char*>(&byte_rate), 4);
    input_stream.read(reinterpret_cast<char*>(&block_align), 2);
    input_stream.read(reinterpret_cast<char*>(&audio_bits_per_sample), 2);

    if (!(audio_format == wave_format_ieee_float || audio_format == wave_format_pcm || audio_format == wave_format_extensible) ||
        audio_num_channels != k_audio_num_channels ||
        audio_sr != k_audio_sr ||
        audio_bits_per_sample != k_bits_per_sample) {
        fprintf(stderr,
        "Unsupported WAV format (need 44.1kHz, stereo, 32-bit float), use this ffmpeg command to convert your file:\n"
        "ffmpeg -i input_audio.mp3 -ar 44100 -ac 2 -c:a pcm_f32le -f wav output.wav\n\n");
        exit(EXIT_FAILURE);
    }

    // Skip any extension bytes in the fmt chunk
    if (fmt_chunk_sz > 16) {
        input_stream.seekg(static_cast<std::streamoff>(fmt_chunk_sz - 16), std::ios::cur);
        AUDIOGEN_CHECK(bool(input_stream));
    }

    // Compute absolute end of this RIFF chunk: 8 (header) + riff_size bytes
    const std::streampos riff_end = riff_base + static_cast<std::streamoff>(8ull + riff_size);

    // Now we scan for the "data" chunk
    char chunk_id[4];
    uint32_t chunk_size = 0;
    for (;;) {
        std::streampos here = input_stream.tellg();
        AUDIOGEN_CHECK(here != std::streampos(-1));

        AUDIOGEN_CHECK(input_stream.read(chunk_id, 4));
        AUDIOGEN_CHECK(input_stream.read(reinterpret_cast<char*>(&chunk_size), 4));

        if (std::string(chunk_id, 4) == "data") {
            data_chunk_sz = chunk_size;
            // Ensure the whole chunk fits in RIFF
            AUDIOGEN_CHECK(input_stream.tellg() + static_cast<std::streamoff>(data_chunk_sz) <= riff_end);
            break;
        }
        // word-align skip (chunks are padded to even sizes)
        input_stream.seekg(static_cast<std::streamoff>(chunk_size + (chunk_size & 1)), std::ios::cur);
        AUDIOGEN_CHECK(bool(input_stream));
    }

    const uint32_t num_frames = data_chunk_sz / block_align;
    const uint32_t total_samples = num_frames * k_audio_num_channels;

    data_chunk.resize(total_samples);
    input_stream.read(reinterpret_cast<char*>(data_chunk.data()),
               static_cast<std::streamsize>(data_chunk_sz));

    // We have the data in interleaved format (L0, R0, L1, R1,....)
    // We need to unpack the data into two channels, as this is the expected input shape to the encoder
    left_ch.resize(num_frames);
    right_ch.resize(num_frames);
    for(int i = 0; i < num_frames; ++i) {
        left_ch[i] = data_chunk[i * 2 + 0];
        right_ch[i] = data_chunk[i * 2 + 1];
    }
}

static void prepare_encoder_input(const std::vector<float>& left_ch, const std::vector<float>& right_ch, float* packed, const size_t audio_input_dim0) {

    AUDIOGEN_CHECK(left_ch.size() == right_ch.size());
    const int32_t num_frames = audio_input_dim0 / 2;

    for(int i = 0; i < num_frames; ++i) {
        packed[i]            = left_ch[i];
        packed[num_frames+i] = right_ch[i];
    }
}

static void encode_audio(const std::string& audio_input_path, const std::string& encoder_model_path, std::vector<float>& encoded_audio, size_t num_threads, litert::Environment& env) {

    std::vector<float> packed;
    std::vector<float> left_ch_input;
    std::vector<float> right_ch_input;

    // Read input audio file
    read_wav(audio_input_path, left_ch_input, right_ch_input);
    fprintf(stderr, "Using %s as an audio input file...\n", audio_input_path.c_str());

    auto encoder_options = get_litert_value(litert::Options::Create());
    AUDIOGEN_CHECK(encoder_options.SetHardwareAccelerators(litert::HwAccelerators::kCpu));
    auto& encoder_cpu_options = get_litert_value(encoder_options.GetCpuOptions());
    AUDIOGEN_CHECK(encoder_cpu_options.SetNumThreads(static_cast<int>(num_threads)));
    AUDIOGEN_CHECK(encoder_cpu_options.SetXNNPackFlags(k_xnnpack_flags_base | TFLITE_XNNPACK_DELEGATE_FLAG_FORCE_FP16));

    auto autoencoder_encoder_model = get_litert_value(litert::CompiledModel::Create(env, encoder_model_path, encoder_options));

    auto encoder_inputs = get_litert_value(autoencoder_encoder_model.CreateInputBuffers());
    auto encoder_outputs = get_litert_value(autoencoder_encoder_model.CreateOutputBuffers());
    AUDIOGEN_CHECK(encoder_inputs.size() >= 1);
    AUDIOGEN_CHECK(encoder_outputs.size() >= 1);

    auto encoder_in_type = get_litert_value(autoencoder_encoder_model.GetInputTensorType(0, 0));
    auto encoder_out_type = get_litert_value(autoencoder_encoder_model.GetOutputTensorType(0, 0));

    const size_t audio_input_dim0 = get_num_elems(encoder_in_type);

    // Divided by 2 because we have two channels
    AUDIOGEN_CHECK(left_ch_input.size() <= audio_input_dim0 / 2);
    AUDIOGEN_CHECK(right_ch_input.size() <= audio_input_dim0 / 2);

    // Resize if needed and fill with zero the newly added values
    left_ch_input.resize(audio_input_dim0 / 2, 0);
    right_ch_input.resize(audio_input_dim0 / 2, 0);

    // Pack the data
    {
        auto encoder_in_ptr = get_litert_value(encoder_inputs[0].Lock(litert::TensorBuffer::LockMode::kWrite));
        auto* autoencoder_encoder_in_data = static_cast<float*>(encoder_in_ptr);
        prepare_encoder_input(left_ch_input, right_ch_input, autoencoder_encoder_in_data, audio_input_dim0);
        AUDIOGEN_CHECK(encoder_inputs[0].Unlock());
    }

    // Run the encoder
    auto start_encoder = time_in_ms();
    AUDIOGEN_CHECK(autoencoder_encoder_model.Run(encoder_inputs, encoder_outputs));
    auto end_encoder = time_in_ms();

    // Copy the output to the output buffer
    const size_t encoder_output_num_elems = get_num_elems(encoder_out_type);
    {
        auto encoder_out_ptr = get_litert_value(encoder_outputs[0].Lock(litert::TensorBuffer::LockMode::kRead));
        auto* autoencoder_encoder_out_data = static_cast<float*>(encoder_out_ptr);
        encoded_audio.resize(encoder_output_num_elems);
        memcpy(encoded_audio.data(), autoencoder_encoder_out_data, encoder_output_num_elems * sizeof(float));
        AUDIOGEN_CHECK(encoder_outputs[0].Unlock());
    }

    auto encoder_exec_time = (end_encoder - start_encoder);
    fprintf(stderr, "Encoder time: %ld ms\n", encoder_exec_time);
}

static void save_as_wav(const std::string& path, const float* left_ch, const float* right_ch, size_t buffer_sz) {

    constexpr uint16_t audio_format = 3; // IEEE float

    const int32_t byte_rate = k_audio_sr * k_audio_num_channels * (k_bits_per_sample / 8);
    const int32_t block_align = k_audio_num_channels * (k_bits_per_sample / 8);
    const int32_t data_chunk_sz = buffer_sz * 2 * sizeof(float);
    const int32_t fmt_chunk_sz = 16;
    const int32_t header_sz = 44;
    const int32_t file_sz = header_sz + data_chunk_sz - 8;

    std::ofstream out_file(path, std::ios::binary);

    // Prepare the header
    // RIFF header
    out_file.write("RIFF", 4);
    out_file.write(reinterpret_cast<const char*>(&file_sz), 4);
    out_file.write("WAVE", 4);
    out_file.write("fmt ", 4);
    out_file.write(reinterpret_cast<const char*>(&fmt_chunk_sz), 4);
    out_file.write(reinterpret_cast<const char*>(&audio_format), 2);
    out_file.write(reinterpret_cast<const char*>(&k_audio_num_channels), 2);
    out_file.write(reinterpret_cast<const char*>(&k_audio_sr), 4);
    out_file.write(reinterpret_cast<const char*>(&byte_rate), 4);
    out_file.write(reinterpret_cast<const char*>(&block_align), 2);
    out_file.write(reinterpret_cast<const char*>(&k_bits_per_sample), 2);

    // Store the data in interleaved format (L0, R0, L1, R1,....)
    out_file.write("data", 4);
    out_file.write(reinterpret_cast<const char*>(&data_chunk_sz), 4);

    for (size_t i = 0; i < buffer_sz; ++i) {
        out_file.write(reinterpret_cast<const char*>(&left_ch[i]), sizeof(float));
        out_file.write(reinterpret_cast<const char*>(&right_ch[i]), sizeof(float));
    }

    out_file.close();
}

static void fill_random_norm_dist(float* buff, size_t buff_sz, size_t seed) {
    std::random_device rd{};
    std::mt19937 gen(seed);
    std::normal_distribution<float> dis(0.0f, 1.0f);

    auto gen_fn = [&dis, &gen](){ return dis(gen); };
    std::generate(buff, buff + buff_sz, gen_fn);
}

static void fill_sigmas(std::vector<float>& arr, float start, float end, float sigma_max) {

    const int32_t sz = static_cast<int32_t>(arr.size());
    const float step = ((end - start) / static_cast<float> (sz - 1));

    // Linspace
    arr[0]      = start;
    arr[sz - 1] = end;

    for(int32_t i = 1; i < sz - 1; ++i) {
        arr[i] = arr[i - 1] + step;
    }

    // Sigmoid(-logsnr)
    for(int32_t i = 0; i < sz; ++i) {
        arr[i] = 1.0f / (1.0f + std::exp(arr[i])) ;
    }

    arr[0]      = sigma_max;
    arr[sz - 1] = k_sigma_min;
}

static void sampler_ping_pong(float* dit_out_data, float* dit_x_in_data, size_t dit_x_in_sz, float cur_t, float next_t, size_t step_idx, size_t seed) {

    for(size_t i = 0; i < dit_x_in_sz; i++) {
        dit_out_data[i] = dit_x_in_data[i] - ( cur_t * dit_out_data[i]);
    }

    std::vector<float> rand_tensor(dit_x_in_sz);
    fill_random_norm_dist(rand_tensor.data(), dit_x_in_sz, seed);

    // x = (1-t_next) * denoised + t_next * torch.randn_like(x)
    for(size_t i = 0; i < dit_x_in_sz; i++) {
        dit_x_in_data[i] = ((1.0f - next_t) * dit_out_data[i]) + (next_t * rand_tensor[i]);
    }
}

int main(int32_t argc, char** argv) {

    // ----- Parse the cmd line arguments
    // ----------------------------------
    // Required arguments
    std::string models_base_path = "";
    std::string prompt           = "";
    std::string audio_input_path = "";
    size_t num_threads           = 0;
    // Optional arguments
    std::string output_file      = "";
    size_t seed                  = k_seed_default;
    size_t num_steps             = k_num_steps_default;
    float audio_len_sec          = static_cast<float>(k_audio_len_sec_default);
    float sigma_max              = static_cast<float>(k_sigma_max);

    int opt;
    while ((opt = getopt(argc, argv, "m:p:t:i:x:s:n:o:l:h")) != -1) {
        switch (opt) {
            case 'm': models_base_path = optarg; break;
            case 'p': prompt           = optarg; break;
            case 't': num_threads      = std::stoull(optarg); break;
            case 'i': audio_input_path = optarg; break;
            case 'x': sigma_max        = static_cast<float>(std::stof(optarg)); break;
            case 's': seed             = std::stoull(optarg); break;
            case 'n': num_steps        = std::stoull(optarg); break;
            case 'o': output_file      = optarg; break;
            case 'l': audio_len_sec    = static_cast<float>(std::stoull(optarg)); break;
            case 'h':
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    // Check the mandatory arguments
    if (models_base_path.empty() || prompt.empty() || num_threads <= 0) {
        fprintf(stderr, "ERROR: Missing required arguments.\n\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if(sigma_max <= 0 || sigma_max >  1) {
        fprintf(stderr, "noise_level (sigma_max) must be between (0,1] \n");
        return EXIT_FAILURE;
    }

    std::string t5_tflite = models_base_path + "/conditioners_float32.tflite";
    std::string dit_tflite = models_base_path + "/dit_model.tflite";
    std::string autoencoder_tflite = models_base_path + "/autoencoder_model.tflite";
    std::string autoencoder_encoder_tflite = models_base_path + "/autoencoder_encoder_model.tflite";
    std::string sentence_model_path = models_base_path + "/spiece.model";

    auto env = get_litert_value(litert::Environment::Create({}));

    // If there is input audio, run the encoder model and release it, to avoid overloading memory
    std::vector<float> encoded_audio;
    if(!audio_input_path.empty()) {
        encode_audio(audio_input_path, autoencoder_encoder_tflite, encoded_audio, num_threads, env);
    }

    // ----- Compile LiteRT models
    // ----------------------------------
    auto t5_options = create_cpu_options(num_threads, k_xnnpack_flags_base);
    auto dit_options = create_cpu_options(num_threads, k_xnnpack_flags_base);
    auto autoencoder_options = create_cpu_options(num_threads, k_xnnpack_flags_base);

    auto t5_model = get_litert_value(litert::CompiledModel::Create(env, t5_tflite, t5_options));
    auto dit_model = get_litert_value(litert::CompiledModel::Create(env, dit_tflite, dit_options));
    auto autoencoder_model = get_litert_value(litert::CompiledModel::Create(env, autoencoder_tflite, autoencoder_options));

    auto t5_inputs = get_litert_value(t5_model.CreateInputBuffers());
    auto t5_outputs = get_litert_value(t5_model.CreateOutputBuffers());
    auto dit_inputs = get_litert_value(dit_model.CreateInputBuffers());
    auto dit_outputs = get_litert_value(dit_model.CreateOutputBuffers());
    auto autoencoder_inputs = get_litert_value(autoencoder_model.CreateInputBuffers());
    auto autoencoder_outputs = get_litert_value(autoencoder_model.CreateOutputBuffers());

    AUDIOGEN_CHECK(t5_inputs.size() > k_t5_audio_len_in_idx);
    AUDIOGEN_CHECK(t5_outputs.size() > k_t5_globalcond_out_idx);
    AUDIOGEN_CHECK(dit_inputs.size() > k_dit_x_in_idx);
    AUDIOGEN_CHECK(dit_outputs.size() > k_dit_out_idx);
    AUDIOGEN_CHECK(autoencoder_inputs.size() >= 1);
    AUDIOGEN_CHECK(autoencoder_outputs.size() >= 1);

    // ----- Query tensor sizes
    auto t5_ids_in_type = get_litert_value(t5_model.GetInputTensorType(0, k_t5_ids_in_idx));
    auto t5_attnmask_in_type = get_litert_value(t5_model.GetInputTensorType(0, k_t5_attnmask_in_idx));
    auto t5_time_in_type = get_litert_value(t5_model.GetInputTensorType(0, k_t5_audio_len_in_idx));
    auto t5_crossattn_out_type = get_litert_value(t5_model.GetOutputTensorType(0, k_t5_crossattn_out_idx));
    auto t5_globalcond_out_type = get_litert_value(t5_model.GetOutputTensorType(0, k_t5_globalcond_out_idx));

    auto dit_x_in_type = get_litert_value(dit_model.GetInputTensorType(0, k_dit_x_in_idx));
    auto dit_t_in_type = get_litert_value(dit_model.GetInputTensorType(0, k_dit_t_in_idx));
    auto dit_crossattn_in_type = get_litert_value(dit_model.GetInputTensorType(0, k_dit_crossattn_in_idx));
    auto dit_globalcond_in_type = get_litert_value(dit_model.GetInputTensorType(0, k_dit_globalcond_in_idx));
    auto dit_out_type = get_litert_value(dit_model.GetOutputTensorType(0, k_dit_out_idx));

    auto autoencoder_in_type = get_litert_value(autoencoder_model.GetInputTensorType(0, 0));
    auto autoencoder_out_type = get_litert_value(autoencoder_model.GetOutputTensorType(0, 0));

    const size_t t5_ids_num_elems = get_num_elems(t5_ids_in_type);
    const size_t t5_attnmask_num_elems = get_num_elems(t5_attnmask_in_type);
    const size_t t5_time_num_elems = get_num_elems(t5_time_in_type);
    const size_t t5_crossattn_num_elems = get_num_elems(t5_crossattn_out_type);
    const size_t t5_globalcond_num_elems = get_num_elems(t5_globalcond_out_type);

    const size_t dit_x_num_elems = get_num_elems(dit_x_in_type);
    const size_t dit_t_num_elems = get_num_elems(dit_t_in_type);
    const size_t dit_crossattn_num_elems = get_num_elems(dit_crossattn_in_type);
    const size_t dit_globalcond_num_elems = get_num_elems(dit_globalcond_in_type);
    const size_t dit_out_num_elems = get_num_elems(dit_out_type);
    const size_t autoencoder_in_num_elems = get_num_elems(autoencoder_in_type);
    const size_t autoencoder_out_num_elems = get_num_elems(autoencoder_out_type);

    AUDIOGEN_CHECK(t5_crossattn_num_elems == dit_crossattn_num_elems);
    AUDIOGEN_CHECK(t5_globalcond_num_elems == dit_globalcond_num_elems);
    AUDIOGEN_CHECK(dit_x_num_elems == autoencoder_in_num_elems);
    AUDIOGEN_CHECK(dit_out_num_elems == dit_x_num_elems);
    AUDIOGEN_CHECK(dit_t_num_elems >= 1);
    AUDIOGEN_CHECK(t5_time_num_elems >= 1);

    // ----- Allocate the extra buffer to pre-compute the sigmas
    std::vector<float> t_buffer(num_steps + 1);

    // ----- Initialize the T and X buffers
    {
        auto dit_x_ptr = get_litert_value(dit_inputs[k_dit_x_in_idx].Lock(litert::TensorBuffer::LockMode::kWrite));
        auto* dit_x_in_data = static_cast<float*>(dit_x_ptr);
        fill_random_norm_dist(dit_x_in_data, dit_x_num_elems, seed);

        if(!audio_input_path.empty()) {
            AUDIOGEN_CHECK(encoded_audio.size() == dit_x_num_elems);
            for(size_t i = 0; i < dit_x_num_elems; ++i) {
                dit_x_in_data[i] =
                    encoded_audio[i] * (1 - sigma_max) +
                    dit_x_in_data[i] * sigma_max;
            }
        }
        AUDIOGEN_CHECK(dit_inputs[k_dit_x_in_idx].Unlock());
    }

    float logsnr_max = k_logsnr_max;
    if(sigma_max < 1) {
        logsnr_max = std::log(((1-sigma_max)/sigma_max) + 1e-6);
    }

    fill_sigmas(t_buffer, logsnr_max, 2.0f, sigma_max);

    // Convert the prompt to IDs
    std::vector<int32_t> ids = convert_prompt_to_ids(prompt, sentence_model_path);
    AUDIOGEN_CHECK(ids.size() <= t5_ids_num_elems);

    // Initialize T5 inputs
    {
        auto t5_ids_ptr = get_litert_value(t5_inputs[k_t5_ids_in_idx].Lock(litert::TensorBuffer::LockMode::kWrite));
        auto* t5_ids_in_data = static_cast<int64_t*>(t5_ids_ptr);
        memset(t5_ids_in_data, 0, t5_ids_num_elems * sizeof(int64_t));
        for(size_t i = 0; i < ids.size(); ++i) {
            t5_ids_in_data[i] = ids[i];
        }
        AUDIOGEN_CHECK(t5_inputs[k_t5_ids_in_idx].Unlock());
    }

    {
        auto t5_attn_ptr = get_litert_value(t5_inputs[k_t5_attnmask_in_idx].Lock(litert::TensorBuffer::LockMode::kWrite));
        auto* t5_attnmask_in_data = static_cast<int64_t*>(t5_attn_ptr);
        memset(t5_attnmask_in_data, 0, t5_attnmask_num_elems * sizeof(int64_t));
        for(size_t i = 0; i < ids.size(); i++) {
            t5_attnmask_in_data[i] = 1;
        }
        AUDIOGEN_CHECK(t5_inputs[k_t5_attnmask_in_idx].Unlock());
    }

    {
        auto t5_time_ptr = get_litert_value(t5_inputs[k_t5_audio_len_in_idx].Lock(litert::TensorBuffer::LockMode::kWrite));
        auto* t5_time_in_data = static_cast<float*>(t5_time_ptr);
        memset(t5_time_in_data, 0, t5_time_num_elems * sizeof(float));
        memcpy(t5_time_in_data, &audio_len_sec, sizeof(float));
        AUDIOGEN_CHECK(t5_inputs[k_t5_audio_len_in_idx].Unlock());
    }

    auto start_t5 = time_in_ms();
    AUDIOGEN_CHECK(t5_model.Run(t5_inputs, t5_outputs));
    auto end_t5 = time_in_ms();

    // Copy T5 outputs to DiT inputs (constants for diffusion loop)
    {
        auto t5_cross_ptr = get_litert_value(t5_outputs[k_t5_crossattn_out_idx].Lock(litert::TensorBuffer::LockMode::kRead));
        auto dit_cross_ptr = get_litert_value(dit_inputs[k_dit_crossattn_in_idx].Lock(litert::TensorBuffer::LockMode::kWrite));
        memcpy(dit_cross_ptr, t5_cross_ptr, t5_crossattn_num_elems * sizeof(float));
        AUDIOGEN_CHECK(dit_inputs[k_dit_crossattn_in_idx].Unlock());
        AUDIOGEN_CHECK(t5_outputs[k_t5_crossattn_out_idx].Unlock());
    }

    {
        auto t5_global_ptr = get_litert_value(t5_outputs[k_t5_globalcond_out_idx].Lock(litert::TensorBuffer::LockMode::kRead));
        auto dit_global_ptr = get_litert_value(dit_inputs[k_dit_globalcond_in_idx].Lock(litert::TensorBuffer::LockMode::kWrite));
        memcpy(dit_global_ptr, t5_global_ptr, t5_globalcond_num_elems * sizeof(float));
        AUDIOGEN_CHECK(dit_inputs[k_dit_globalcond_in_idx].Unlock());
        AUDIOGEN_CHECK(t5_outputs[k_t5_globalcond_out_idx].Unlock());
    }

    auto start_dit = time_in_ms();

    for(size_t i = 0; i < num_steps; ++i) {
        const float curr_t = t_buffer[i];
        const float next_t = t_buffer[i + 1];
        {
            auto dit_t_ptr = get_litert_value(dit_inputs[k_dit_t_in_idx].Lock(litert::TensorBuffer::LockMode::kWrite));
            auto* dit_t_in_data = static_cast<float*>(dit_t_ptr);
            memcpy(dit_t_in_data, &curr_t, sizeof(float));
            AUDIOGEN_CHECK(dit_inputs[k_dit_t_in_idx].Unlock());
        }

        // Run DiT
        AUDIOGEN_CHECK(dit_model.Run(dit_inputs, dit_outputs));

        // Update x for next step
        auto dit_out_ptr = get_litert_value(dit_outputs[k_dit_out_idx].Lock(litert::TensorBuffer::LockMode::kReadWrite));
        auto dit_x_ptr = get_litert_value(dit_inputs[k_dit_x_in_idx].Lock(litert::TensorBuffer::LockMode::kReadWrite));
        auto* dit_out_data = static_cast<float*>(dit_out_ptr);
        auto* dit_x_in_data = static_cast<float*>(dit_x_ptr);

        sampler_ping_pong(dit_out_data, dit_x_in_data, dit_x_num_elems, curr_t, next_t, i, seed + i + 4564);

        AUDIOGEN_CHECK(dit_inputs[k_dit_x_in_idx].Unlock());
        AUDIOGEN_CHECK(dit_outputs[k_dit_out_idx].Unlock());
    }
    auto end_dit = time_in_ms();

    auto start_autoencoder = time_in_ms();

    // Initialize the autoencoder's input from DiT x
    {
        auto dit_x_ptr = get_litert_value(dit_inputs[k_dit_x_in_idx].Lock(litert::TensorBuffer::LockMode::kRead));
        auto auto_in_ptr = get_litert_value(autoencoder_inputs[0].Lock(litert::TensorBuffer::LockMode::kWrite));
        memcpy(auto_in_ptr, dit_x_ptr, dit_x_num_elems * sizeof(float));
        AUDIOGEN_CHECK(autoencoder_inputs[0].Unlock());
        AUDIOGEN_CHECK(dit_inputs[k_dit_x_in_idx].Unlock());
    }

    // Run AutoEncoder
    AUDIOGEN_CHECK(autoencoder_model.Run(autoencoder_inputs, autoencoder_outputs));

    auto end_autoencoder = time_in_ms();

    const size_t num_audio_samples = autoencoder_out_num_elems / 2;

    // If output filename empty -> filename = <prompt>_<seed>.wav
    if (output_file.empty()) {
        output_file = get_filename(prompt, seed);
    }

    {
        auto auto_out_ptr = get_litert_value(autoencoder_outputs[0].Lock(litert::TensorBuffer::LockMode::kRead));
        const float* autoencoder_out_data = static_cast<const float*>(auto_out_ptr);
        const float* left_ch = autoencoder_out_data;
        const float* right_ch = autoencoder_out_data + num_audio_samples;
        save_as_wav(output_file.c_str(), left_ch, right_ch, num_audio_samples);
        AUDIOGEN_CHECK(autoencoder_outputs[0].Unlock());
    }

    // Save the file
    auto t5_exec_time          = (end_t5 - start_t5);
    auto dit_exec_time         = (end_dit - start_dit);
    auto dit_avg_step_time     = (dit_exec_time / static_cast<float>(num_steps));
    auto autoencoder_exec_time = (end_autoencoder - start_autoencoder);
    auto total_exec_time       = t5_exec_time + dit_exec_time + autoencoder_exec_time;

    printf("T5: %ld ms\n", t5_exec_time);
    printf("DiT: %ld ms\n", dit_exec_time);
    printf("DiT Avg per step: %f ms\n", dit_avg_step_time);
    printf("Autoencoder: %ld ms\n", autoencoder_exec_time);
    printf("Total run time: %ld ms\n", total_exec_time);

    return 0;
}
