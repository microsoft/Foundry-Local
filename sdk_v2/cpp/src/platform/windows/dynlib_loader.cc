// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "platform/dynlib_loader.h"
#include "logger.h"

#include <fmt/format.h>

#include <memory>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace fl::platform {
namespace {

constexpr DWORD kLibrarySearchFlags = LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR;

std::shared_ptr<void> OwnLibrary(HMODULE handle) {
  return std::shared_ptr<void>(handle, [](void* h) { ::FreeLibrary(static_cast<HMODULE>(h)); });
}

std::string PathForLog(const std::filesystem::path& path) {
  const auto utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

std::vector<std::filesystem::path> GetPathDirectories(ILogger& logger) {
  std::wstring value;
  DWORD size = ::GetEnvironmentVariableW(L"PATH", nullptr, 0);
  if (size == 0) {
    const DWORD error = ::GetLastError();
    if (error != ERROR_ENVVAR_NOT_FOUND && error != ERROR_SUCCESS) {
      logger.Log(LogLevel::Warning, fmt::format("EP: failed to read PATH ({})", error));
    }

    return {};
  }

  while (size != 0) {
    value.resize(size);
    const DWORD copied = ::GetEnvironmentVariableW(L"PATH", value.data(), size);
    if (copied >= size) {
      size = copied + 1;
      continue;
    }

    if (copied == 0 && ::GetLastError() != ERROR_ENVVAR_NOT_FOUND && ::GetLastError() != ERROR_SUCCESS) {
      logger.Log(LogLevel::Warning, fmt::format("EP: failed to read PATH ({})", ::GetLastError()));
    }

    value.resize(copied);
    break;
  }

  std::vector<std::filesystem::path> directories;
  size_t start = 0;
  while (start < value.size()) {
    const auto end = value.find(L';', start);
    auto entry = value.substr(start, end == std::wstring::npos ? end : end - start);
    if (entry.size() >= 2 && entry.front() == L'"' && entry.back() == L'"') {
      entry = entry.substr(1, entry.size() - 2);
    }

    std::filesystem::path directory(entry);
    if (directory.is_absolute()) {
      directories.push_back(std::move(directory));
    }

    if (end == std::wstring::npos) break;
    start = end + 1;
  }

  return directories;
}

std::shared_ptr<void> LoadDependency(const std::filesystem::path& directory, std::wstring_view name,
                                     const std::vector<std::filesystem::path>& path_directories, ILogger& logger) {
  const auto sibling = directory / name;
  if (HMODULE handle = ::LoadLibraryExW(sibling.c_str(), nullptr, kLibrarySearchFlags)) {
    return OwnLibrary(handle);
  }

  const std::wstring basename(name);
  if (HMODULE handle = ::LoadLibraryExW(basename.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)) {
    return OwnLibrary(handle);
  }

  for (const auto& path_directory : path_directories) {
    const auto candidate = path_directory / name;
    if (HMODULE handle = ::LoadLibraryExW(candidate.c_str(), nullptr, kLibrarySearchFlags)) {
      auto library = OwnLibrary(handle);
      logger.Log(LogLevel::Information,
                 fmt::format("EP: loaded CUDA dependency from PATH: '{}'", PathForLog(candidate)));
      return library;
    }

    const DWORD error = ::GetLastError();
    if (error != ERROR_MOD_NOT_FOUND || ::GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) {
      logger.Log(LogLevel::Warning,
                 fmt::format("EP: failed to load dependency '{}' ({})", PathForLog(candidate), error));
      return {};
    }
  }

  logger.Log(LogLevel::Warning,
             fmt::format("EP: dependency '{}' was not found in the provider directory, configured DLL directories, "
                         "or absolute PATH entries",
                         std::filesystem::path(name).string()));
  return {};
}

}  // namespace

bool SetDynamicLibrarySearchDirectory(const std::filesystem::path& directory, ILogger& logger) {
  const auto absolute_directory = std::filesystem::absolute(directory).lexically_normal();
  if (SetDllDirectoryW(absolute_directory.c_str())) {
    return true;
  }

  logger.Log(LogLevel::Warning,
             fmt::format("Failed to set DLL search directory '{}' ({})", absolute_directory.string(), GetLastError()));
  return false;
}

std::shared_ptr<void> LoadSharedLibrary(const std::filesystem::path& path, fl::ILogger& logger) {
  const auto absolute_path = std::filesystem::absolute(path).lexically_normal();
  // LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR lets the library resolve its sibling dependencies (the CUDA
  // runtime + provider DLLs staged alongside it) from its own directory; DEFAULT_DIRS keeps System32
  // and the process directories (where onnxruntime.dll is already resident) on the search path.
  HMODULE handle = ::LoadLibraryExW(absolute_path.c_str(), nullptr, kLibrarySearchFlags);
  if (handle == nullptr) {
    const DWORD error = ::GetLastError();
    logger.Log(LogLevel::Warning,
               fmt::format("EP: failed to load '{}' ({})", PathForLog(absolute_path), error));
    return {};
  }

  return OwnLibrary(handle);
}

std::shared_ptr<void> LoadSharedLibraryWithDependencies(const std::filesystem::path& path,
                                                        std::span<const std::wstring_view> dependencies,
                                                        ILogger& logger) {
  const auto absolute_path = std::filesystem::absolute(path).lexically_normal();
  const auto path_directories = GetPathDirectories(logger);
  std::vector<std::shared_ptr<void>> loaded_dependencies;
  for (const auto name : dependencies) {
    auto dependency = LoadDependency(absolute_path.parent_path(), name, path_directories, logger);
    if (!dependency) return {};
    loaded_dependencies.push_back(std::move(dependency));
  }

  auto library = LoadSharedLibrary(absolute_path, logger);
  if (!library) return {};

  void* handle = library.get();
  return std::shared_ptr<void>(handle, [library = std::move(library),
                                        dependencies = std::move(loaded_dependencies)](void*) mutable {
    library.reset();
    dependencies.clear();
  });
}

}  // namespace fl::platform
