// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "util/stacktrace.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>

#include <mutex>
#include <sstream>
#include <vector>

namespace fl {

std::vector<std::string> GetStackTrace() {
  std::vector<std::string> stack;

#ifndef NDEBUG
  constexpr USHORT kCallstackLimit = 62;
  void* frames[kCallstackLimit];
  USHORT count = CaptureStackBackTrace(1, kCallstackLimit, frames, nullptr);
  if (count == 0) {
    return stack;
  }

  HANDLE process = GetCurrentProcess();
  static std::once_flag init_flag;
  static bool initialized = false;

  std::call_once(init_flag, [process]() {
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    initialized = SymInitialize(process, nullptr, TRUE) == TRUE;
  });

  if (!initialized) {
    return stack;
  }

  stack.reserve(count);

  char symbol_buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(char)] = {};
  auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer);
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = MAX_SYM_NAME;

  for (USHORT index = 0; index < count; ++index) {
    DWORD64 address = reinterpret_cast<DWORD64>(frames[index]);
    DWORD64 displacement64 = 0;
    DWORD displacement32 = 0;

    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    std::ostringstream out;

    if (SymFromAddr(process, address, &displacement64, symbol) == TRUE) {
      out << symbol->Name;
    } else {
      out << "0x" << std::hex << address;
    }

    if (SymGetLineFromAddr64(process, address, &displacement32, &line) == TRUE) {
      out << " at " << line.FileName << ":" << line.LineNumber;
    }

    stack.push_back(out.str());
  }
#endif

  return stack;
}

}  // namespace fl
#endif
