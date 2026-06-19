# Copyright (c) Microsoft. All rights reserved.
#
# FoundryLocalSDK.cmake — wires a sample against a *locally built* copy of the
# Foundry Local C++ SDK (sdk_v2/cpp).
#
# The SDK does not install/export a CMake package, and re-building it via
# add_subdirectory() would require the full vcpkg toolchain. Instead, after a
# user runs `python sdk_v2/cpp/build.py`, this module references that build tree
# directly: it picks up the public headers from sdk_v2/cpp/include, the bundled
# third-party headers vcpkg produced (gsl/span — required by the C++ wrapper —
# and nlohmann/json, used by the web-service samples), and the built shared
# library. It then defines an INTERFACE target `foundry_local_cpp` so each
# sample links it exactly like the in-tree SDK examples do.
#
# Override points (cache variables):
#   FOUNDRY_LOCAL_SDK_DIR       Path to sdk_v2/cpp        (default: repo layout)
#   FOUNDRY_LOCAL_BUILD_CONFIG  SDK build config          (default: RelWithDebInfo)
#   FOUNDRY_LOCAL_BUILD_DIR     SDK build output dir      (default: derived)

if(TARGET foundry_local_cpp)
  return()
endif()

# --- Locate the SDK source tree ---------------------------------------------
get_filename_component(_fl_default_sdk_dir "${CMAKE_CURRENT_LIST_DIR}/../../../sdk_v2/cpp" ABSOLUTE)
set(FOUNDRY_LOCAL_SDK_DIR "${_fl_default_sdk_dir}" CACHE PATH "Path to the sdk_v2/cpp source tree")

set(_fl_include_dir "${FOUNDRY_LOCAL_SDK_DIR}/include")
if(NOT EXISTS "${_fl_include_dir}/foundry_local/foundry_local_cpp.h")
  message(FATAL_ERROR
    "Foundry Local public header not found under '${_fl_include_dir}'.\n"
    "Set -DFOUNDRY_LOCAL_SDK_DIR=<path-to>/sdk_v2/cpp.")
endif()

# --- Derive the build output directory (mirrors build.py's layout) ----------
# build.py writes to build/<Windows|Linux|macOS>/<Config>.
if(WIN32)
  set(_fl_platform "Windows")
elseif(APPLE)
  set(_fl_platform "macOS")
else()
  set(_fl_platform "Linux")
endif()

set(FOUNDRY_LOCAL_BUILD_CONFIG "RelWithDebInfo"
    CACHE STRING "SDK build configuration produced by build.py (Debug/Release/RelWithDebInfo/MinSizeRel)")
set(FOUNDRY_LOCAL_BUILD_DIR "${FOUNDRY_LOCAL_SDK_DIR}/build/${_fl_platform}/${FOUNDRY_LOCAL_BUILD_CONFIG}"
    CACHE PATH "SDK build output directory")

if(NOT EXISTS "${FOUNDRY_LOCAL_BUILD_DIR}")
  message(FATAL_ERROR
    "SDK build directory '${FOUNDRY_LOCAL_BUILD_DIR}' does not exist.\n"
    "Build the SDK first:  python ${FOUNDRY_LOCAL_SDK_DIR}/build.py --config ${FOUNDRY_LOCAL_BUILD_CONFIG}\n"
    "Or point -DFOUNDRY_LOCAL_BUILD_DIR=<your build dir>.")
endif()

# --- Bundled third-party headers (gsl, nlohmann/json) -----------------------
# The C++ wrapper includes <gsl/span>, so every TU that includes it needs the
# GSL headers. vcpkg dropped them under build/.../vcpkg_installed/<triplet>/include.
file(GLOB _fl_vcpkg_includes "${FOUNDRY_LOCAL_BUILD_DIR}/vcpkg_installed/*/include")
set(_fl_thirdparty_include "")
foreach(_inc ${_fl_vcpkg_includes})
  if(EXISTS "${_inc}/gsl/span")
    set(_fl_thirdparty_include "${_inc}")
    break()
  endif()
endforeach()

