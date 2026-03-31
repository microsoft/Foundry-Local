cmake_minimum_required(VERSION 3.20)

# VS hot reload policy (safe-guarded)
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  if (MSVC)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
      "$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>")
  endif()
endif()

project(CppSdk LANGUAGES CXX)

# -----------------------------
# Windows-only + compiler guard
# -----------------------------
if (NOT WIN32)
  message(FATAL_ERROR "CppSdk is Windows-only for now (uses Win32/WIL headers).")
endif()

# Accept MSVC OR clang-cl (Clang in MSVC compatibility mode).
# VS CMake Open-Folder often uses clang-cl by default.
if (NOT (MSVC OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")))
  message(STATUS "CMAKE_CXX_COMPILER_ID = ${CMAKE_CXX_COMPILER_ID}")
  message(STATUS "CMAKE_CXX_COMPILER    = ${CMAKE_CXX_COMPILER}")
  message(STATUS "CMAKE_CXX_SIMULATE_ID = ${CMAKE_CXX_SIMULATE_ID}")
  message(FATAL_ERROR "Need MSVC or clang-cl (MSVC-compatible toolchain).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optional: target Windows 10+ APIs (adjust if you need older)
add_compile_definitions(_WIN32_WINNT=0x0A00 WINVER=0x0A00)

include(FetchContent)

# -----------------------------
# nlohmann_json (clean CMake target)
# -----------------------------
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

# -----------------------------
# WIL (download headers only; DO NOT run WIL's CMake)
# This avoids NuGet/test requirements and missing wil::wil targets.
# -----------------------------
FetchContent_Declare(
  wil_src
  GIT_REPOSITORY https://github.com/microsoft/wil.git
  GIT_TAG v1.0.250325.1
)
FetchContent_Populate(wil_src)

# -----------------------------
# Microsoft GSL (Guidelines Support Library)
# Provides gsl::span for C++17 (std::span is C++20)
# -----------------------------
FetchContent_Declare(
  gsl
  GIT_REPOSITORY https://github.com/microsoft/GSL.git
  GIT_TAG v4.0.0
)
FetchContent_MakeAvailable(gsl)

# -----------------------------
# Google Test (for unit tests)
# -----------------------------
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# Prevent GoogleTest from overriding our compiler/linker options on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# -----------------------------
# SDK library (STATIC)
# List ONLY .cpp files here.
# -----------------------------
add_library(CppSdk STATIC
  src/foundry_local.cpp
  # Add more .cpp files as you migrate:
  # src/parser.cpp
  # src/dllmain.cpp
  # src/pch.cpp
)

target_include_directories(CppSdk
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${wil_src_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(CppSdk
  PUBLIC
    nlohmann_json::nlohmann_json
    Microsoft.GSL::GSL
)

# -----------------------------
# Sample executable
# -----------------------------
add_executable(CppSdkSample
  sample/main.cpp
)

target_link_libraries(CppSdkSample PRIVATE CppSdk)

# -----------------------------
# Unit tests
# -----------------------------
enable_testing()

add_executable(CppSdkTests
  test/parser_and_types_test.cpp
  test/model_variant_test.cpp
  test/catalog_test.cpp
  test/client_test.cpp
)

target_include_directories(CppSdkTests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(CppSdkTests PRIVATE FL_TESTS)

target_link_libraries(CppSdkTests
  PRIVATE
    CppSdk
    GTest::gtest_main
)

# Copy testdata files next to the test executable so file-based tests can find them.
add_custom_command(TARGET CppSdkTests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/test/testdata
    $<TARGET_FILE_DIR:CppSdkTests>/testdata
)

include(GoogleTest)
gtest_discover_tests(CppSdkTests
  WORKING_DIRECTORY $<TARGET_FILE_DIR:CppSdkTests>
)

# Make Visual Studio start/debug this target by default
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  PROPERTY VS_STARTUP_PROJECT CppSdkSample)
