// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "platform/path.h"

#include <system_error>

namespace fl::platform {

bool GetWeaklyCanonicalPath(const std::filesystem::path& input,
                            std::filesystem::path& result,
                            std::string& error_message) {
  std::error_code ec;
  auto canonical = std::filesystem::weakly_canonical(input, ec);
  if (ec) {
    error_message = ec.message();
    return false;
  }

  result = std::move(canonical);
  return true;
}

}  // namespace fl::platform