if(_fl_thirdparty_include STREQUAL "")
  message(FATAL_ERROR
    "Could not find the bundled GSL headers (gsl/span) under "
    "'${FOUNDRY_LOCAL_BUILD_DIR}/vcpkg_installed/*/include'.\n"
    "Re-run the SDK build:  python ${FOUNDRY_LOCAL_SDK_DIR}/build.py")
endif()

# --- Locate the shared library ----------------------------------------------
# Unix single-config: build/.../bin. Windows multi-config: build/.../bin/<Config>.
set(_fl_bin_candidates
    "${FOUNDRY_LOCAL_BUILD_DIR}/bin"
    "${FOUNDRY_LOCAL_BUILD_DIR}/bin/${FOUNDRY_LOCAL_BUILD_CONFIG}")

find_library(FOUNDRY_LOCAL_LINK_LIB
    NAMES foundry_local
    PATHS ${_fl_bin_candidates} "${FOUNDRY_LOCAL_BUILD_DIR}/${FOUNDRY_LOCAL_BUILD_CONFIG}"
    NO_DEFAULT_PATH)

if(NOT FOUNDRY_LOCAL_LINK_LIB)
  message(FATAL_ERROR
    "Could not find the foundry_local library under '${FOUNDRY_LOCAL_BUILD_DIR}'.\n"
    "Build the SDK first:  python ${FOUNDRY_LOCAL_SDK_DIR}/build.py --config ${FOUNDRY_LOCAL_BUILD_CONFIG}")
endif()

# Runtime directory that holds the shared library + co-located ORT/GenAI libs.
# On Windows the import .lib may sit elsewhere, so locate the .dll explicitly.
if(WIN32)
  find_file(FOUNDRY_LOCAL_DLL
      NAMES foundry_local.dll
      PATHS ${_fl_bin_candidates}
      NO_DEFAULT_PATH)
  if(FOUNDRY_LOCAL_DLL)
    get_filename_component(FOUNDRY_LOCAL_BIN_DIR "${FOUNDRY_LOCAL_DLL}" DIRECTORY)
  else()
    set(FOUNDRY_LOCAL_BIN_DIR "${FOUNDRY_LOCAL_BUILD_DIR}/bin/${FOUNDRY_LOCAL_BUILD_CONFIG}")
  endif()
else()
  get_filename_component(FOUNDRY_LOCAL_BIN_DIR "${FOUNDRY_LOCAL_LINK_LIB}" DIRECTORY)
endif()

# --- The consumable INTERFACE target ----------------------------------------
add_library(foundry_local_cpp INTERFACE)
target_include_directories(foundry_local_cpp INTERFACE
    "${_fl_include_dir}"
    "${_fl_thirdparty_include}")
target_link_libraries(foundry_local_cpp INTERFACE "${FOUNDRY_LOCAL_LINK_LIB}")
target_compile_features(foundry_local_cpp INTERFACE cxx_std_20)

message(STATUS "Foundry Local SDK library: ${FOUNDRY_LOCAL_LINK_LIB}")
message(STATUS "Foundry Local SDK headers: ${_fl_include_dir}")

# --- Per-target finalizer: make the executable find the shared lib at runtime.
# The shared library bakes in @loader_path/$ORIGIN, so co-located ORT/GenAI libs
# resolve automatically once the executable can find libfoundry_local itself.
function(foundry_local_configure_sample _target)
  if(APPLE)
    set_target_properties(${_target} PROPERTIES BUILD_RPATH "${FOUNDRY_LOCAL_BIN_DIR}")
  elseif(UNIX)
    # --disable-new-dtags forces RPATH (not RUNPATH) so it propagates to GenAI's
    # internal dlopen("libonnxruntime.so") — same treatment as the SDK examples.
    set_target_properties(${_target} PROPERTIES BUILD_RPATH "${FOUNDRY_LOCAL_BIN_DIR}")
    target_link_options(${_target} PRIVATE -Wl,--disable-new-dtags)
  elseif(WIN32)
    # Windows has no rpath: copy every runtime DLL next to the executable.
    file(GLOB _fl_runtime_dlls "${FOUNDRY_LOCAL_BIN_DIR}/*.dll")
    foreach(_dll ${_fl_runtime_dlls})
      add_custom_command(TARGET ${_target} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "$<TARGET_FILE_DIR:${_target}>")
    endforeach()
  endif()
endfunction()
