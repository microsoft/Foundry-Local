cmake_minimum_required(VERSION 3.20)
project(translation-benchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Match the SDK's vcpkg triplet
set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "")

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

add_executable(translation-benchmark main.cpp)

target_link_libraries(translation-benchmark PRIVATE
    ${FL_SDK_TARGET}
)

# Ensure MSVC treats source and execution charset as UTF-8
if(MSVC)
    target_compile_options(translation-benchmark PRIVATE /utf-8)
endif()

# Copy Foundry Local + ONNX Runtime DLLs next to the executable (Windows only).
if(COMMAND fl_copy_runtime_dlls)
    fl_copy_runtime_dlls(translation-benchmark)
endif()
