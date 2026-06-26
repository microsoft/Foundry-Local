// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Real-network tests for HttpDownloadFile — the generic HTTPS file downloader used by the
// execution-provider bootstrappers (WebGPU / CUDA). These are DISABLED by default because they
// require network access; the coverage run enables them via --gtest_also_run_disabled_tests.
//
// The positive case downloads the small WebGPU EP manifest JSON (a few hundred bytes) from the
// same CDN that serves the EP packages, rather than a multi-hundred-MB EP zip. That keeps the
// test fast while still exercising the full curl transport + Content-Length + progress + success
// path. The negative case targets an unresolvable host so the failure path is deterministic and
// independent of any server's error-page behavior.
#include "http/http_download.h"

#include "logger.h"

#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace fl;

namespace {

// Mirrors kManifestUrl in src/ep_detection/webgpu_ep_bootstrapper.cc. The manifest is a small
// JSON document served from the same CDN as the EP packages, so it validates the real download
// path without pulling a large binary.
constexpr const char* kWebGpuManifestUrl =
    "https://foundrypackages-ffhrdhbxb7gpdreh.b02.azurefd.net/webgpu_ep_prod.json";

constexpr const char* kUserAgent = "FoundryLocal";

/// Captures log output so a failed download surfaces the downloader's own diagnostics.
class RecordingLogger : public ILogger {
 public:
  void Log(LogLevel level, std::string_view message) override {
    entries.emplace_back(level, std::string(message));
  }

  std::string Dump() const {
    std::ostringstream oss;
    for (const auto& [level, msg] : entries) {
      oss << "  [" << static_cast<int>(level) << "] " << msg << "\n";
    }
    return oss.str();
  }

  std::vector<std::pair<LogLevel, std::string>> entries;
};

/// RAII temp file that removes itself on construction and destruction so each run starts clean.
class TempFile {
 public:
  explicit TempFile(const std::string& name) {
    path_ = fs::temp_directory_path() / name;
    std::error_code ec;
    fs::remove(path_, ec);
  }

  ~TempFile() {
    std::error_code ec;
    fs::remove(path_, ec);
  }

  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

std::string ReadFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

// Downloads the real WebGPU EP manifest and validates the success path end-to-end: returns
// true, writes a non-empty file, reports a terminal 100% progress callback, and the payload is
// the expected JSON shape.
TEST(DISABLED_HttpDownload, DownloadsWebGpuManifest) {
  RecordingLogger logger;
  TempFile dest("fl_webgpu_manifest_test.json");

  std::vector<float> progress;
  std::atomic<bool> cancel{false};

  bool ok = HttpDownloadFile(kWebGpuManifestUrl, dest.path(), kUserAgent, &cancel, [&progress](float pct) { progress.push_back(pct); }, logger);

  ASSERT_TRUE(ok) << "WebGPU manifest download failed. Logger output:\n"
                  << logger.Dump();

  ASSERT_TRUE(fs::exists(dest.path()));
  EXPECT_GT(fs::file_size(dest.path()), 0u);

  // The final progress callback is always 100% on success.
  ASSERT_FALSE(progress.empty());
  EXPECT_FLOAT_EQ(progress.back(), 100.0f);

  // The payload is the EP manifest — parses as JSON and carries a 'packages' object.
  auto manifest = nlohmann::json::parse(ReadFile(dest.path()));
  EXPECT_TRUE(manifest.contains("packages"));
}

// A transport failure (unresolvable host) returns false and leaves no output file behind.
// The reserved `.invalid` TLD (RFC 2606) never resolves, so the DNS failure is deterministic and
// fast regardless of network conditions to real hosts.
TEST(DISABLED_HttpDownload, ReturnsFalseAndWritesNoFileOnUnresolvableHost) {
  RecordingLogger logger;
  TempFile dest("fl_webgpu_manifest_unresolvable.json");

  bool ok = HttpDownloadFile("https://foundry-local-test.invalid/webgpu_ep_prod.json", dest.path(),
                             kUserAgent, /*cancel_flag=*/nullptr, /*progress_cb=*/{}, logger);

  EXPECT_FALSE(ok);
  EXPECT_FALSE(fs::exists(dest.path()));
}
