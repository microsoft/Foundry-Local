// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "util/zip_extract.h"

#include "logger.h"

#include <fmt/format.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;  // POSIX global — must be declared outside any namespace
#endif

namespace fl {

bool IsSafeArchiveEntry(std::string_view entry) {
  if (entry.empty()) {
    return true;
  }

  // Reject absolute POSIX paths.
  if (entry.front() == '/') {
    return false;
  }

  // Reject leading backslash (Windows root-relative).
  if (entry.front() == '\\') {
    return false;
  }

  // Reject Windows drive-letter prefix ("X:..."). Defensive: also reject any
  // ':' that isn't in the drive-letter position, since archive entries should
  // never contain ':'.
  for (size_t i = 0; i < entry.size(); ++i) {
    if (entry[i] != ':') {
      continue;
    }

    // Allow exactly the drive-letter form at position 1 (e.g. "C:") — but we
    // still reject the entry overall because it implies an absolute path.
    return false;
  }

  // Split on both '/' and '\\' and reject any literal ".." component.
  size_t start = 0;
  for (size_t i = 0; i <= entry.size(); ++i) {
    bool is_sep = (i == entry.size()) || entry[i] == '/' || entry[i] == '\\';
    if (!is_sep) {
      continue;
    }

    auto component = entry.substr(start, i - start);
    if (component == "..") {
      return false;
    }

    start = i + 1;
  }

  return true;
}

namespace {

#ifdef _WIN32

/// Run `tar -tf <archive>` and return the captured stdout. Returns false on
/// failure to spawn or on non-zero exit. Bypasses the shell to avoid injection.
/// On failure, emits a diagnostic via `logger`.
bool RunTarList(const std::filesystem::path& zip_path, std::string& out_stdout, ILogger& logger) {
  // Create an anonymous pipe; child inherits the write end as stdout.
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;

  HANDLE read_h = nullptr;
  HANDLE write_h = nullptr;
  if (!CreatePipe(&read_h, &write_h, &sa, 0)) {
    logger.Log(LogLevel::Warning,
               fmt::format("ExtractZip: CreatePipe failed (GetLastError={})", GetLastError()));
    return false;
  }

  // The read end must NOT be inherited by the child.
  SetHandleInformation(read_h, HANDLE_FLAG_INHERIT, 0);

  std::wstring command_line = L"tar -tf \"" + zip_path.wstring() + L"\"";
  std::vector<wchar_t> cmd_buf(command_line.begin(), command_line.end());
  cmd_buf.push_back(L'\0');

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = write_h;
  si.hStdError = write_h;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  PROCESS_INFORMATION pi{};

  BOOL ok = CreateProcessW(nullptr, cmd_buf.data(), nullptr, nullptr, TRUE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

  // Close the write end in the parent so ReadFile returns EOF when the child exits.
  CloseHandle(write_h);

  if (!ok) {
    DWORD err = GetLastError();
    CloseHandle(read_h);
    logger.Log(LogLevel::Warning,
               fmt::format("ExtractZip: failed to spawn `tar -tf` for '{}' (GetLastError={})",
                           zip_path.string(), err));
    return false;
  }

  // Drain the pipe into out_stdout.
  std::string buf;
  char chunk[4096];
  DWORD bytes_read = 0;
  while (ReadFile(read_h, chunk, sizeof(chunk), &bytes_read, nullptr) && bytes_read > 0) {
    buf.append(chunk, bytes_read);
  }
  CloseHandle(read_h);

  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD exit_code = 1;
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  if (exit_code != 0) {
    logger.Log(LogLevel::Warning,
               fmt::format("ExtractZip: `tar -tf` for '{}' exited {} \u2014 output: {}",
                           zip_path.string(), exit_code, buf));
    return false;
  }

  out_stdout = std::move(buf);
  return true;
}

#else

bool RunTarList(const std::filesystem::path& zip_path, std::string& out_stdout, ILogger& logger) {
  int pipe_fds[2];
  if (pipe(pipe_fds) != 0) {
    logger.Log(LogLevel::Warning,
               fmt::format("ExtractZip: pipe() failed (errno={})", errno));
    return false;
  }

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  // Child: close the read end; route stdout/stderr to the write end; close write fd after dup.
  posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);
  posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDERR_FILENO);
  posix_spawn_file_actions_addclose(&actions, pipe_fds[1]);

