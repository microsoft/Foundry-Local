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

# Create imported static library target
if(NOT TARGET FoundryLocal::FoundryLocal)
    add_library(FoundryLocal::FoundryLocal STATIC IMPORTED)

    set_target_properties(FoundryLocal::FoundryLocal PROPERTIES
        IMPORTED_LOCATION "${_FL_SDK_DIR}/lib/CppSdk.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${_FL_SDK_DIR}/include"
    )

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
