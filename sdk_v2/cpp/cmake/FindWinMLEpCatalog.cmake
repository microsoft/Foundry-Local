# Copyright (c) Microsoft. All rights reserved.
# Find/acquire the WinML EP Catalog C API from Microsoft.Windows.AI.MachineLearning.
#
# Downloads the NuGet package if needed, then defers to the package's first-party
# CMake config (build/cmake/microsoft.windows.ai.machinelearning-config.cmake) for
# target discovery. The config defines WindowsML::Api (EP catalog) and
# WindowsML::OnnxRuntime; we link only WindowsML::Api here so that ORT remains
# sourced from FindOnnxRuntime.cmake (Microsoft.ML.OnnxRuntime, same version for
# WinML and non-WinML builds).
#
# Reg-free runtime: WinML 2.x (Microsoft.Windows.AI.MachineLearning) does not
# require the Windows App SDK runtime bootstrap. The package ships a self-contained
# native DLL that can be loaded directly from any unpackaged app on Windows 10
# 19H1 (build 18362) or newer. No MddBootstrapInitialize2 plumbing is needed.
#
# Re-exports an ALIAS target: WinMLEpCatalog::WinMLEpCatalog -> WindowsML::Api
# Sets: WINML_EP_CATALOG_DLL_DIR (= WINML_BINARY_DIR)

if(WinMLEpCatalog_FOUND)
    return()
endif()

# EP catalog is Windows-only
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(WinMLEpCatalog_FOUND FALSE)
    message(STATUS "WinML EP Catalog: skipped (not Windows)")
    return()
endif()

# Latest GA Microsoft.Windows.AI.MachineLearning on nuget.org. Bump as new GA
# releases ship; the WinMLEpCatalog.h C ABI is stable across 2.0.x and 2.1.x.
set(_WINML_EP_CATALOG_MIN_VERSION "2.1.70")

if(NOT WINML_EP_CATALOG_VERSION)
    set(WINML_EP_CATALOG_VERSION "${_WINML_EP_CATALOG_MIN_VERSION}")
endif()

# The package's own CMake config FATAL_ERRORs on architectures it doesn't ship
# binaries for (anything other than x64/ARM64). Pre-check so we degrade to a soft
# disable instead of halting configuration when someone builds e.g. ARM64EC with
# FOUNDRY_LOCAL_USE_WINML=ON.
if(CMAKE_GENERATOR_PLATFORM)
    string(TOUPPER "${CMAKE_GENERATOR_PLATFORM}" _WINML_PLATFORM_UPPER)
elseif(CMAKE_VS_PLATFORM_NAME)
    string(TOUPPER "${CMAKE_VS_PLATFORM_NAME}" _WINML_PLATFORM_UPPER)
elseif(CMAKE_SYSTEM_PROCESSOR)
    string(TOUPPER "${CMAKE_SYSTEM_PROCESSOR}" _WINML_PLATFORM_UPPER)
else()
    set(_WINML_PLATFORM_UPPER "X64")
endif()

if(NOT _WINML_PLATFORM_UPPER MATCHES "^(AMD64|X64|X86_64|ARM64|AARCH64)$")
    message(WARNING "WinML EP Catalog: unsupported architecture '${_WINML_PLATFORM_UPPER}'. "
                    "Package ships x64/ARM64 only. EP detection will be disabled.")
    set(WinMLEpCatalog_FOUND FALSE)
    return()
endif()

include(cmake/nuget.cmake)

# WINML_EP_CATALOG_FETCH_URL can be set externally (e.g. for CI where nuget.org is blocked).
set(WINML_EP_CATALOG_FETCH_URL "" CACHE STRING "Override URL or local path for the WinML EP Catalog NuGet package")

