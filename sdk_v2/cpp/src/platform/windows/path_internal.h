// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Internal Windows-only helper used by fl::platform::GetWeaklyCanonicalPath
// as the AppContainer fallback. Exposed here (rather than kept in an anonymous
// namespace) so unit tests can exercise it directly — the AppContainer trigger
// itself cannot be reproduced in a normal test environment.
#pragma once

#ifdef _WIN32

#include <filesystem>

namespace fl::platform::internal {

/// Implements weakly_canonical via GetFinalPathNameByHandleW with
/// FILE_NAME_NORMALIZED | VOLUME_NAME_NT, prefixed with "\\?\GLOBALROOT".
/// Walks `input`'s parent chain to find the longest existing prefix, queries
/// the OS for its canonical NT-volume path, then lexically appends the missing
/// tail. Returns false if no existing prefix is found, or if the OS call
/// fails — callers must NOT substitute an unverified path in that case.
bool WeaklyCanonicalPathNtVolumeFallback(const std::filesystem::path& input,
                                         std::filesystem::path& result);

}  // namespace fl::platform::internal

#endif  // _WIN32
