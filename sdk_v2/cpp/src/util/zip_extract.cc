// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "util/zip_extract.h"

#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;  // POSIX global — must be declared outside any namespace
#endif

namespace fl {

bool ExtractZip(const std::filesystem::path& zip_path,
                const std::filesystem::path& destination) {
  if (!std::filesystem::exists(zip_path)) {
    return false;
  }

  std::filesystem::create_directories(destination);

  // Use system tar (available on Windows 10+ and all Linux distros) to avoid
  // adding a minizip/libzip dependency. We bypass the shell to prevent injection.
#ifdef _WIN32
  std::wstring command_line =
      L"tar -xf \"" + zip_path.wstring() + L"\" -C \"" + destination.wstring() + L"\"";

  // CreateProcessW requires a mutable command-line buffer
  std::vector<wchar_t> cmd_buf(command_line.begin(), command_line.end());
  cmd_buf.push_back(L'\0');

  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};

  if (!CreateProcessW(nullptr, cmd_buf.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    return false;
  }

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD exit_code = 1;
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return exit_code == 0;
#else
  std::string zip_str = zip_path.string();
  std::string dest_str = destination.string();

  // posix_spawnp searches PATH for "tar"
  std::vector<char*> argv;
  std::string arg_tar = "tar";
  std::string arg_xf = "-xf";
  std::string arg_c = "-C";
  argv.push_back(arg_tar.data());
  argv.push_back(arg_xf.data());
  argv.push_back(zip_str.data());
  argv.push_back(arg_c.data());
  argv.push_back(dest_str.data());
  argv.push_back(nullptr);

  pid_t pid;
  int spawn_result = posix_spawnp(&pid, "tar", nullptr, nullptr, argv.data(), environ);

  if (spawn_result != 0) {
    return false;
  }

  int status;
  waitpid(pid, &status, 0);

  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

}  // namespace fl
