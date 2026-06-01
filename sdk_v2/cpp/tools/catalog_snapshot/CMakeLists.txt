# Copyright (c) Microsoft. All rights reserved.

# Links against foundry_local_static (not foundry_local_cpp) because the tool
# uses internal SDK symbols (MakeCatalogClient, ModelInfoToJson, IEpDetector,
# StderrLogger) that aren't part of the public C++ wrapper.
add_executable(catalog_snapshot main.cc)
target_link_libraries(catalog_snapshot
    PRIVATE
        foundry_local_static
        nlohmann_json::nlohmann_json
)

# Internal headers live under src/.
target_include_directories(catalog_snapshot
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

# Ensure ORT/GenAI runtime DLLs are present alongside the executable —
# the live catalog client transitively pulls in ORT initialization paths.
add_dependencies(catalog_snapshot foundry_local)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(catalog_snapshot PRIVATE -Wl,--disable-new-dtags)
endif()
