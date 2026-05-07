cmake_minimum_required(VERSION 3.20)

project(VerifyWinMLCpp LANGUAGES CXX)

set(BUILD_TESTING OFF CACHE BOOL "Build C++ SDK tests" FORCE)
set(FOUNDRY_LOCAL_CPP_BUILD_SAMPLE OFF CACHE BOOL "Build the C++ SDK sample executable" FORCE)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../../sdk/cpp" "${CMAKE_CURRENT_BINARY_DIR}/sdk-cpp")

add_executable(VerifyWinML main.cpp)
target_compile_features(VerifyWinML PRIVATE cxx_std_17)
target_link_libraries(VerifyWinML PRIVATE CppSdk)
foundry_local_copy_winml_native_deps(VerifyWinML)
