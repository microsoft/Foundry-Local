# Copyright (c) Microsoft. All rights reserved.
#
# Standalone build for the Foundry Local C++ "responses-api" (vision) sample.
# Build the SDK first:  python ../../../sdk_v2/cpp/build.py
# Then:                 cmake -S . -B build && cmake --build build

cmake_minimum_required(VERSION 3.20)
project(foundry_local_responses_api_sample CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Locate the locally-built SDK and define the foundry_local_cpp target.
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/FoundryLocalSDK.cmake)

add_executable(responses_api main.cc)

# Shared sample helpers (HTTP client, base64) live in samples/cpp/common.
target_include_directories(responses_api PRIVATE ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(responses_api PRIVATE foundry_local_cpp)

# Absolute path to this sample dir so the bundled test image is found regardless
# of where the executable runs from.
target_compile_definitions(responses_api PRIVATE SAMPLE_SOURCE_DIR="${CMAKE_CURRENT_LIST_DIR}")

# Bake in the rpath so the executable finds the SDK shared library at runtime.
foundry_local_configure_sample(responses_api)
