cmake_minimum_required(VERSION 3.20)
project(foundry_local_cpp_samples LANGUAGES CXX)

set(FOUNDRY_LOCAL_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk_v2/cpp" CACHE PATH
    "Path to the Foundry Local C++ SDK source tree")
set(FOUNDRY_LOCAL_SDK_BUILD_DIR "" CACHE PATH
    "Canonical SDK build directory produced by sdk_v2/cpp/build.py")

if(NOT FOUNDRY_LOCAL_SDK_BUILD_DIR)
  if(WIN32)
    set(_platform "Windows")
  elseif(APPLE)
    set(_platform "macOS")
  else()
    set(_platform "Linux")
  endif()
  set(FOUNDRY_LOCAL_SDK_BUILD_DIR "${FOUNDRY_LOCAL_SDK_DIR}/build/${_platform}/RelWithDebInfo")
endif()

get_filename_component(FOUNDRY_LOCAL_SDK_CONFIG "${FOUNDRY_LOCAL_SDK_BUILD_DIR}" NAME)

find_library(FOUNDRY_LOCAL_LIBRARY
  NAMES foundry_local
  PATHS
    "${FOUNDRY_LOCAL_SDK_BUILD_DIR}"
    "${FOUNDRY_LOCAL_SDK_BUILD_DIR}/${FOUNDRY_LOCAL_SDK_CONFIG}"
    "${FOUNDRY_LOCAL_SDK_BUILD_DIR}/lib"
    "${FOUNDRY_LOCAL_SDK_BUILD_DIR}/lib/${FOUNDRY_LOCAL_SDK_CONFIG}"
    "${FOUNDRY_LOCAL_SDK_BUILD_DIR}/bin"
    "${FOUNDRY_LOCAL_SDK_BUILD_DIR}/bin/${FOUNDRY_LOCAL_SDK_CONFIG}"
  NO_DEFAULT_PATH
  REQUIRED
)

add_library(foundry_local_sdk SHARED IMPORTED GLOBAL)
if(WIN32)
  set_target_properties(foundry_local_sdk PROPERTIES
    IMPORTED_IMPLIB "${FOUNDRY_LOCAL_LIBRARY}"
    IMPORTED_LOCATION "${FOUNDRY_LOCAL_SDK_BUILD_DIR}/bin/${FOUNDRY_LOCAL_SDK_CONFIG}/foundry_local.dll"
  )
else()
  set_target_properties(foundry_local_sdk PROPERTIES IMPORTED_LOCATION "${FOUNDRY_LOCAL_LIBRARY}")
endif()
target_include_directories(foundry_local_sdk SYSTEM INTERFACE "${FOUNDRY_LOCAL_SDK_DIR}/include")

function(add_foundry_sample target directory)
  add_executable(${target} "${directory}/main.cc")
  target_compile_features(${target} PRIVATE cxx_std_20)
  target_link_libraries(${target} PRIVATE foundry_local_sdk)
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../..")

  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()

add_foundry_sample(hello-world hello-world)
add_foundry_sample(realtime-transcription realtime-transcription)
add_foundry_sample(embeddings embeddings)
add_foundry_sample(tool-calling tool-calling)
add_foundry_sample(web-server web-server)
add_foundry_sample(vision vision)
add_foundry_sample(byom byom)

find_package(Threads REQUIRED)
target_link_libraries(realtime-transcription PRIVATE Threads::Threads)

find_path(PORTAUDIO_INCLUDE_DIR portaudio.h)
find_library(PORTAUDIO_LIBRARY NAMES portaudio)
if(PORTAUDIO_INCLUDE_DIR AND PORTAUDIO_LIBRARY)
  target_compile_definitions(realtime-transcription PRIVATE FOUNDRY_SAMPLE_HAS_PORTAUDIO=1)
  target_include_directories(realtime-transcription PRIVATE "${PORTAUDIO_INCLUDE_DIR}")
  target_link_libraries(realtime-transcription PRIVATE "${PORTAUDIO_LIBRARY}")
endif()

if(WIN32)
  target_link_libraries(web-server PRIVATE ws2_32)
endif()
