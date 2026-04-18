# ── Fetch native dependencies from NuGet ─────────────────────────────────────
# Downloads FLCore, OnnxRuntime, and OnnxRuntimeGenAI NuGet packages
# and extracts the platform-specific native DLLs.
#
# Usage: include(FetchNativeDeps) from your CMakeLists.txt
#        Then link or copy NATIVE_DEPS_DLLS to your output directory.
# ─────────────────────────────────────────────────────────────────────────────

include(FetchContent)

# Read dependency versions from deps_versions.json
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../deps_versions.json" DEPS_JSON)

string(JSON FLCORE_VERSION GET ${DEPS_JSON} "foundry-local-core" "nuget")
string(JSON ORT_VERSION GET ${DEPS_JSON} "onnxruntime" "version")
string(JSON ORTGENAI_VERSION GET ${DEPS_JSON} "onnxruntime-genai" "version")

message(STATUS "Native deps: FLCore=${FLCORE_VERSION}  ORT=${ORT_VERSION}  ORT-GenAI=${ORTGENAI_VERSION}")

# Determine platform RID
if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
  set(NUGET_RID "win-x64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
  set(NUGET_RID "win-arm64")
else()
  message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(NATIVE_DEPS_DIR "${CMAKE_BINARY_DIR}/_native_deps")
file(MAKE_DIRECTORY "${NATIVE_DEPS_DIR}")

# ── Helper: download and extract a NuGet package ────────────────────────────
function(fetch_nuget_package PKG_NAME PKG_VERSION FEED_URL OUT_DLLS)
  string(TOLOWER "${PKG_NAME}" PKG_LOWER)
  string(TOLOWER "${PKG_VERSION}" VER_LOWER)
  set(NUPKG_URL "${FEED_URL}/${PKG_LOWER}/${VER_LOWER}/${PKG_LOWER}.${VER_LOWER}.nupkg")
  set(NUPKG_FILE "${NATIVE_DEPS_DIR}/${PKG_LOWER}.${VER_LOWER}.nupkg")
  set(EXTRACT_DIR "${NATIVE_DEPS_DIR}/${PKG_LOWER}")

  if(NOT EXISTS "${EXTRACT_DIR}/extracted.stamp")
    message(STATUS "Downloading ${PKG_NAME} ${PKG_VERSION}...")
    file(DOWNLOAD "${NUPKG_URL}" "${NUPKG_FILE}"
         STATUS DL_STATUS
         TLS_VERIFY ON
         SHOW_PROGRESS)
    list(GET DL_STATUS 0 DL_CODE)
    if(NOT DL_CODE EQUAL 0)
      list(GET DL_STATUS 1 DL_MSG)
      message(FATAL_ERROR "Failed to download ${PKG_NAME}: ${DL_MSG}\n  URL: ${NUPKG_URL}")
    endif()

    # nupkg is a ZIP file
    file(ARCHIVE_EXTRACT INPUT "${NUPKG_FILE}" DESTINATION "${EXTRACT_DIR}")
    file(TOUCH "${EXTRACT_DIR}/extracted.stamp")
    file(REMOVE "${NUPKG_FILE}")
  endif()

  # Collect native DLLs for our RID
  file(GLOB FOUND_DLLS "${EXTRACT_DIR}/runtimes/${NUGET_RID}/native/*.dll")
  set(${OUT_DLLS} ${FOUND_DLLS} PARENT_SCOPE)
endfunction()

# ── Fetch each package ──────────────────────────────────────────────────────

# FLCore — from ADO ORT-Nightly feed (public, no auth required)
set(ORT_NIGHTLY_FEED "https://pkgs.dev.azure.com/aiinfra/PublicPackages/_packaging/ORT-Nightly/nuget/v3/flat2")

fetch_nuget_package(
  "Microsoft.AI.Foundry.Local.Core"
  "${FLCORE_VERSION}"
  "${ORT_NIGHTLY_FEED}"
  FLCORE_DLLS
)

# OnnxRuntime — from ORT-Nightly feed
fetch_nuget_package(
  "Microsoft.ML.OnnxRuntime.Foundry"
  "${ORT_VERSION}"
  "${ORT_NIGHTLY_FEED}"
  ORT_DLLS
)

# OnnxRuntimeGenAI — from ORT-Nightly feed
fetch_nuget_package(
  "Microsoft.ML.OnnxRuntimeGenAI.Foundry"
  "${ORTGENAI_VERSION}"
  "${ORT_NIGHTLY_FEED}"
  ORTGENAI_DLLS
)

# ── Aggregate all native DLLs ───────────────────────────────────────────────
set(NATIVE_DEPS_DLLS ${FLCORE_DLLS} ${ORT_DLLS} ${ORTGENAI_DLLS})

message(STATUS "Native DLLs found:")
foreach(DLL ${NATIVE_DEPS_DLLS})
  get_filename_component(DLL_NAME "${DLL}" NAME)
  message(STATUS "  ${DLL_NAME}")
endforeach()

# ── Helper: copy DLLs to a target's output directory ────────────────────────
function(copy_native_deps_to_target TARGET_NAME)
  foreach(DLL ${NATIVE_DEPS_DLLS})
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" "$<TARGET_FILE_DIR:${TARGET_NAME}>"
      COMMENT "Copying native DLL to output"
    )
  endforeach()
endfunction()
