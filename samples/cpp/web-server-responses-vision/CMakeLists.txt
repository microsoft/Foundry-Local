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

# WinML EP Catalog (from NuGet _native_deps)
set(WINML_DEPS_DIR "${FOUNDRY_SDK_DIR}/_native_deps/Microsoft.Windows.AI.MachineLearning.2.0.300")

add_executable(web-server-responses-vision main.cpp stb_impl.cpp)

target_include_directories(web-server-responses-vision PRIVATE
    ${WINML_DEPS_DIR}/include
)

target_link_libraries(web-server-responses-vision PRIVATE
    CppSdk
    CURL::libcurl
    ${WINML_DEPS_DIR}/lib/native/x64/Microsoft.Windows.AI.MachineLearning.lib
)
