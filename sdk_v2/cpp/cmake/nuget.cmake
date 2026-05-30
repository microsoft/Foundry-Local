# Copyright (c) Microsoft. All rights reserved.
# NuGet package download helper.
# Adapted from ort.genai/cmake/nuget.cmake.

include_guard()

#[[====================================================================================================================
    install_nuget_package
    ---------------------
    Downloads a NuGet package and returns the installation path.

        install_nuget_package(
            <package name>
            <package version>
            <output variable>       # set to the extracted package directory
            [SOURCE <feed url>]     # optional NuGet feed URL (-Source)
        )

    Requires nuget.exe to be on PATH.
====================================================================================================================]]#
function(install_nuget_package NUGET_PACKAGE_NAME NUGET_PACKAGE_VERSION NUGET_PACKAGE_PATH_PROPERTY)
    cmake_parse_arguments(ARG "" "SOURCE" "" ${ARGN})

    if(NOT NUGET_PACKAGE_ROOT_PATH)
        set(NUGET_PACKAGE_ROOT_PATH ${CMAKE_BINARY_DIR}/__nuget)
    endif()

    set(NUGET_PACKAGE_PATH "${NUGET_PACKAGE_ROOT_PATH}/${NUGET_PACKAGE_NAME}.${NUGET_PACKAGE_VERSION}")

    if(NOT EXISTS "${NUGET_PACKAGE_PATH}")
        find_program(NUGET_PATH NAMES nuget nuget.exe)
        if(NUGET_PATH STREQUAL "NUGET_PATH-NOTFOUND")
            message(FATAL_ERROR "nuget.exe not found on PATH. Install from https://www.nuget.org/downloads")
        endif()

        set(NUGET_COMMAND ${NUGET_PATH} install ${NUGET_PACKAGE_NAME})
        list(APPEND NUGET_COMMAND -OutputDirectory ${NUGET_PACKAGE_ROOT_PATH})
        list(APPEND NUGET_COMMAND -Version ${NUGET_PACKAGE_VERSION})
        list(APPEND NUGET_COMMAND -PackageSaveMode nuspec)

        if(ARG_SOURCE)
            list(APPEND NUGET_COMMAND -Source ${ARG_SOURCE})
        endif()

        message(STATUS "Downloading NuGet package: ${NUGET_PACKAGE_NAME} ${NUGET_PACKAGE_VERSION}")

        execute_process(
            COMMAND ${NUGET_COMMAND}
            OUTPUT_VARIABLE NUGET_OUTPUT
            ERROR_VARIABLE NUGET_ERROR
            RESULT_VARIABLE NUGET_RESULT
        )

        if(NOT (NUGET_RESULT STREQUAL 0))
            message(FATAL_ERROR "NuGet install failed: ${NUGET_ERROR}")
        endif()
    endif()

    set(${NUGET_PACKAGE_PATH_PROPERTY} "${NUGET_PACKAGE_PATH}" PARENT_SCOPE)
endfunction()
