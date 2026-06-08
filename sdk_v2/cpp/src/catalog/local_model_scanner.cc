// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "catalog/local_model_scanner.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace fl {

namespace fs = std::filesystem;

namespace {

const char* kGenAIConfigFileName = "genai_config.json";
const char* kDownloadSignalFileName = "download.tmp";
const char* kInferenceModelFileName = "inference_model.json";

/// Try to read model metadata from inference_model.json in the given directory.
/// Returns false on failure.
bool ReadModelInfoFromInferenceModel(const fs::path& dir,
                                     std::string& model_name,
                                     LocalModelScanResult& info) {
  auto path = dir / kInferenceModelFileName;
  if (!fs::exists(path)) {
    return false;
  }

  try {
    std::ifstream file(path);
    if (!file.is_open()) {
      return false;
    }

    auto j = nlohmann::json::parse(file);
    if (j.contains("Name") && j["Name"].is_string()) {
      model_name = j["Name"].get<std::string>();
    }

    if (j.contains("toolCallStart") && j["toolCallStart"].is_string()) {
      info.tool_call_start = j["toolCallStart"].get<std::string>();
    }

    if (j.contains("toolCallEnd") && j["toolCallEnd"].is_string()) {
      info.tool_call_end = j["toolCallEnd"].get<std::string>();
    }

    if (j.contains("supportsToolCalling") && j["supportsToolCalling"].is_boolean()) {
      info.supports_tool_calling = j["supportsToolCalling"].get<bool>() ? 1 : 0;
    }
  } catch (...) {
    // Ignore parse errors for individual files.
  }

  return !model_name.empty();
}

/// Check if a directory is a valid cached model directory.
/// Must have genai_config.json, no download.tmp, and an inference_model.json with a Name.
bool IsValidModelDirectory(const fs::path& dir) {
  if (!fs::exists(dir / kGenAIConfigFileName)) {
    return false;
  }

  if (fs::exists(dir / kDownloadSignalFileName)) {
    return false;  // Incomplete download.
  }

  if (!fs::exists(dir / kInferenceModelFileName)) {
    return false;
  }

  return true;
}

/// Recursively scan a directory for valid model directories.
void ScanDirectory(const fs::path& dir,
                   std::map<std::string, LocalModelScanResult>& results,
                   ILogger& logger) {
  try {
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
      return;
    }

    // Check if this directory itself is a valid model directory.
    if (IsValidModelDirectory(dir)) {
      std::string model_name;
      LocalModelScanResult info;
      if (ReadModelInfoFromInferenceModel(dir, model_name, info)) {
        // If the model name doesn't contain a ':' version separator, append ":0".
        if (model_name.find(':') == std::string::npos) {
          model_name += ":0";
        }

        info.path = dir.string();
        results[model_name] = std::move(info);
      }

      // Don't recurse into a valid model directory — it's a leaf.
      return;
    }

    // Recurse into subdirectories.
    for (const auto& entry : fs::directory_iterator(dir)) {
      if (entry.is_directory()) {
        ScanDirectory(entry.path(), results, logger);
      }
    }
  } catch (const std::exception& ex) {
    logger.Log(LogLevel::Warning,
               fmt::format("local model scan: error scanning {} — {}", dir.string(), ex.what()));
  } catch (...) {
    logger.Log(LogLevel::Warning,
               fmt::format("local model scan: unknown error scanning {}", dir.string()));
  }
}

}  // anonymous namespace

std::map<std::string, LocalModelScanResult> ScanLocalModelInfos(const std::string& cache_directory,
                                                               ILogger& logger) {
  std::map<std::string, LocalModelScanResult> results;

  if (cache_directory.empty()) {
    return results;
  }

  fs::path cache_path(cache_directory);
  if (!fs::exists(cache_path) || !fs::is_directory(cache_path)) {
    return results;
  }

  // Scan each top-level subdirectory (publisher directories).
  try {
    for (const auto& entry : fs::directory_iterator(cache_path)) {
      if (entry.is_directory()) {
        ScanDirectory(entry.path(), results, logger);
      }
    }
  } catch (const std::exception& ex) {
    logger.Log(LogLevel::Warning,
               fmt::format("local model scan: error iterating cache directory — {}", ex.what()));
  } catch (...) {
    logger.Log(LogLevel::Warning, "local model scan: unknown error iterating cache directory");
  }

  return results;
}

std::map<std::string, std::string> ScanLocalModels(const std::string& cache_directory,
                                                   ILogger& logger) {
  std::map<std::string, std::string> results;
  for (const auto& [id, info] : ScanLocalModelInfos(cache_directory, logger)) {
    results[id] = info.path;
  }

  return results;
}

}  // namespace fl