  std::string zip_str = zip_path.string();
  std::vector<char*> argv;
  std::string arg_tar = "tar";
  std::string arg_tf = "-tf";
  argv.push_back(arg_tar.data());
  argv.push_back(arg_tf.data());
  argv.push_back(zip_str.data());
  argv.push_back(nullptr);

  pid_t pid;
  int spawn_result = posix_spawnp(&pid, "tar", &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);

  // Parent closes the write end so read returns EOF when child exits.
  close(pipe_fds[1]);

  if (spawn_result != 0) {
    close(pipe_fds[0]);
    logger.Log(LogLevel::Warning,
               fmt::format("ExtractZip: posix_spawnp('tar') failed for '{}' (errno={})",
                           zip_str, spawn_result));
    return false;
  }

  std::string buf;
  char chunk[4096];
  ssize_t n = 0;
  while ((n = read(pipe_fds[0], chunk, sizeof(chunk))) > 0) {
    buf.append(chunk, static_cast<size_t>(n));
  }
  close(pipe_fds[0]);

  int status = 0;
  waitpid(pid, &status, 0);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    logger.Log(LogLevel::Warning,
               fmt::format("ExtractZip: `tar -tf` for '{}' exited abnormally (status={}) \u2014 output: {}",
                           zip_str, status, buf));
    return false;
  }

  out_stdout = std::move(buf);
  return true;
}

#endif

/// Validate every entry in `listing` (newline-separated). Returns true if all
/// entries are safe; on failure populates `bad_entry` with the offending name.
bool ValidateTarListing(std::string_view listing, std::string& bad_entry) {
  size_t pos = 0;
  while (pos < listing.size()) {
    size_t nl = listing.find('\n', pos);
    if (nl == std::string_view::npos) {
      nl = listing.size();
    }

    auto line = listing.substr(pos, nl - pos);

    // Trim trailing CR for Windows tar output.
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }

    if (!line.empty() && !IsSafeArchiveEntry(line)) {
      bad_entry.assign(line.data(), line.size());
      return false;
    }

    pos = nl + 1;
  }

  return true;
}

}  // namespace

bool ExtractZip(const std::filesystem::path& zip_path,
                const std::filesystem::path& destination,
                ILogger& logger) {
  if (!std::filesystem::exists(zip_path)) {
    logger.Log(LogLevel::Warning,
               fmt::format("ExtractZip: archive does not exist: '{}'", zip_path.string()));
    return false;
  }

  std::filesystem::create_directories(destination);

  // Zip-slip defense: list the archive contents first and reject any entry
  // whose path would escape the destination. Only proceed with extraction
  // if every entry is safe. Since downloads are rare and Microsoft-controlled
  // (EP runtime zips over HTTPS) the cost of an extra `tar -tf` is negligible.
  std::string listing;
  if (!RunTarList(zip_path, listing, logger)) {
    return false;
  }

  std::string bad_entry;
  if (!ValidateTarListing(listing, bad_entry)) {
    logger.Log(LogLevel::Warning,
               fmt::format("ExtractZip: refusing to extract '{}' \u2014 unsafe archive entry: '{}'",
                           zip_path.string(), bad_entry));
    // Refuse to extract. Caller logs a generic extraction-failed message;
    // we deliberately do not throw to preserve the existing bool contract.
    return false;
  }

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
    logger.Log(LogLevel::Warning,
               fmt::format("ExtractZip: failed to spawn `tar -xf` for '{}' (GetLastError={})",
                           zip_path.string(), GetLastError()));
    return false;
  }

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD exit_code = 1;
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  if (exit_code != 0) {
    logger.Log(LogLevel::Warning,
               fmt::format("ExtractZip: `tar -xf` for '{}' \u2192 '{}' exited {}",
                           zip_path.string(), destination.string(), exit_code));
    return false;
  }

  return true;
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
    logger.Log(LogLevel::Warning,
               fmt::format("ExtractZip: posix_spawnp('tar') failed for '{}' (errno={})",
                           zip_str, spawn_result));
    return false;
  }

  int status;
  waitpid(pid, &status, 0);

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    logger.Log(LogLevel::Warning,
               fmt::format("ExtractZip: `tar -xf` for '{}' \u2192 '{}' exited abnormally (status={})",
                           zip_str, dest_str, status));
    return false;
  }

  return true;
#endif
}

}  // namespace fl
