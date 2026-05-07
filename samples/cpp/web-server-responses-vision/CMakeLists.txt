cmake_minimum_required(VERSION 3.20)
project(web-server-responses-vision LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Match the SDK's vcpkg triplet
set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "")

# ── Auto-download NuGet native dependencies ──────────────────────────────
# These provide the Foundry Local Core DLL and ONNX Runtime DLLs needed at
# runtime. Versions are kept in sync with the SDK's deps_versions.json.
set(CORE_VERSION "1.1.0")
set(ORT_VERSION "1.25.1")
set(ORTGENAI_VERSION "0.13.2")
set(NATIVE_DEPS_DIR "${CMAKE_CURRENT_BINARY_DIR}/_native_deps")

function(ensure_nuget_package PKG_NAME PKG_VERSION)
    set(PKG_DIR "${NATIVE_DEPS_DIR}/${PKG_NAME}.${PKG_VERSION}")
    if(NOT EXISTS "${PKG_DIR}")
        message(STATUS "Downloading ${PKG_NAME} ${PKG_VERSION}...")
        find_program(NUGET_EXE nuget)
        if(NOT NUGET_EXE)
            message(FATAL_ERROR "nuget.exe not found on PATH. Install NuGet CLI: https://www.nuget.org/downloads")
        endif()
        execute_process(
            COMMAND ${NUGET_EXE} install ${PKG_NAME} -Version ${PKG_VERSION}
                    -OutputDirectory ${NATIVE_DEPS_DIR} -NonInteractive
            RESULT_VARIABLE result
        )
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "Failed to download ${PKG_NAME} ${PKG_VERSION}")
        endif()
    endif()
endfunction()

ensure_nuget_package("Microsoft.AI.Foundry.Local.Core" ${CORE_VERSION})
ensure_nuget_package("Microsoft.ML.OnnxRuntime.Foundry" ${ORT_VERSION})
ensure_nuget_package("Microsoft.ML.OnnxRuntimeGenAI.Foundry" ${ORTGENAI_VERSION})

# ── Copy runtime DLLs to build output after build ────────────────────────
set(CORE_DLL_DIR "${NATIVE_DEPS_DIR}/Microsoft.AI.Foundry.Local.Core.${CORE_VERSION}/runtimes/win-x64/native")
set(ORT_DLL_DIR "${NATIVE_DEPS_DIR}/Microsoft.ML.OnnxRuntime.Foundry.${ORT_VERSION}/runtimes/win-x64/native")
set(ORTGENAI_DLL_DIR "${NATIVE_DEPS_DIR}/Microsoft.ML.OnnxRuntimeGenAI.Foundry.${ORTGENAI_VERSION}/runtimes/win-x64/native")

# Build the Foundry Local C++ SDK as a subdirectory
set(FOUNDRY_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../sdk/cpp")
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${FOUNDRY_SDK_DIR} ${CMAKE_CURRENT_BINARY_DIR}/CppSdk)

# curl is installed by vcpkg via the SDK's vcpkg.json
find_package(CURL REQUIRED)

add_executable(web-server-responses-vision main.cpp stb_impl.cpp)

target_link_libraries(web-server-responses-vision PRIVATE
    CppSdk
    CURL::libcurl
)

# Copy runtime DLLs next to the executable after build
add_custom_command(TARGET web-server-responses-vision POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CORE_DLL_DIR}/Microsoft.AI.Foundry.Local.Core.dll"
        $<TARGET_FILE_DIR:web-server-responses-vision>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ORT_DLL_DIR}"
        $<TARGET_FILE_DIR:web-server-responses-vision>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ORTGENAI_DLL_DIR}"
        $<TARGET_FILE_DIR:web-server-responses-vision>
    COMMENT "Copying runtime DLLs..."
)
