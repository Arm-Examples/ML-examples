> [!CAUTION]
> Deprecated example.
> Please note, this example is no longer actively maintained and is provided as-is.

# RNN unrolling for TFLite

This repository contains a Jupyter notebook that will demonstrate how to train a Recurrent Neural Network (RNN) in TensorFlow and then prepare it for exporting to [TensorFlow Lite](https://www.tensorflow.org/lite) format by unrolling it.

It also shows how to quantize your unrolled model so that it is ready to be optimized by [Vela](https://pypi.org/project/ethos-u-vela/) in order for the model to be deployed onto [Arm Ethos-U55](https://www.arm.com/products/silicon-ip-cpu/ethos/ethos-u55) or [Arm Ethos-U65](https://www.arm.com/products/silicon-ip-cpu/ethos/ethos-u65) NPU.
