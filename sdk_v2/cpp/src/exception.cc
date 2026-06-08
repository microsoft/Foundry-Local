// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "exception.h"

#include <sstream>

namespace fl {

namespace {

std::string FormatExceptionMessage(const CodeLocation& location, const std::string& message) {
  std::ostringstream out;
  out << location.ToString() << " " << message;

  if (!location.stacktrace.empty()) {
    out << "\nStacktrace:";
    for (const auto& frame : location.stacktrace) {
      out << "\n  " << frame;
    }
  }

  return out.str();
}

}  // namespace

Exception::Exception(const CodeLocation& location, const std::string& message, flErrorCode code)
    : std::runtime_error(FormatExceptionMessage(location, message)), code_(code) {}

}  // namespace fl
