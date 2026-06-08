// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <filesystem>

namespace fl {

/// Returns true if `candidate` resolves to a path inside (or equal to) `root`.
/// Uses fl::platform::GetWeaklyCanonicalPath so the paths need not exist on
/// disk. This is used to defend against path-traversal where untrusted input
/// contains `..` segments or absolute paths that would escape the destination
/// directory.
bool IsPathWithinDirectory(const std::filesystem::path& candidate,
                           const std::filesystem::path& root);

}  // namespace fl
