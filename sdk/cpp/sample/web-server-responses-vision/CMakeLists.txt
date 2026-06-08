cmake_minimum_required(VERSION 3.20)
project(web-server-responses-vision LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Match the SDK's vcpkg triplet
set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "")

# Try find_package first (prebuilt SDK zip), fall back to add_subdirectory (repo)
find_package(FoundryLocal QUIET)
if(NOT FoundryLocal_FOUND)
    message(STATUS "Prebuilt FoundryLocal not found — building SDK from source")
    set(FOUNDRY_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    add_subdirectory(${FOUNDRY_SDK_DIR} ${CMAKE_CURRENT_BINARY_DIR}/CppSdk)
    set(FL_SDK_TARGET CppSdk)
else()
    message(STATUS "Using prebuilt FoundryLocal SDK")
    set(FL_SDK_TARGET FoundryLocal::FoundryLocal)
endif()

find_package(CURL REQUIRED)

add_executable(web-server-responses-vision main.cpp stb_impl.cpp)

target_link_libraries(web-server-responses-vision PRIVATE
    ${FL_SDK_TARGET}
    CURL::libcurl
)

# Copy runtime DLLs next to the executable
fl_copy_runtime_dlls(web-server-responses-vision)
