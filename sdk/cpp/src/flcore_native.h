// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <cstdint>

extern "C"
{
    // Layout must match C# structs exactly
#pragma pack(push, 8)
    struct RequestBuffer {
        const void* Command;
        int32_t CommandLength;
        const void* Data;
        int32_t DataLength;
    };

    struct ResponseBuffer {
        void* Data;
        int32_t DataLength;
        void* Error;
        int32_t ErrorLength;
    };

    // Callback signature: void(*)(void* data, int length, void* userData)
    using UserCallbackFn = void(__cdecl*)(void*, int32_t, void*);

    // Exported function pointer types
    using execute_command_fn = void(__cdecl*)(RequestBuffer*, ResponseBuffer*);
    using execute_command_with_callback_fn = void(__cdecl*)(RequestBuffer*, ResponseBuffer*, void* /*callback*/,
                                                            void* /*userData*/);
    using free_response_fn = void(__cdecl*)(ResponseBuffer*);

    static_assert(std::is_standard_layout<RequestBuffer>::value, "RequestBuffer must be standard layout");
    static_assert(std::is_standard_layout<ResponseBuffer>::value, "ResponseBuffer must be standard layout");

#pragma pack(pop)
}
