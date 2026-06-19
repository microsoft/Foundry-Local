# Copyright (c) Microsoft. All rights reserved.
#
# Standalone build for the Foundry Local C++ "embeddings" sample.
# Build the SDK first:  python ../../../sdk_v2/cpp/build.py
# Then:                 cmake -S . -B build && cmake --build build

cmake_minimum_required(VERSION 3.20)
project(foundry_local_embeddings_sample CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Locate the locally-built SDK and define the foundry_local_cpp target.
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/FoundryLocalSDK.cmake)

add_executable(embeddings main.cc)
target_link_libraries(embeddings PRIVATE foundry_local_cpp)

# Bake in the rpath so the executable finds the SDK shared library at runtime.
foundry_local_configure_sample(embeddings)
