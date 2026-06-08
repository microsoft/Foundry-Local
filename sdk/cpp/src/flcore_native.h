// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <cstdint>
#include <type_traits>

#ifndef FL_CDECL
  #ifdef _WIN32
    #define FL_CDECL __cdecl
  #else
    #define FL_CDECL
  #endif
#endif

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

    // Callback signature: int32_t(*)(const void* data, int length, void* userData)
    // Return 0 to continue, 1 to cancel.
    using UserCallbackFn = int32_t(FL_CDECL*)(const void*, int32_t, void*);

    struct StreamingRequestBuffer {
        const void* Command;
        int32_t CommandLength;
        const void* Data;
        int32_t DataLength;
        const void* BinaryData;
        int32_t BinaryDataLength;
    };

    // Exported function pointer types
    using execute_command_fn = void(FL_CDECL*)(RequestBuffer*, ResponseBuffer*);
    using execute_command_with_callback_fn = void(FL_CDECL*)(RequestBuffer*, ResponseBuffer*,
                                                            UserCallbackFn /*callback*/,
                                                            void* /*userData*/);
    using execute_command_with_binary_fn = void(FL_CDECL*)(StreamingRequestBuffer*, ResponseBuffer*);
    using free_response_fn = void(FL_CDECL*)(ResponseBuffer*);

    static_assert(std::is_standard_layout<RequestBuffer>::value, "RequestBuffer must be standard layout");
    static_assert(std::is_standard_layout<ResponseBuffer>::value, "ResponseBuffer must be standard layout");
    static_assert(std::is_standard_layout<StreamingRequestBuffer>::value, "StreamingRequestBuffer must be standard layout");

#pragma pack(pop)
}
