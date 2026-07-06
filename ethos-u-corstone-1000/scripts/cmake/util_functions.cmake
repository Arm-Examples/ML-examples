#----------------------------------------------------------------------------
#  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its
#  affiliates <open-source-office@arm.com>
#  SPDX-License-Identifier: Apache-2.0
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#----------------------------------------------------------------------------
include_guard(GLOBAL)

# Define and report a cache-backed user option.
#
# The option is only initialized when it is not already defined, so command-line
# or preset values are preserved. The option name is also recorded in the
# USER_OPTIONS internal cache list for later reporting.
#
# Arguments:
#   name
#     Cache variable name.
#   description
#     Cache help text.
#   default
#     Default value used when the cache variable is not already defined.
#   type
#     CMake cache entry type, such as BOOL, STRING, PATH, or FILEPATH.
function(USER_OPTION name description default type)
    if (NOT DEFINED ${name})
        set(${name} "${default}" CACHE ${type} "${description}")
    endif()

    message(STATUS "User option ${name} is set to ${${name}}")
    list(APPEND USER_OPTIONS ${name})
    set(USER_OPTIONS ${USER_OPTIONS} CACHE INTERNAL "")
endfunction()

# Convert an arbitrary identifier into a conservative file-name-safe value.
#
# Characters outside letters, numbers, underscore, period, and dash are
# replaced with underscores. The result is suitable for generated file names
# where path separators, whitespace, and shell-special characters should not be
# preserved.
#
# Arguments:
#   output_variable
#     Name of the variable that receives the safe value, returned with
#     PARENT_SCOPE.
#   value
#     Input value to sanitize.
#
# Outputs:
#   output_variable
#     Receives the sanitized file name.
#
# Examples:
#   make_safe_file_name(name "direct-drive/image classification zero input")
#     name receives direct-drive_image_classification_zero_input
function(make_safe_file_name output_variable value)
    string(REGEX REPLACE "[^A-Za-z0-9_.-]" "_" safe_value "${value}")
    set("${output_variable}" "${safe_value}" PARENT_SCOPE)
endfunction()
