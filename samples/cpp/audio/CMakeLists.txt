# Copyright (c) Microsoft. All rights reserved.
#
# Standalone build for the Foundry Local C++ audio transcription sample.
# Build the SDK first:  python ../../../sdk_v2/cpp/build.py
# Then:                 cmake -S . -B build && cmake --build build
#
# PortAudio is OPTIONAL: when found, live microphone capture is enabled
# (HAS_PORTAUDIO). Without it, the sample still builds and transcribes files /
# synthetic audio.

cmake_minimum_required(VERSION 3.20)
project(foundry_local_audio_sample CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Locate the locally-built SDK and define the foundry_local_cpp target.
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/FoundryLocalSDK.cmake)

add_executable(audio main.cc)

target_link_libraries(audio PRIVATE foundry_local_cpp)

# Absolute path to this sample dir so the bundled Recording.wav is found
# regardless of where the executable runs from.
target_compile_definitions(audio PRIVATE SAMPLE_SOURCE_DIR="${CMAKE_CURRENT_LIST_DIR}")

# --- Optional PortAudio for live microphone capture -------------------------
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(PORTAUDIO QUIET portaudio-2.0)
endif()

find_path(PORTAUDIO_INCLUDE_DIR portaudio.h
    HINTS ${PORTAUDIO_INCLUDE_DIRS} /opt/homebrew/include /usr/local/include)
find_library(PORTAUDIO_LIBRARY NAMES portaudio
    HINTS ${PORTAUDIO_LIBRARY_DIRS} /opt/homebrew/lib /usr/local/lib)

if(PORTAUDIO_INCLUDE_DIR AND PORTAUDIO_LIBRARY)
  message(STATUS "PortAudio found — live microphone capture enabled")
  target_compile_definitions(audio PRIVATE HAS_PORTAUDIO)
  target_include_directories(audio PRIVATE ${PORTAUDIO_INCLUDE_DIR})
  target_link_libraries(audio PRIVATE ${PORTAUDIO_LIBRARY})
else()
  message(STATUS "PortAudio not found — building file/synthetic-only (no live mic)")
endif()

# Bake in the rpath so the executable finds the SDK shared library at runtime.
foundry_local_configure_sample(audio)
