// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "util/path_safety.h"

#include <system_error>

namespace fl {

bool IsPathWithinDirectory(const std::filesystem::path& candidate,
                           const std::filesystem::path& root) {
  std::error_code ec;
  auto canonical_candidate = std::filesystem::weakly_canonical(candidate, ec);
  if (ec) {
    return false;
  }

  auto canonical_root = std::filesystem::weakly_canonical(root, ec);
  if (ec) {
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
