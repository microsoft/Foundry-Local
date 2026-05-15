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

# Latest stable Microsoft.WindowsAppSDK.ML 1.8.x on nuget.org. Anything older
# than 1.8.2141 silently disables EP detection (no WinMLEpCatalog.h).
set(_WINML_EP_CATALOG_MIN_VERSION "1.8.2192")

# WINML_EP_CATALOG_VERSION may be set explicitly; otherwise pick the minimum
# known-good version. We deliberately do NOT inherit WINML_SDK_VERSION here:
# the WinML SDK and the EP catalog package have independent compatibility
# requirements (the EP catalog ships only in newer WindowsAppSDK.ML packages,
# and our build no longer uses the WinML-bundled ORT regardless).
if(NOT WINML_EP_CATALOG_VERSION)
    set(WINML_EP_CATALOG_VERSION "${_WINML_EP_CATALOG_MIN_VERSION}")
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

# --------------------------------------------------------------------------
# WinAppSDK Bootstrap (MddBootstrapInitialize2)
# --------------------------------------------------------------------------
# The bootstrap C ABI ships in Microsoft.WindowsAppSDK.Foundation, which is pulled in
# transitively as a dependency of Microsoft.WindowsAppSDK.ML when nuget install resolves
# dependencies. Locate the highest-version Foundation directory under the same NuGet root
# and expose:
#   - WinAppSdkBootstrap::WinAppSdkBootstrap (IMPORTED SHARED) — link to get
#     Microsoft.WindowsAppRuntime.Bootstrap.lib + the MddBootstrap.h include path.
#   - WINAPPSDK_BOOTSTRAP_DLL — full path to Microsoft.WindowsAppRuntime.Bootstrap.dll
#     for post-build co-location next to the consuming binary.
#
# When WINML_EP_CATALOG_FETCH_URL is set (CI offline path), only the .ML zip is fetched —
# Foundation isn't pulled transitively, so we skip bootstrap setup. WinML builds in that
# configuration cannot use the bootstrap; consumers must already have the WinAppSDK runtime
# installed system-wide.
if(NOT WINML_EP_CATALOG_FETCH_URL)
    if(NOT NUGET_PACKAGE_ROOT_PATH)
        set(NUGET_PACKAGE_ROOT_PATH "${CMAKE_BINARY_DIR}/__nuget")
    endif()
    file(GLOB _WINAPPSDK_FOUNDATION_DIRS LIST_DIRECTORIES TRUE
        "${NUGET_PACKAGE_ROOT_PATH}/Microsoft.WindowsAppSDK.Foundation.*")
    if(_WINAPPSDK_FOUNDATION_DIRS)
        list(SORT _WINAPPSDK_FOUNDATION_DIRS COMPARE NATURAL ORDER DESCENDING)
        list(GET _WINAPPSDK_FOUNDATION_DIRS 0 _WINAPPSDK_FOUNDATION_ROOT)

        set(_WINAPPSDK_BOOTSTRAP_HEADER_DIR "${_WINAPPSDK_FOUNDATION_ROOT}/include")
        set(_WINAPPSDK_BOOTSTRAP_LIB
            "${_WINAPPSDK_FOUNDATION_ROOT}/lib/native/${_WINML_EP_ARCH}/Microsoft.WindowsAppRuntime.Bootstrap.lib")
        set(_WINAPPSDK_BOOTSTRAP_DLL
            "${_WINAPPSDK_FOUNDATION_ROOT}/runtimes/${_WINML_EP_RUNTIME_PLATFORM}/native/Microsoft.WindowsAppRuntime.Bootstrap.dll")

        if(EXISTS "${_WINAPPSDK_BOOTSTRAP_HEADER_DIR}/MddBootstrap.h" AND EXISTS "${_WINAPPSDK_BOOTSTRAP_LIB}")
            add_library(WinAppSdkBootstrap::WinAppSdkBootstrap SHARED IMPORTED)
            set_target_properties(WinAppSdkBootstrap::WinAppSdkBootstrap PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${_WINAPPSDK_BOOTSTRAP_HEADER_DIR}"
                IMPORTED_IMPLIB "${_WINAPPSDK_BOOTSTRAP_LIB}"
            )
            if(EXISTS "${_WINAPPSDK_BOOTSTRAP_DLL}")
                set_target_properties(WinAppSdkBootstrap::WinAppSdkBootstrap PROPERTIES
                    IMPORTED_LOCATION "${_WINAPPSDK_BOOTSTRAP_DLL}"
                )
                set(WINAPPSDK_BOOTSTRAP_DLL "${_WINAPPSDK_BOOTSTRAP_DLL}"
                    CACHE FILEPATH "Microsoft.WindowsAppRuntime.Bootstrap.dll for post-build copy" FORCE)
            endif()
            set(WinAppSdkBootstrap_FOUND TRUE)
            message(STATUS "WinAppSDK Bootstrap: ${_WINAPPSDK_FOUNDATION_ROOT}")
            message(STATUS "  Import lib: ${_WINAPPSDK_BOOTSTRAP_LIB}")
            message(STATUS "  DLL: ${_WINAPPSDK_BOOTSTRAP_DLL}")
        else()
            message(WARNING "WinAppSDK Bootstrap: header or import lib missing under "
                            "${_WINAPPSDK_FOUNDATION_ROOT}. Bootstrap.Initialize() will be a no-op.")
        endif()
    else()
        message(WARNING "WinAppSDK Bootstrap: Microsoft.WindowsAppSDK.Foundation NuGet not found "
                        "under ${NUGET_PACKAGE_ROOT_PATH}. Bootstrap.Initialize() will be a no-op.")
    endif()
endif()
