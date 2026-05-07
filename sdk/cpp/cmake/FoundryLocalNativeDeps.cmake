# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

include_guard(GLOBAL)

set(FOUNDRY_LOCAL_CPP_NUGET_CACHE_DIR
    "${CMAKE_BINARY_DIR}/_deps/foundry-local-nuget"
    CACHE PATH
    "Directory used to cache Foundry Local native NuGet packages for C++ builds.")

set(FOUNDRY_LOCAL_CPP_NUGET_FEEDS
    "https://api.nuget.org/v3/index.json;https://pkgs.dev.azure.com/aiinfra/PublicPackages/_packaging/ORT-Nightly/nuget/v3/index.json"
    CACHE STRING
    "Semicolon-separated NuGet v3 service indexes used for Foundry Local C++ native dependencies.")

macro(_foundry_local_lock_cache_resource resource_name)
  string(SHA256 _foundry_local_lock_hash "${resource_name}")
  set(_foundry_local_lock_dir "${FOUNDRY_LOCAL_CPP_NUGET_CACHE_DIR}/locks")
  file(MAKE_DIRECTORY "${_foundry_local_lock_dir}")
  file(LOCK "${_foundry_local_lock_dir}/${_foundry_local_lock_hash}.lock"
       GUARD FUNCTION
       TIMEOUT 300
       RESULT_VARIABLE _foundry_local_lock_result)
  if(NOT _foundry_local_lock_result STREQUAL "0")
    message(FATAL_ERROR
        "Timed out waiting for Foundry Local C++ native dependency cache lock "
        "for '${resource_name}': ${_foundry_local_lock_result}")
  endif()
endmacro()

function(_foundry_local_status_failed status_var out_failed out_message)
  set(_status ${${status_var}})
  list(GET _status 0 _code)
  list(GET _status 1 _message)
  if(_code EQUAL 0)
    set(${out_failed} FALSE PARENT_SCOPE)
  else()
    set(${out_failed} TRUE PARENT_SCOPE)
  endif()
  set(${out_message} "${_message}" PARENT_SCOPE)
endfunction()

function(_foundry_local_get_package_base_address out_var feed_url)
  string(SHA256 _feed_hash "${feed_url}")
  set(_index_path "${FOUNDRY_LOCAL_CPP_NUGET_CACHE_DIR}/service-indexes/${_feed_hash}.json")
  file(MAKE_DIRECTORY "${FOUNDRY_LOCAL_CPP_NUGET_CACHE_DIR}/service-indexes")

  if(NOT EXISTS "${_index_path}")
    _foundry_local_lock_cache_resource("service-index:${feed_url}")
  endif()

  if(NOT EXISTS "${_index_path}")
    string(RANDOM LENGTH 16 ALPHABET "0123456789abcdef" _temp_suffix)
    set(_temp_index_path "${_index_path}.${_temp_suffix}.tmp")
    file(DOWNLOAD "${feed_url}" "${_temp_index_path}" TLS_VERIFY ON STATUS _download_status)
    _foundry_local_status_failed(_download_status _failed _message)
    if(_failed)
      file(REMOVE "${_temp_index_path}")
      message(VERBOSE "Failed to download NuGet service index ${feed_url}: ${_message}")
      set(${out_var} "" PARENT_SCOPE)
      return()
    endif()
    file(RENAME "${_temp_index_path}" "${_index_path}")
  endif()

  file(READ "${_index_path}" _index_json)
  string(JSON _resource_count ERROR_VARIABLE _resource_error LENGTH "${_index_json}" resources)
  if(_resource_error)
    message(VERBOSE "NuGet service index ${feed_url} did not contain a resources array: ${_resource_error}")
    set(${out_var} "" PARENT_SCOPE)
    return()
  endif()

  if(_resource_count EQUAL 0)
    set(${out_var} "" PARENT_SCOPE)
    return()
  endif()

  math(EXPR _last_resource "${_resource_count} - 1")
  foreach(_index RANGE 0 ${_last_resource})
    string(JSON _type ERROR_VARIABLE _type_error GET "${_index_json}" resources ${_index} "@type")
    if(_type_error)
      continue()
    endif()

    if(_type MATCHES "^PackageBaseAddress/")
      string(JSON _id GET "${_index_json}" resources ${_index} "@id")
      if(NOT _id MATCHES "/$")
        string(APPEND _id "/")
      endif()
      set(${out_var} "${_id}" PARENT_SCOPE)
      return()
    endif()
  endforeach()

  set(${out_var} "" PARENT_SCOPE)
endfunction()

