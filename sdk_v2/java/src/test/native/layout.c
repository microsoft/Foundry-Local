/* Copyright (c) Microsoft Corporation. Licensed under the MIT License. */
#include <stddef.h>
#include <stdio.h>
#include "foundry_local/foundry_local_c.h"

/* Compile against the header extracted from the pinned runtime package, not current main. */
_Static_assert(FOUNDRY_LOCAL_API_VERSION == 1, "Wrong package header");
_Static_assert(sizeof(void*) == 8, "64-bit targets only");
_Static_assert(sizeof(flStreamingCallbackData) == 16, "Callback ABI mismatch");
_Static_assert(sizeof(flAudioData) == 72, "Audio ABI mismatch");
_Static_assert(sizeof(flBytesData) == 48, "Bytes ABI mismatch");
_Static_assert(sizeof(flSpeechSegmentData) == 64, "Segment ABI mismatch");
_Static_assert(sizeof(flSpeechResultData) == 48, "Result ABI mismatch");
_Static_assert(offsetof(flBytesData, mutable_data) == 16, "Deleter ABI mismatch");
_Static_assert(offsetof(flApi, GetCatalogApi) == 10 * 8, "Root table ABI mismatch");
_Static_assert(offsetof(flApi, Manager_Shutdown) == 27 * 8, "Root shutdown ABI mismatch");
_Static_assert(offsetof(flItemApi, GetSpeechSegment) == 19 * 8, "Item table ABI mismatch");
_Static_assert(offsetof(flItemApi, ItemQueue_MarkFinished) == 29 * 8, "Queue table ABI mismatch");
_Static_assert(offsetof(flInferenceApi, Session_ProcessRequest) == 17 * 8, "Inference table ABI mismatch");
_Static_assert(offsetof(flModelApi, Info_GetStringProperty) == 21 * 8, "Model table ABI mismatch");

int main(void) {
    puts("{\"apiVersion\":1,\"pointerSize\":8,\"callbackSize\":16,\"audioSize\":72,"
         "\"bytesSize\":48,\"segmentSize\":64,\"resultSize\":48}");
    return 0;
}
