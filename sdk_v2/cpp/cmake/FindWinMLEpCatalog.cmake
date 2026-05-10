# Copyright (c) Microsoft. All rights reserved.
# Find/acquire the WinML EP Catalog C API from Microsoft.WindowsAppSDK.ML.
#
# Downloads the NuGet package if needed, then creates an IMPORTED target for
# the EP catalog library (Microsoft.Windows.AI.MachineLearning.dll/.lib).
#
# This is separate from ORT — we use the EP catalog to discover and download
# hardware-specific execution providers at runtime. ORT itself comes from
# FindOnnxRuntime.cmake (either WinML SDK or ORT-Nightly feed).
#
# Creates an IMPORTED target: WinMLEpCatalog::WinMLEpCatalog
# Sets: WINML_EP_CATALOG_HEADER_DIR, WINML_EP_CATALOG_DLL_DIR

if(WinMLEpCatalog_FOUND)
    return()
endif()

# EP catalog is Windows-only
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(WinMLEpCatalog_FOUND FALSE)
    message(STATUS "WinML EP Catalog: skipped (not Windows)")
    return()
endif()

# Use the same WinML SDK version as FindOnnxRuntime.cmake when USE_WINML=ON,
# or a default version for non-WinML builds.
if(NOT WINML_EP_CATALOG_VERSION)
    if(WINML_SDK_VERSION AND NOT WINML_SDK_VERSION STREQUAL "")
        set(WINML_EP_CATALOG_VERSION "${WINML_SDK_VERSION}")
    else()
        set(WINML_EP_CATALOG_VERSION "1.8.2091")
    endif()
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
    install_nuget_package(Microsoft.WindowsAppSDK.ML ${WINML_EP_CATALOG_VERSION} _WINML_EP_ROOT
        SOURCE https://api.nuget.org/v3/index.json)
endif()

# Determine platform for lib/native path
if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64" OR CMAKE_GENERATOR_PLATFORM STREQUAL "arm64")
    set(_WINML_EP_ARCH "arm64")
    set(_WINML_EP_RUNTIME_PLATFORM "win-arm64")
elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64EC" OR CMAKE_GENERATOR_PLATFORM STREQUAL "arm64EC")
    set(_WINML_EP_ARCH "arm64ec")
    set(_WINML_EP_RUNTIME_PLATFORM "win-arm64ec")
else()
    set(_WINML_EP_ARCH "x64")
    set(_WINML_EP_RUNTIME_PLATFORM "win-x64")
endif()

set(_WINML_EP_HEADER_DIR "${_WINML_EP_ROOT}/include")
set(_WINML_EP_LIB_DIR "${_WINML_EP_ROOT}/lib/native/${_WINML_EP_ARCH}")
set(_WINML_EP_DLL_DIR "${_WINML_EP_ROOT}/runtimes-framework/${_WINML_EP_RUNTIME_PLATFORM}/native")

# Validate headers
if(NOT EXISTS "${_WINML_EP_HEADER_DIR}/WinMLEpCatalog.h")
    message(WARNING "WinML EP Catalog: WinMLEpCatalog.h not found at ${_WINML_EP_HEADER_DIR}. "
                    "EP detection will be disabled. Package version may be too old (need ≥1.8.2141).")
    set(WinMLEpCatalog_FOUND FALSE)
    return()
endif()

# Validate import lib
set(_WINML_EP_IMPORT_LIB "${_WINML_EP_LIB_DIR}/Microsoft.Windows.AI.MachineLearning.lib")
if(NOT EXISTS "${_WINML_EP_IMPORT_LIB}")
    message(WARNING "WinML EP Catalog: import lib not found at ${_WINML_EP_IMPORT_LIB}. "
                    "EP detection will be disabled.")
    set(WinMLEpCatalog_FOUND FALSE)
    return()
endif()

# Create imported target
add_library(WinMLEpCatalog::WinMLEpCatalog SHARED IMPORTED)
set_target_properties(WinMLEpCatalog::WinMLEpCatalog PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_WINML_EP_HEADER_DIR}"
    IMPORTED_IMPLIB "${_WINML_EP_IMPORT_LIB}"
)

# Set the DLL location if it exists (for post-build copy)
set(_WINML_EP_DLL "${_WINML_EP_DLL_DIR}/Microsoft.Windows.AI.MachineLearning.dll")
if(EXISTS "${_WINML_EP_DLL}")
    set_target_properties(WinMLEpCatalog::WinMLEpCatalog PROPERTIES
        IMPORTED_LOCATION "${_WINML_EP_DLL}"
    )
endif()

# Export paths for downstream use
set(WINML_EP_CATALOG_HEADER_DIR "${_WINML_EP_HEADER_DIR}" CACHE PATH "WinML EP Catalog include directory" FORCE)
set(WINML_EP_CATALOG_DLL_DIR "${_WINML_EP_DLL_DIR}" CACHE PATH "WinML EP Catalog native DLL directory" FORCE)

set(WinMLEpCatalog_FOUND TRUE)
message(STATUS "WinML EP Catalog: ${_WINML_EP_ROOT}")
message(STATUS "  Headers: ${_WINML_EP_HEADER_DIR}")
message(STATUS "  Import lib: ${_WINML_EP_IMPORT_LIB}")
message(STATUS "  DLL dir: ${_WINML_EP_DLL_DIR}")
