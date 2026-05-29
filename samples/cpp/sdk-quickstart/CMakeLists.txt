cmake_minimum_required(VERSION 3.20)
project(sdk-quickstart LANGUAGES CXX)

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
    add_subdirectory(${FOUNDRY_SDK_DIR} ${CMAKE_CURRENT_BINARY_DIR}/CppSdk)
    set(FL_SDK_TARGET CppSdk)
else()
    message(STATUS "Using prebuilt FoundryLocal SDK")
    set(FL_SDK_TARGET FoundryLocal::FoundryLocal)
endif()

add_executable(CppSdkSample main.cpp)

target_link_libraries(CppSdkSample PRIVATE ${FL_SDK_TARGET})

# Copy Foundry Local + ONNX Runtime DLLs next to the executable (Windows only).
# The fl_copy_runtime_dlls helper from sdk/cpp/CMakeLists.txt expands FL_*_DLL_DIR
# in the caller's scope; re-derive them here so the post-build commands have real paths.
if(WIN32 AND COMMAND fl_copy_runtime_dlls)
    set(FL_NATIVE_DEPS_DIR "${CMAKE_CURRENT_BINARY_DIR}/CppSdk/_native_deps")
    set(FL_CORE_DLL_DIR    "${FL_NATIVE_DEPS_DIR}/Microsoft.AI.Foundry.Local.Core.${FL_CORE_VERSION}/runtimes/win-x64/native")
    set(FL_ORT_DLL_DIR     "${FL_NATIVE_DEPS_DIR}/Microsoft.ML.OnnxRuntime.Foundry.${FL_ORT_VERSION}/runtimes/win-x64/native")
    set(FL_ORTGENAI_DLL_DIR "${FL_NATIVE_DEPS_DIR}/Microsoft.ML.OnnxRuntimeGenAI.Foundry.${FL_ORTGENAI_VERSION}/runtimes/win-x64/native")
    fl_copy_runtime_dlls(CppSdkSample)
endif()
