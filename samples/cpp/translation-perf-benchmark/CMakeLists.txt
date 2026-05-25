cmake_minimum_required(VERSION 3.20)
project(translation-perf-benchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Match the SDK's vcpkg triplet
set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "")

# --- Optional llama.cpp support ---
# Set LLAMA_CPP_DIR to a llama.cpp install prefix (with include/ and lib/)
# or the llama-cpp-python site-packages directory that ships pre-built binaries.
#   cmake --preset x64-release -DUSE_LLAMA_CPP=ON -DLLAMA_CPP_DIR=C:/path/to/llama
option(USE_LLAMA_CPP "Link against llama.cpp for GGUF benchmark columns" OFF)
set(LLAMA_CPP_DIR "" CACHE PATH "Root of a llama.cpp install (must contain include/ and lib/)")

# Try find_package first (prebuilt SDK zip), fall back to add_subdirectory (repo)
find_package(FoundryLocal QUIET)
if(NOT FoundryLocal_FOUND)
    message(STATUS "Prebuilt FoundryLocal not found — building SDK from source")
    set(FOUNDRY_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../sdk/cpp")
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(FL_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(${FOUNDRY_SDK_DIR} ${CMAKE_CURRENT_BINARY_DIR}/CppSdk)
    set(FL_SDK_TARGET CppSdk)
else()
    message(STATUS "Using prebuilt FoundryLocal SDK")
    set(FL_SDK_TARGET FoundryLocal::FoundryLocal)
endif()

add_executable(translation-perf-benchmark main.cpp)

target_link_libraries(translation-perf-benchmark PRIVATE
    ${FL_SDK_TARGET}
)

# --- llama.cpp linking ---
if(USE_LLAMA_CPP)
    if(NOT LLAMA_CPP_DIR)
        message(FATAL_ERROR "USE_LLAMA_CPP=ON but LLAMA_CPP_DIR is not set. "
                            "Set it to a llama.cpp install prefix with include/ and lib/.")
    endif()
    message(STATUS "llama.cpp enabled: ${LLAMA_CPP_DIR}")

    target_compile_definitions(translation-perf-benchmark PRIVATE HAS_LLAMA_CPP)
    target_include_directories(translation-perf-benchmark PRIVATE "${LLAMA_CPP_DIR}/include")
    target_link_directories(translation-perf-benchmark    PRIVATE "${LLAMA_CPP_DIR}/lib")
    target_link_libraries(translation-perf-benchmark      PRIVATE llama ggml ggml-base ggml-cpu)

    # Copy llama.cpp DLLs next to the executable at build time
    file(GLOB LLAMA_DLLS "${LLAMA_CPP_DIR}/bin/*.dll")
    if(LLAMA_DLLS)
        foreach(DLL ${LLAMA_DLLS})
            add_custom_command(TARGET translation-perf-benchmark POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}"
                        "$<TARGET_FILE_DIR:translation-perf-benchmark>")
        endforeach()
    endif()
endif()

# Ensure MSVC treats source and execution charset as UTF-8
if(MSVC)
    target_compile_options(translation-perf-benchmark PRIVATE /utf-8)
endif()

# Copy Foundry Local + ONNX Runtime DLLs next to the executable (Windows only).
if(COMMAND fl_copy_runtime_dlls)
    fl_copy_runtime_dlls(translation-perf-benchmark)
endif()
