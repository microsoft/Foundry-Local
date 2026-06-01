// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "util/path_safety.h"

#include "platform/path.h"

#include <string>

namespace fl {

bool IsPathWithinDirectory(const std::filesystem::path& candidate,
                           const std::filesystem::path& root) {
  std::filesystem::path canonical_candidate;
  std::filesystem::path canonical_root;
  std::string error_message;

  if (!platform::GetWeaklyCanonicalPath(candidate, canonical_candidate, error_message)) {
    return false;
  }

  if (!platform::GetWeaklyCanonicalPath(root, canonical_root, error_message)) {
    return false;
  }

  // Compare path components rather than raw strings to avoid trailing-separator
  // and case-sensitivity edge cases (e.g. "/foo/bar" should not be considered
  // inside "/foo/ba"). path::iterator yields one component at a time.
  auto root_it = canonical_root.begin();
  auto root_end = canonical_root.end();
  auto cand_it = canonical_candidate.begin();
  auto cand_end = canonical_candidate.end();

  for (; root_it != root_end; ++root_it, ++cand_it) {
    if (cand_it == cand_end || *cand_it != *root_it) {
      return false;
    }
  }

  return true;
}

}  // namespace fl
