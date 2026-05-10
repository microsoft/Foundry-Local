// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Delay-load notification hook for ORT DLLs.
// Redirects onnxruntime.dll and onnxruntime-genai.dll resolution to the
// user-specified path (from FoundryLocalSetRuntimeLibraryPath).
// Falls through to default behavior when no path is set.

#include "util/runtime_library_path.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// delayimp.h must come after windows.h
#include <delayimp.h>

#include <filesystem>
#include <string>

#pragma comment(lib, "delayimp")

namespace {

// Case-insensitive comparison for DLL names
bool EqualsIgnoreCase(const char* a, const char* b) {
  while (*a && *b) {
    if (tolower(static_cast<unsigned char>(*a)) != tolower(static_cast<unsigned char>(*b))) {
      return false;
    }

    ++a;
    ++b;
  }

  return *a == *b;
}

constexpr const char* kOrtDllName = "onnxruntime.dll";
constexpr const char* kGenAiDllName = "onnxruntime-genai.dll";

HMODULE LoadFromPath(const std::string& dir, const char* dll_name) {
  auto full_path = (std::filesystem::path(dir) / dll_name).wstring();
  return LoadLibraryExW(full_path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
}

}  // anonymous namespace

// The delay-load helper calls this hook before loading a DLL.
// Returning a non-null HMODULE overrides the default search.
static FARPROC WINAPI DelayLoadNotifyHook(unsigned dliNotify, PDelayLoadInfo pdli) {
  if (dliNotify != dliNotePreLoadLibrary || pdli == nullptr || pdli->szDll == nullptr) {
    return nullptr;
  }

  const auto& path = fl::GetRuntimeLibraryPath();

  if (path.empty()) {
    // No custom path set — use default DLL search order (co-location works here)
    return nullptr;
  }

  if (EqualsIgnoreCase(pdli->szDll, kOrtDllName)) {
    auto mod = LoadFromPath(path, kOrtDllName);
    if (mod != nullptr) {
      fl::MarkOrtLoaded();
    }

    return reinterpret_cast<FARPROC>(mod);
  }

  if (EqualsIgnoreCase(pdli->szDll, kGenAiDllName)) {
    // Load order contract: onnxruntime.dll must be loaded before onnxruntime-genai.dll.
    // GenAI has a load-time import of ORT — Windows deduplicates by module name,
    // so if ORT is already loaded from our path, GenAI's import resolves to it.
    if (GetModuleHandleA(kOrtDllName) == nullptr) {
      auto ort_mod = LoadFromPath(path, kOrtDllName);

      if (ort_mod == nullptr) {
        // ORT failed to load — GenAI will also fail
        return nullptr;
      }
    }

    auto mod = LoadFromPath(path, kGenAiDllName);
    if (mod != nullptr) {
      fl::MarkOrtLoaded();
    }

    return reinterpret_cast<FARPROC>(mod);
  }

  return nullptr;
}

// Register the hook. The MSVC delay-load helper checks this global function pointer.
extern "C" const PfnDliHook __pfnDliNotifyHook2 = reinterpret_cast<PfnDliHook>(DelayLoadNotifyHook);
