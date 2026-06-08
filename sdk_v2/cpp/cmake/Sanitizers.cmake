# Copyright (c) Microsoft. All rights reserved.
#
# Opt-in AddressSanitizer + UndefinedBehaviorSanitizer support.
#
# Enabled by configuring with -DFOUNDRY_LOCAL_ENABLE_ASAN=ON. Linux only.
# Driven by sdk_v2/cpp/scripts/run_sanitizer_tests.py — see
# .github/instructions/cpp-memory-validation.instructions.md.
#
# Flags are applied globally via add_compile_options/add_link_options because
# ASan must instrument every translation unit (including third-party) linked
# into the test binaries; mixing instrumented and non-instrumented code
# produces false positives or missed errors.

if(NOT FOUNDRY_LOCAL_ENABLE_ASAN)
    return()
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR
        "FOUNDRY_LOCAL_ENABLE_ASAN=ON is only supported on Linux (including WSL). "
        "Current system: ${CMAKE_SYSTEM_NAME}.")
endif()

message(STATUS "AddressSanitizer + UndefinedBehaviorSanitizer: ENABLED")

add_compile_options(
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
    -fno-sanitize-recover=all
    -g
)
add_link_options(-fsanitize=address,undefined)
