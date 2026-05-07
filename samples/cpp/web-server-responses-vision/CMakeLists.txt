cmake_minimum_required(VERSION 3.20)
project(web-server-responses-vision LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Match the SDK's vcpkg triplet
set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "")

# Build the Foundry Local C++ SDK as a subdirectory
set(FOUNDRY_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../sdk/cpp")
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${FOUNDRY_SDK_DIR} ${CMAKE_CURRENT_BINARY_DIR}/CppSdk)

# curl is installed by vcpkg via the SDK's vcpkg.json
find_package(CURL REQUIRED)

add_executable(web-server-responses-vision main.cpp stb_impl.cpp)

target_link_libraries(web-server-responses-vision PRIVATE
    CppSdk
    CURL::libcurl
)
