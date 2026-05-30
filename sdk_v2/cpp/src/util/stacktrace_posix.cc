// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "util/stacktrace.h"

#if !defined(__ANDROID__) && !defined(__wasm__) && !defined(_AIX)
#include <execinfo.h>
#endif

#include <cstdlib>
#include <vector>

namespace fl {

std::vector<std::string> GetStackTrace() {
  std::vector<std::string> stack;

#if !defined(NDEBUG) && !defined(__ANDROID__) && !defined(__wasm__) && !defined(_AIX)
  constexpr int kCallstackLimit = 64;

  void* frames[kCallstackLimit];
  int size = backtrace(frames, kCallstackLimit);
  if (size <= 0) {
    return stack;
  }

  char** strings = backtrace_symbols(frames, size);
  if (strings == nullptr) {
    return stack;
  }

  stack.reserve(static_cast<size_t>(size));

  constexpr int start_frame = 1;
  for (int index = start_frame; index < size; ++index) {
    stack.push_back(strings[index]);
  }

  free(strings);
#endif

  return stack;
}

}  // namespace fl
