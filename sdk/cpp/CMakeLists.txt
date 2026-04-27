cmake_minimum_required(VERSION 3.20)

# VS hot reload policy (safe-guarded)
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  if (MSVC)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
      "$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>")
  endif()
endif()

project(CppSdk VERSION 1.1.0 LANGUAGES CXX)

# Full semver string (e.g. "1.1.0-dev.202604241855"). Defaults to PROJECT_VERSION.
# CI passes the full version; local builds just get MAJOR.MINOR.PATCH.
set(CPPSDK_VERSION_STRING "${PROJECT_VERSION}" CACHE STRING "Full semantic version string")

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
option(BUILD_TESTING "Build unit and end-to-end tests" ON)
if (BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
  )
  # Prevent GoogleTest from overriding our compiler/linker options on Windows
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

# -----------------------------
# SDK library (STATIC)
# List ONLY .cpp files here.
# -----------------------------
add_library(CppSdk STATIC
  src/model.cpp
  src/catalog.cpp
  src/openai_chat_client.cpp
  src/openai_audio_client.cpp
  src/openai_live_audio_types.cpp
  src/openai_live_audio_client.cpp
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

target_compile_definitions(CppSdk PUBLIC
  CPPSDK_VERSION="${CPPSDK_VERSION_STRING}"
)

# -----------------------------
# Native dependencies (Core DLL + ONNX Runtime)
# Downloads from NuGet at configure time, copies next to executables at build time.
# Set FOUNDRY_NATIVE_OVERRIDE_DIR to skip download and use local binaries.
# -----------------------------
option(FOUNDRY_DOWNLOAD_NATIVE_DEPS "Download native Foundry Local Core libraries from NuGet" ON)
if (FOUNDRY_DOWNLOAD_NATIVE_DEPS)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FoundryLocalNativeDeps.cmake)
  foundry_local_download_native_deps()
endif()

# -----------------------------
# Sample executable
# -----------------------------
add_executable(CppSdkSample
  sample/main.cpp
)

target_link_libraries(CppSdkSample PRIVATE CppSdk)

if (FOUNDRY_DOWNLOAD_NATIVE_DEPS AND DEFINED FOUNDRY_NATIVE_DIR)
  foundry_local_copy_native_deps(CppSdkSample)
endif()

# -----------------------------
# Unit tests
# -----------------------------
if (BUILD_TESTING)
  enable_testing()

  add_executable(CppSdkTests
    test/parser_and_types_test.cpp
    test/model_variant_test.cpp
    test/catalog_test.cpp
    test/client_test.cpp
    test/live_audio_test.cpp
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
      gtest_main
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
      gtest_main
  )

  gtest_discover_tests(CppSdkE2ETests
    WORKING_DIRECTORY $<TARGET_FILE_DIR:CppSdkE2ETests>
  )

  # Copy native libs next to E2E test binary so Core DLL is found at runtime
  if (FOUNDRY_DOWNLOAD_NATIVE_DEPS AND DEFINED FOUNDRY_NATIVE_DIR)
    foundry_local_copy_native_deps(CppSdkE2ETests)
  endif()
endif()

# -----------------------------
# Install rules
# Produces a redistributable tree:
#   include/     – public headers
#   lib/         – static library
#   lib/cmake/   – CMake package config for find_package(CppSdk)
# -----------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS CppSdk
  EXPORT CppSdkTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT CppSdkTargets
  FILE CppSdkTargets.cmake
  NAMESPACE foundry_local::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CppSdk
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CppSdkConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/CppSdkConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CppSdk
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/CppSdkConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/CppSdkConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/CppSdkConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CppSdk
)

# Make Visual Studio start/debug this target by default
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  PROPERTY VS_STARTUP_PROJECT CppSdkSample)