function(_foundry_local_download_nupkg out_var package_id package_version)
  string(TOLOWER "${package_id}" _package_id_lower)
  string(TOLOWER "${package_version}" _package_version_lower)
  set(_package_dir "${FOUNDRY_LOCAL_CPP_NUGET_CACHE_DIR}/packages")
  set(_package_path "${_package_dir}/${_package_id_lower}.${_package_version_lower}.nupkg")
  file(MAKE_DIRECTORY "${_package_dir}")

  if(EXISTS "${_package_path}")
    set(${out_var} "${_package_path}" PARENT_SCOPE)
    return()
  endif()

  _foundry_local_lock_cache_resource("package:${_package_id_lower}:${_package_version_lower}")

  if(EXISTS "${_package_path}")
    set(${out_var} "${_package_path}" PARENT_SCOPE)
    return()
  endif()

  set(_last_error "")
  foreach(_feed_url IN LISTS FOUNDRY_LOCAL_CPP_NUGET_FEEDS)
    _foundry_local_get_package_base_address(_base_address "${_feed_url}")
    if(NOT _base_address)
      set(_last_error "Could not resolve PackageBaseAddress from ${_feed_url}")
      continue()
    endif()

    set(_download_url "${_base_address}${_package_id_lower}/${_package_version_lower}/${_package_id_lower}.${_package_version_lower}.nupkg")
    string(RANDOM LENGTH 16 ALPHABET "0123456789abcdef" _temp_suffix)
    set(_temp_path "${_package_path}.${_temp_suffix}.tmp")
    message(STATUS "Downloading ${package_id} ${package_version}")
    file(DOWNLOAD "${_download_url}" "${_temp_path}" TLS_VERIFY ON STATUS _download_status)
    _foundry_local_status_failed(_download_status _failed _message)
    if(_failed)
      file(REMOVE "${_temp_path}")
      set(_last_error "${_message}")
      continue()
    endif()

    file(RENAME "${_temp_path}" "${_package_path}")
    set(${out_var} "${_package_path}" PARENT_SCOPE)
    return()
  endforeach()

  message(FATAL_ERROR
      "Failed to download ${package_id} ${package_version} from configured NuGet feeds. "
      "Last error: ${_last_error}")
endfunction()

function(_foundry_local_extract_nupkg out_var package_path package_id package_version)
  string(TOLOWER "${package_id}" _package_id_lower)
  string(TOLOWER "${package_version}" _package_version_lower)
  set(_extract_dir "${FOUNDRY_LOCAL_CPP_NUGET_CACHE_DIR}/extract/${_package_id_lower}/${_package_version_lower}")
  set(_stamp "${_extract_dir}/.extracted")

  if(NOT EXISTS "${_stamp}")
    _foundry_local_lock_cache_resource("extract:${_package_id_lower}:${_package_version_lower}")
  endif()

  if(NOT EXISTS "${_stamp}")
    string(RANDOM LENGTH 16 ALPHABET "0123456789abcdef" _temp_suffix)
    set(_temp_extract_dir "${_extract_dir}.tmp-${_temp_suffix}")
    file(REMOVE_RECURSE "${_temp_extract_dir}")
    file(MAKE_DIRECTORY "${_temp_extract_dir}")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E tar xf "${package_path}"
      WORKING_DIRECTORY "${_temp_extract_dir}"
      RESULT_VARIABLE _extract_result
      OUTPUT_QUIET
      ERROR_VARIABLE _extract_error)
    if(NOT _extract_result EQUAL 0)
      file(REMOVE_RECURSE "${_temp_extract_dir}")
      message(FATAL_ERROR "Failed to extract ${package_path}: ${_extract_error}")
    endif()
    file(WRITE "${_temp_extract_dir}/.extracted" "${package_id} ${package_version}\n")
    file(REMOVE_RECURSE "${_extract_dir}")
    file(RENAME "${_temp_extract_dir}" "${_extract_dir}")
  endif()

  set(${out_var} "${_extract_dir}" PARENT_SCOPE)
endfunction()

function(_foundry_local_stage_package_native_files stage_dir package_id package_version rid)
  _foundry_local_download_nupkg(_package_path "${package_id}" "${package_version}")
  _foundry_local_extract_nupkg(_extract_dir "${_package_path}" "${package_id}" "${package_version}")

  file(GLOB _native_files
      "${_extract_dir}/runtimes/${rid}/native/*.dll"
      "${_extract_dir}/runtimes/${rid}/*.dll")

  if(NOT _native_files)
    message(WARNING "${package_id} ${package_version} did not contain native DLLs for ${rid}.")
    return()
  endif()

  foreach(_native_file IN LISTS _native_files)
    get_filename_component(_native_name "${_native_file}" NAME)
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "${_native_file}"
              "${stage_dir}/${_native_name}"
      RESULT_VARIABLE _copy_result)
    if(NOT _copy_result EQUAL 0)
      message(FATAL_ERROR "Failed to stage ${_native_file}.")
    endif()
  endforeach()
