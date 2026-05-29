// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace fl {

struct CodeLocation {
  CodeLocation(const char* file_path, int line, const char* func)
      : file_and_path{file_path}, line_num{line}, function{func} {
  }

  CodeLocation(const char* file_path, int line, const char* func, const std::vector<std::string>& stack)
      : file_and_path{file_path}, line_num{line}, function{func}, stacktrace(stack) {
  }

  std::string FileNoPath() const {
    return file_and_path.substr(file_and_path.find_last_of("/\\") + 1);
  }

  enum class Format {
    kFilename,
    kFilenameAndPath,
  };

  std::string ToString(Format format = Format::kFilename) const {
    std::ostringstream out;
    out << (format == Format::kFilename ? FileNoPath() : file_and_path)
        << ":" << line_num << " " << function;
    return out.str();
  }

  const std::string file_and_path;
  const int line_num;
  const std::string function;
  const std::vector<std::string> stacktrace;
};

}  // namespace fl
