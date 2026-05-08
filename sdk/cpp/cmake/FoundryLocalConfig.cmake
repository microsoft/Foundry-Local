# FoundryLocalConfig.cmake
#
# Imported target: FoundryLocal::FoundryLocal
#
# Usage in your CMakeLists.txt:
#   list(APPEND CMAKE_PREFIX_PATH "<path-to-foundry-local-sdk>")
#   find_package(FoundryLocal REQUIRED)
#   target_link_libraries(my_app PRIVATE FoundryLocal::FoundryLocal)
#   fl_copy_runtime_dlls(my_app)

get_filename_component(_FL_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Validate SDK layout
if(NOT EXISTS "${_FL_SDK_DIR}/include/foundry_local.h")
    message(FATAL_ERROR
        "FoundryLocal SDK incomplete: include/foundry_local.h not found at ${_FL_SDK_DIR}/include/. "
        "Ensure CMAKE_PREFIX_PATH points to the correct SDK directory."
    )
endif()

if(NOT EXISTS "${_FL_SDK_DIR}/lib/CppSdk.lib" AND NOT EXISTS "${_FL_SDK_DIR}/lib/debug/CppSdk.lib")
    message(FATAL_ERROR
        "FoundryLocal SDK incomplete: CppSdk.lib not found at ${_FL_SDK_DIR}/lib/. "
        "Build and install the SDK first: cmake --install out/build/x64-release --prefix <sdk-dir>"
    )
endif()

if(WIN32 AND NOT EXISTS "${_FL_SDK_DIR}/bin")
    message(FATAL_ERROR
        "FoundryLocal SDK incomplete: bin/ directory not found at ${_FL_SDK_DIR}/bin/. "
        "Runtime DLLs are required. Rebuild and install the SDK."
    )
endif()

# Create imported static library target
if(NOT TARGET FoundryLocal::FoundryLocal)
    add_library(FoundryLocal::FoundryLocal STATIC IMPORTED)

    # Support both Debug and Release libs if available
    if(EXISTS "${_FL_SDK_DIR}/lib/debug/CppSdk.lib")
        set_target_properties(FoundryLocal::FoundryLocal PROPERTIES
            IMPORTED_LOCATION_RELEASE "${_FL_SDK_DIR}/lib/CppSdk.lib"
            IMPORTED_LOCATION_DEBUG "${_FL_SDK_DIR}/lib/debug/CppSdk.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${_FL_SDK_DIR}/include"
        )
    elseif(EXISTS "${_FL_SDK_DIR}/lib/CppSdk.lib")
        # Single-config: warn if consumer build type doesn't match
        if(CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
            message(WARNING
                "FoundryLocal SDK was built in Release mode. "
                "Linking with CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} may cause "
                "MSVC runtime-library mismatches. Consider using Release or "
                "rebuilding the SDK in Debug mode."
            )
        endif()
        set_target_properties(FoundryLocal::FoundryLocal PROPERTIES
            IMPORTED_LOCATION "${_FL_SDK_DIR}/lib/CppSdk.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${_FL_SDK_DIR}/include"
        )
    else()
        message(FATAL_ERROR "FoundryLocal SDK library not found at ${_FL_SDK_DIR}/lib/")
    endif()

    # Require nlohmann_json and GSL from vcpkg (consumer must have these)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(Microsoft.GSL CONFIG REQUIRED)

    set_property(TARGET FoundryLocal::FoundryLocal APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES
            nlohmann_json::nlohmann_json
            Microsoft.GSL::GSL
    )
endif()

# Runtime DLLs directory
set(FL_RUNTIME_DLL_DIR "${_FL_SDK_DIR}/bin")

# Helper function: copy Foundry Local runtime DLLs next to an executable
function(fl_copy_runtime_dlls TARGET_NAME)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${FL_RUNTIME_DLL_DIR}"
            $<TARGET_FILE_DIR:${TARGET_NAME}>
        COMMENT "Copying Foundry Local runtime DLLs for ${TARGET_NAME}..."
    )
endfunction()

set(FoundryLocal_FOUND TRUE)