endfunction()

function(_foundry_local_windows_rid out_var)
  if(NOT WIN32)
    message(FATAL_ERROR "Foundry Local WinML native dependencies are only available on Windows.")
  endif()

  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64|X86_64)$")
    set(${out_var} "win-x64" PARENT_SCOPE)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|aarch64|AARCH64)$")
    set(${out_var} "win-arm64" PARENT_SCOPE)
  else()
    message(FATAL_ERROR "Unsupported Windows architecture for WinML native dependencies: ${CMAKE_SYSTEM_PROCESSOR}")
  endif()
endfunction()

function(_foundry_local_prepare_winml_native_deps out_var)
  set(_deps_file "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../deps_versions_winml.json")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_deps_file}")
  file(READ "${_deps_file}" _deps_json)

  string(JSON _core_version GET "${_deps_json}" "foundry-local-core" "nuget")
  string(JSON _ort_version GET "${_deps_json}" "onnxruntime" "version")
  string(JSON _genai_version GET "${_deps_json}" "onnxruntime-genai" "version")
  string(JSON _winml_runtime_version GET "${_deps_json}" "windows-ai-machinelearning" "version")

  if(DEFINED ENV{FOUNDRY_WINDOWS_AI_MACHINELEARNING_VERSION} AND NOT "$ENV{FOUNDRY_WINDOWS_AI_MACHINELEARNING_VERSION}" STREQUAL "")
    set(_winml_runtime_version "$ENV{FOUNDRY_WINDOWS_AI_MACHINELEARNING_VERSION}")
  endif()

  _foundry_local_windows_rid(_rid)

  set(_stage_dir
      "${FOUNDRY_LOCAL_CPP_NUGET_CACHE_DIR}/native/winml-${_rid}-${_core_version}-${_ort_version}-${_genai_version}-${_winml_runtime_version}")
  set(_complete_stamp "${_stage_dir}/.complete")

  if(NOT EXISTS "${_complete_stamp}")
    _foundry_local_lock_cache_resource("native:${_rid}:${_core_version}:${_ort_version}:${_genai_version}:${_winml_runtime_version}")
  endif()

  if(NOT EXISTS "${_complete_stamp}")
    string(RANDOM LENGTH 16 ALPHABET "0123456789abcdef" _temp_suffix)
    set(_temp_stage_dir "${_stage_dir}.tmp-${_temp_suffix}")
    file(REMOVE_RECURSE "${_temp_stage_dir}")
    file(MAKE_DIRECTORY "${_temp_stage_dir}")

    _foundry_local_stage_package_native_files(
      "${_temp_stage_dir}" "Microsoft.AI.Foundry.Local.Core.WinML" "${_core_version}" "${_rid}")
    _foundry_local_stage_package_native_files(
      "${_temp_stage_dir}" "Microsoft.ML.OnnxRuntime.Foundry" "${_ort_version}" "${_rid}")
    _foundry_local_stage_package_native_files(
      "${_temp_stage_dir}" "Microsoft.ML.OnnxRuntimeGenAI.Foundry" "${_genai_version}" "${_rid}")
    _foundry_local_stage_package_native_files(
      "${_temp_stage_dir}" "Microsoft.Windows.AI.MachineLearning" "${_winml_runtime_version}" "${_rid}")

    file(GLOB _staged_dlls "${_temp_stage_dir}/*.dll")
    if(NOT _staged_dlls)
      file(REMOVE_RECURSE "${_temp_stage_dir}")
      message(FATAL_ERROR "No WinML native DLLs were staged in ${_temp_stage_dir}.")
    endif()

    file(WRITE "${_temp_stage_dir}/.complete" "Foundry Local C++ WinML native dependencies for ${_rid}\n")
    file(REMOVE_RECURSE "${_stage_dir}")
    file(RENAME "${_temp_stage_dir}" "${_stage_dir}")
  endif()

  set(${out_var} "${_stage_dir}" PARENT_SCOPE)
endfunction()

function(foundry_local_copy_winml_native_deps target_name)
  if(NOT TARGET "${target_name}")
    message(FATAL_ERROR "Target '${target_name}' does not exist.")
  endif()

  _foundry_local_prepare_winml_native_deps(_native_dir)
  file(GLOB _native_files "${_native_dir}/*.dll")

  foreach(_native_file IN LISTS _native_files)
    add_custom_command(TARGET "${target_name}" POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "${_native_file}"
              "$<TARGET_FILE_DIR:${target_name}>"
      VERBATIM)
  endforeach()

  message(STATUS "Foundry Local WinML native dependencies for ${target_name}: ${_native_dir}")
endfunction()
