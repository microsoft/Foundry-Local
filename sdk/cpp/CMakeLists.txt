cmake_minimum_required(VERSION 3.20)

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

# -----------------------------
# Dependencies (installed via vcpkg)
# -----------------------------
find_package(nlohmann_json CONFIG REQUIRED)
find_package(wil CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

# -----------------------------
# SDK library (STATIC)
# List ONLY .cpp files here.
# -----------------------------
add_library(CppSdk STATIC
  src/model.cpp
  src/catalog.cpp
  src/openai_chat_client.cpp
  src/openai_audio_client.cpp
  src/foundry_local_manager.cpp
)

target_include_directories(CppSdk
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(CppSdk
  PUBLIC
    nlohmann_json::nlohmann_json
    Microsoft.GSL::GSL
    WIL::WIL
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

# -----------------------------
# End-to-end tests (separate executable, requires Core DLL)
# Exercises the full public API against the real catalog.
# Tests that need model download are DISABLED by default;
# run with --gtest_also_run_disabled_tests locally.
# -----------------------------
add_executable(CppSdkE2ETests
  test/e2e_test.cpp
)

target_include_directories(CppSdkE2ETests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(CppSdkE2ETests
  PRIVATE
    CppSdk
    GTest::gtest_main
)

gtest_discover_tests(CppSdkE2ETests
  WORKING_DIRECTORY $<TARGET_FILE_DIR:CppSdkE2ETests>
)

# Make Visual Studio start/debug this target by default
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  PROPERTY VS_STARTUP_PROJECT CppSdkSample)
