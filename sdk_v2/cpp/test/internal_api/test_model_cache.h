// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Shared helper for tests that need access to the shared test model cache.
// Mirrors the C# test setup pattern:
//   - Default: look for "test-data-shared" in the parent of the git repo root
//   - Override via TEST_MODEL_CACHE_DIR environment variable (absolute or relative path)
#pragma once

#include "utils/safe_getenv.h"
#include "utils/string_utils.h"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace fl::test {

/// Walk up from source_dir to find the directory containing .git.
inline fs::path FindRepoRoot(const fs::path& source_dir) {
  fs::path dir = source_dir;

  while (!dir.empty() && dir.has_parent_path()) {
    if (fs::exists(dir / ".git")) {
      return dir;
    }

    auto parent = dir.parent_path();
    if (parent == dir) {
      break;
    }

    dir = parent;
  }

  throw std::runtime_error("Could not find git repository root from: " + source_dir.string());
}

/// Default test-data-shared directory name.
constexpr const char* kDefaultTestModelCacheDirName = "test-data-shared";

/// Resolve the test model cache directory.
/// Priority:
///   1. TEST_MODEL_CACHE_DIR environment variable (absolute or relative path)
///   2. Default: {repo_root}/../test-data-shared/
inline fs::path GetTestModelCacheDir() {
  // Check environment variable first
  std::string env_value = SafeGetEnv("TEST_MODEL_CACHE_DIR");
  if (!env_value.empty()) {
    fs::path env_path(env_value);
    if (!fs::exists(env_path)) {
      throw std::runtime_error("TEST_MODEL_CACHE_DIR does not exist: " + env_path.string());
    }

    return fs::canonical(env_path);
  }

  // Default: {repo_root}/../test-data-shared/
  fs::path repo_root = FindRepoRoot(fs::path(__FILE__).parent_path());
  fs::path default_path = repo_root.parent_path() / kDefaultTestModelCacheDirName;

  if (!fs::exists(default_path)) {
    throw std::runtime_error(
        "Test model cache directory not found at default location: " + default_path.string() +
        "\nSet TEST_MODEL_CACHE_DIR environment variable to override.");
  }

  return fs::canonical(default_path);
}

/// Returns true when running under a known CI provider.
/// Mirrors the C# helper: TF_BUILD=True (Azure DevOps) or
/// GITHUB_ACTIONS=true (GitHub Actions), case-insensitive.
inline bool IsRunningInCI() {
  return ToLower(SafeGetEnv("TF_BUILD")) == "true" ||
         ToLower(SafeGetEnv("GITHUB_ACTIONS")) == "true";
}

/// Get the effective model path — the directory containing genai_config.json.
/// ModelLoadManager::LoadModel expects this resolved path (it no longer searches
/// subdirectories). Some models use a variant layout where genai_config.json is
/// in a subdirectory of the model root.
/// @param model_alias  e.g. "qwen2.5-0.5b-instruct-generic-cpu-4"
inline fs::path GetTestModelPath(const std::string& model_alias) {
  fs::path cache_dir = GetTestModelCacheDir();
  fs::path model_dir = cache_dir / model_alias;

  if (!fs::exists(model_dir)) {
    throw std::runtime_error("Test model not found: " + model_dir.string());
  }

  // Check for genai_config.json at the root level.
  constexpr const char* kConfigFile = "genai_config.json";

  if (fs::exists(model_dir / kConfigFile)) {
    return model_dir;
  }

  // Search immediate subdirectories (variant layout).
  for (const auto& entry : fs::directory_iterator(model_dir)) {
    if (entry.is_directory() && fs::exists(entry.path() / kConfigFile)) {
      return fs::canonical(entry.path());
    }
  }

  throw std::runtime_error(
      "Test model does not contain genai_config.json (checked root and subdirectories): " +
      model_dir.string());
}

/// The standard chat model used in tests.
constexpr const char* kTestChatModelAlias = "qwen2.5-0.5b-instruct-generic-cpu-4";

/// The standard audio (whisper) model used in tests.
constexpr const char* kTestAudioModelAlias = "openai-whisper-tiny-generic-cpu-2";

/// Get the path to a file in the test data directory.
/// Uses the compile-time FOUNDRY_LOCAL_TEST_DATA_DIR macro set by CMake.
/// On Android this is the relative path "testdata" (binary runs from the same directory).
/// On desktop this is the absolute path to the build output testdata directory.
inline fs::path GetTestDataPath(const std::string& relative_path) {
#ifdef FOUNDRY_LOCAL_TEST_DATA_DIR
  return fs::path(FOUNDRY_LOCAL_TEST_DATA_DIR) / relative_path;
#else
  // Fallback: assume cwd is the build output directory (CTest sets WORKING_DIRECTORY).
  return fs::path("testdata") / relative_path;
#endif
}

}  // namespace fl::test
