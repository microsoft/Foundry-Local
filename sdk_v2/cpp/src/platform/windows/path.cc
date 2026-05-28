// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "platform/path.h"
#include "platform/windows/path_internal.h"

#define NOMINMAX
#include <windows.h>

#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace fl::platform {

namespace {

constexpr std::wstring_view kGlobalRootPrefix{L"\\\\?\\GLOBALROOT"};

struct HandleCloser {
  using pointer = HANDLE;
  void operator()(HANDLE h) const noexcept {
    if (h != nullptr && h != INVALID_HANDLE_VALUE) {
      ::CloseHandle(h);
    }
  }
};
using UniqueHandle = std::unique_ptr<HANDLE, HandleCloser>;

UniqueHandle OpenHandleForFinalPath(const std::filesystem::path& path) {
  CREATEFILE2_EXTENDED_PARAMETERS params{};
  params.dwSize = sizeof(params);
  params.dwFileFlags = FILE_FLAG_BACKUP_SEMANTICS;
  return UniqueHandle{::CreateFile2(path.c_str(),
                                    FILE_READ_ATTRIBUTES,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    OPEN_EXISTING,
                                    &params)};
}

// Final-path query using VOLUME_NAME_NT, prefixed with "\\?\GLOBALROOT" to stay a valid Win32 path.
bool TryGetFinalPathNt(const std::filesystem::path& path, std::filesystem::path& result) {
  UniqueHandle handle = OpenHandleForFinalPath(path);
  if (!handle || handle.get() == INVALID_HANDLE_VALUE) {
    return false;
  }

  std::wstring buffer(MAX_PATH, L'\0');
  constexpr DWORD kFlags = FILE_NAME_NORMALIZED | VOLUME_NAME_NT;
  DWORD needed = ::GetFinalPathNameByHandleW(handle.get(), buffer.data(),
                                             static_cast<DWORD>(buffer.size()), kFlags);
  if (needed != 0 && needed >= buffer.size()) {
    buffer.resize(needed + 1);
    needed = ::GetFinalPathNameByHandleW(handle.get(), buffer.data(),
                                         static_cast<DWORD>(buffer.size()), kFlags);
  }

  if (needed == 0 || needed > buffer.size()) {
    return false;
  }
  buffer.resize(needed);

  std::wstring prefixed;
  prefixed.reserve(kGlobalRootPrefix.size() + buffer.size());
  prefixed.append(kGlobalRootPrefix);
  prefixed.append(buffer);
  result = std::filesystem::path(std::move(prefixed));
  return true;
}

}  // namespace

namespace internal {

// weakly_canonical analogue using TryGetFinalPathNt for the existing prefix.
bool WeaklyCanonicalPathNtVolumeFallback(const std::filesystem::path& input,
                                         std::filesystem::path& result) {
  std::filesystem::path head = input;
  std::filesystem::path tail;
  std::filesystem::path canonical_head;
  bool found_existing_prefix = false;

  while (true) {
    std::error_code ec;
    const bool exists = std::filesystem::exists(head, ec);
    if (ec) {
      return false;
    }
    if (exists) {
      if (!TryGetFinalPathNt(head, canonical_head)) {
        return false;
      }
      found_existing_prefix = true;
      break;
    }
    if (head.empty()) {
      break;
    }
    const auto parent = head.parent_path();
    if (parent == head) {
      break;
    }
    const auto leaf = head.filename();
    if (!leaf.empty()) {
      // path / empty would insert a trailing separator.
      tail = tail.empty() ? leaf : (leaf / tail);
    }
    head = parent;
  }

  if (!found_existing_prefix) {
    return false;
  }

  if (tail.empty()) {
    result = std::move(canonical_head);
  } else {
    result = (canonical_head / tail).lexically_normal();
  }
  return true;
}

}  // namespace internal

// On AppContainer, std::filesystem::weakly_canonical fails with ERROR_ACCESS_DENIED
// because VOLUME_NAME_DOS goes through the Volume Mount Manager. Fall back to
// VOLUME_NAME_NT, which preserves volume identity (cross-volume escape rejection
// relies on this — do NOT use VOLUME_NAME_NONE).
bool GetWeaklyCanonicalPath(const std::filesystem::path& input,
                            std::filesystem::path& result,
                            std::string& error_message) {
  std::error_code ec;
  auto canonical = std::filesystem::weakly_canonical(input, ec);
  if (!ec) {
    result = std::move(canonical);
    return true;
  }

  if (ec.value() == ERROR_ACCESS_DENIED) {
    std::filesystem::path fallback;
    if (internal::WeaklyCanonicalPathNtVolumeFallback(input, fallback)) {
      result = std::move(fallback);
      return true;
    }
  }

  error_message = ec.message();
  return false;
}

}  // namespace fl::platform