if(WINML_EP_CATALOG_FETCH_URL)
    # Use FetchContent to download/extract the pre-downloaded package
    include(FetchContent)
    string(REPLACE "\\" "/" WINML_EP_CATALOG_FETCH_URL "${WINML_EP_CATALOG_FETCH_URL}")
    if(WINML_EP_CATALOG_FETCH_URL MATCHES "\\.nupkg$" AND NOT WINML_EP_CATALOG_FETCH_URL MATCHES "^https?://")
        set(_WINML_ZIP_PATH "${CMAKE_BINARY_DIR}/_deps/winml_ep_catalog-download/winml_ep_catalog.zip")
        get_filename_component(_WINML_ZIP_DIR "${_WINML_ZIP_PATH}" DIRECTORY)
        file(MAKE_DIRECTORY "${_WINML_ZIP_DIR}")
        file(COPY_FILE "${WINML_EP_CATALOG_FETCH_URL}" "${_WINML_ZIP_PATH}")
        set(WINML_EP_CATALOG_FETCH_URL "${_WINML_ZIP_PATH}")
    endif()
    FetchContent_Declare(winml_ep_catalog URL ${WINML_EP_CATALOG_FETCH_URL} DOWNLOAD_EXTRACT_TIMESTAMP TRUE DOWNLOAD_NAME winml_ep_catalog.zip)
    FetchContent_MakeAvailable(winml_ep_catalog)
    set(_WINML_EP_ROOT "${winml_ep_catalog_SOURCE_DIR}")
    message(STATUS "WinML EP Catalog via FetchContent: ${_WINML_EP_ROOT}")
else()
    install_nuget_package(Microsoft.Windows.AI.MachineLearning ${WINML_EP_CATALOG_VERSION} _WINML_EP_ROOT
        SOURCE https://api.nuget.org/v3/index.json)
endif()

# Defer to the package's first-party CMake config for target discovery and layout
# resolution. The config lives at build/cmake/<lowercased-package>-config.cmake
# and defines WindowsML::Api / WindowsML::OnnxRuntime / WindowsML::DirectML.
set(_WINML_EP_CONFIG_DIR "${_WINML_EP_ROOT}/build/cmake")
if(NOT EXISTS "${_WINML_EP_CONFIG_DIR}/microsoft.windows.ai.machinelearning-config.cmake")
    message(WARNING "WinML EP Catalog: package CMake config not found at ${_WINML_EP_CONFIG_DIR}. "
                    "Package version may be too old or layout has changed. EP detection will be disabled.")
    set(WinMLEpCatalog_FOUND FALSE)
    return()
endif()

set(microsoft.windows.ai.machinelearning_DIR "${_WINML_EP_CONFIG_DIR}" CACHE PATH
    "Path to the Microsoft.Windows.AI.MachineLearning CMake config" FORCE)
find_package(microsoft.windows.ai.machinelearning CONFIG REQUIRED)

if(NOT TARGET WindowsML::Api)
    message(WARNING "WinML EP Catalog: WindowsML::Api target not defined after find_package. "
                    "EP detection will be disabled.")
    set(WinMLEpCatalog_FOUND FALSE)
    return()
endif()

# Re-export under our existing name so consumers don't need to change. Promote
# WindowsML::Api to GLOBAL so the alias is visible in any subdirectory that may
# link foundry_local transitively.
set_target_properties(WindowsML::Api PROPERTIES IMPORTED_GLOBAL TRUE)
add_library(WinMLEpCatalog::WinMLEpCatalog ALIAS WindowsML::Api)

# Export the binary dir set by the official config (WINML_BINARY_DIR) under
# our existing variable name for the post-build DLL-copy step. The header dir
# is not re-exported; consumers get include paths via the WinMLEpCatalog::WinMLEpCatalog
# target's INTERFACE_INCLUDE_DIRECTORIES.
set(WINML_EP_CATALOG_DLL_DIR "${WINML_BINARY_DIR}" CACHE PATH "WinML EP Catalog native DLL directory" FORCE)

set(WinMLEpCatalog_FOUND TRUE)
message(STATUS "WinML EP Catalog: ${_WINML_EP_ROOT}")
message(STATUS "  Target: WinMLEpCatalog::WinMLEpCatalog -> WindowsML::Api")
message(STATUS "  DLL dir: ${WINML_EP_CATALOG_DLL_DIR}")
