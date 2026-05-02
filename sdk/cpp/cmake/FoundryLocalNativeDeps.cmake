# FoundryLocalNativeDeps.cmake
#
# Downloads Foundry Local Core + ONNX Runtime native libraries from NuGet
# at configure time. Mirrors the Rust SDK's build.rs approach.
#
# Outputs:
#   FOUNDRY_NATIVE_DIR  - directory containing the downloaded native libraries
#
# The caller should copy ${FOUNDRY_NATIVE_DIR}/*.dll next to their executable.
# A convenience function foundry_local_copy_native_deps(<target>) is provided.

include(FetchContent)

# ---------------------------------------------------------------------------
# Read deps_versions.json to get pinned package versions
# ---------------------------------------------------------------------------
function(_foundry_read_deps_versions out_core out_ort out_genai)
  # Look for deps_versions.json: first next to CMakeLists.txt, then parent sdk/ dir
  set(_candidates
    "${CMAKE_CURRENT_SOURCE_DIR}/deps_versions.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/../deps_versions.json"
  )

  set(_found "")
  foreach(_path ${_candidates})
    if(EXISTS "${_path}")
      set(_found "${_path}")
      break()
    endif()
  endforeach()

  if(NOT _found)
    message(FATAL_ERROR "deps_versions.json not found. Searched: ${_candidates}")
  endif()

  file(READ "${_found}" _json)

  # Parse versions using CMake's string(JSON) (CMake 3.19+)
  string(JSON _core_version GET "${_json}" "foundry-local-core" "nuget")
  string(JSON _ort_version GET "${_json}" "onnxruntime" "version")
  string(JSON _genai_version GET "${_json}" "onnxruntime-genai" "version")

  set(${out_core} "${_core_version}" PARENT_SCOPE)
  set(${out_ort} "${_ort_version}" PARENT_SCOPE)
  set(${out_genai} "${_genai_version}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Download a .nupkg from NuGet and extract native libs for the current RID
# ---------------------------------------------------------------------------
function(_foundry_download_nuget_native pkg_name pkg_version rid out_dir)
  string(TOLOWER "${pkg_name}" _lower_name)
  string(TOLOWER "${pkg_version}" _lower_version)

  set(_nupkg_url "https://api.nuget.org/v3-flatcontainer/${_lower_name}/${_lower_version}/${_lower_name}.${_lower_version}.nupkg")
  set(_download_path "${out_dir}/${_lower_name}.${_lower_version}.nupkg")

  # Skip if already downloaded
  if(NOT EXISTS "${_download_path}")
    message(STATUS "Downloading ${pkg_name} ${pkg_version} from NuGet...")
    file(DOWNLOAD "${_nupkg_url}" "${_download_path}"
      STATUS _dl_status
      TLS_VERIFY ON
    )
    list(GET _dl_status 0 _dl_code)
    if(NOT _dl_code EQUAL 0)
      list(GET _dl_status 1 _dl_msg)
      message(FATAL_ERROR "Failed to download ${pkg_name}: ${_dl_msg}")
    endif()
  endif()

  # Extract native binaries for this RID
  set(_extract_dir "${out_dir}/${_lower_name}-extract")
  if(NOT EXISTS "${_extract_dir}")
    file(ARCHIVE_EXTRACT INPUT "${_download_path}" DESTINATION "${_extract_dir}")
  endif()

  set(_native_dir "${_extract_dir}/runtimes/${rid}/native")
  if(EXISTS "${_native_dir}")
    file(GLOB _native_files "${_native_dir}/*${CMAKE_SHARED_LIBRARY_SUFFIX}")
    foreach(_f ${_native_files})
      get_filename_component(_fname "${_f}" NAME)
      file(COPY_FILE "${_f}" "${out_dir}/${_fname}" ONLY_IF_DIFFERENT)
      message(STATUS "  Extracted ${_fname}")
    endforeach()
  else()
    message(WARNING "No native binaries found for RID '${rid}' in ${pkg_name} ${pkg_version}")
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Main: download all native deps
# ---------------------------------------------------------------------------
function(foundry_local_download_native_deps)
  # Determine RID
  if(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "AARCH64")
      set(_rid "win-arm64")
    else()
      set(_rid "win-x64")
    endif()
  elseif(APPLE)
    set(_rid "osx-arm64")
  elseif(UNIX)
    set(_rid "linux-x64")
  else()
    message(WARNING "Unsupported platform — native libraries will not be downloaded.")
    return()
  endif()

  # Allow override (CI sets this to use pipeline-built binaries)
  if(DEFINED ENV{FOUNDRY_NATIVE_OVERRIDE_DIR} AND IS_DIRECTORY "$ENV{FOUNDRY_NATIVE_OVERRIDE_DIR}")
    set(_native_dir "$ENV{FOUNDRY_NATIVE_OVERRIDE_DIR}")
    message(STATUS "Using native libraries from FOUNDRY_NATIVE_OVERRIDE_DIR: ${_native_dir}")
    set(FOUNDRY_NATIVE_DIR "${_native_dir}" CACHE PATH "Directory containing Foundry Local native libraries" FORCE)
    return()
  endif()

  set(_native_dir "${CMAKE_BINARY_DIR}/_foundry_native")
  file(MAKE_DIRECTORY "${_native_dir}")

  _foundry_read_deps_versions(_core_ver _ort_ver _genai_ver)
  message(STATUS "Foundry Local native deps: Core=${_core_ver} ORT=${_ort_ver} GenAI=${_genai_ver}")

  # Check if all required libs are already present
  set(_core_lib "${_native_dir}/Microsoft.AI.Foundry.Local.Core${CMAKE_SHARED_LIBRARY_SUFFIX}")
  if(WIN32)
    set(_ort_lib "${_native_dir}/onnxruntime${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(_genai_lib "${_native_dir}/onnxruntime-genai${CMAKE_SHARED_LIBRARY_SUFFIX}")
  else()
    set(_ort_lib "${_native_dir}/libonnxruntime${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(_genai_lib "${_native_dir}/libonnxruntime-genai${CMAKE_SHARED_LIBRARY_SUFFIX}")
  endif()

  if(EXISTS "${_core_lib}" AND EXISTS "${_ort_lib}" AND EXISTS "${_genai_lib}")
    message(STATUS "Native libraries already present, skipping download.")
    set(FOUNDRY_NATIVE_DIR "${_native_dir}" CACHE PATH "Directory containing Foundry Local native libraries" FORCE)
    return()
  endif()

  # Download each package
  _foundry_download_nuget_native("Microsoft.AI.Foundry.Local.Core" "${_core_ver}" "${_rid}" "${_native_dir}")

  if(_rid STREQUAL "linux-x64")
    _foundry_download_nuget_native("Microsoft.ML.OnnxRuntime.Gpu.Linux" "${_ort_ver}" "${_rid}" "${_native_dir}")
  else()
    _foundry_download_nuget_native("Microsoft.ML.OnnxRuntime.Foundry" "${_ort_ver}" "${_rid}" "${_native_dir}")
  endif()

  _foundry_download_nuget_native("Microsoft.ML.OnnxRuntimeGenAI.Foundry" "${_genai_ver}" "${_rid}" "${_native_dir}")

  set(FOUNDRY_NATIVE_DIR "${_native_dir}" CACHE PATH "Directory containing Foundry Local native libraries" FORCE)
endfunction()

# ---------------------------------------------------------------------------
# Convenience: copy native libs next to a target's output binary
# ---------------------------------------------------------------------------
function(foundry_local_copy_native_deps target)
  if(NOT DEFINED FOUNDRY_NATIVE_DIR OR NOT IS_DIRECTORY "${FOUNDRY_NATIVE_DIR}")
    message(WARNING "FOUNDRY_NATIVE_DIR not set — call foundry_local_download_native_deps() first")
    return()
  endif()

  file(GLOB _native_libs "${FOUNDRY_NATIVE_DIR}/*${CMAKE_SHARED_LIBRARY_SUFFIX}")
  foreach(_lib ${_native_libs})
    get_filename_component(_fname "${_lib}" NAME)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_lib}" "$<TARGET_FILE_DIR:${target}>/${_fname}"
      COMMENT "Copying ${_fname} to output directory"
    )
  endforeach()
endfunction()
