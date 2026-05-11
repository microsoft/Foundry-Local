// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "manager.h"

#include "catalog.h"
#include "catalog/azure_model_catalog.h"
#include "download/download_manager.h"
#include "ep_detection/cuda_ep_bootstrapper.h"
#include "ep_detection/ep_detector.h"
#include "ep_detection/ep_types.h"
#include "ep_detection/runtime_version_info.h"
#include "ep_detection/webgpu_ep_bootstrapper.h"
#include "exception.h"
#include "inferencing/model_load_manager.h"
#include "inferencing/session/session_manager.h"
#include "spdlog_logger.h"
#include "telemetry/telemetry_action_tracker.h"
#include "telemetry/telemetry_logger.h"
#include "util/runtime_library_path.h"
#include "utils.h"

#include <onnxruntime_c_api.h>

#include <fmt/format.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include "ep_detection/winml_ep_bootstrapper.h"
#endif

#ifdef FOUNDRY_LOCAL_HAS_WEB_SERVICE
#include "service/web_service.h"
#endif

#if defined(__ANDROID__) && !defined(NDEBUG)
#include "platform/ssl_cert_checker.h"
#endif

namespace fl {

#ifdef _WIN32
namespace {

// Get the directory containing foundry_local.dll. Used as the default location
// for co-located ORT DLLs when RuntimeLibraryPath isn't explicitly configured.
std::wstring GetFoundryLocalDllDirectory() {
  HMODULE hmod = nullptr;
  // GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS resolves the module containing this function's code,
  // which is foundry_local.dll (or foundry_local_static.lib linked into the host exe).
  // GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT avoids bumping the DLL refcount.
  if (!GetModuleHandleExW(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          reinterpret_cast<LPCWSTR>(&GetFoundryLocalDllDirectory),
          &hmod)) {
    return {};
  }

  wchar_t path_buf[MAX_PATH];
  DWORD len = GetModuleFileNameW(hmod, path_buf, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return {};
  }

  // Strip the filename to get the directory
  return std::filesystem::path(std::wstring(path_buf, len)).parent_path().wstring();
}

// Eagerly load both ORT DLLs during Manager construction (single-threaded) so the
// delay-load hook doesn't fire during concurrent Model::Load() calls.
// Load order: onnxruntime.dll first, then onnxruntime-genai.dll. GenAI has a
// load-time import on ORT, so ORT must already be resident to prevent Windows
// from picking up a system copy.
void EagerLoadOrtDlls() {
  const auto& runtime_path = GetRuntimeLibraryPath();

  // Resolve the search directory: explicit RuntimeLibraryPath, or the directory
  // containing foundry_local.dll itself (co-location is the default layout).
  std::wstring search_dir;
  if (!runtime_path.empty()) {
    search_dir = std::filesystem::path(runtime_path).wstring();
  } else {
    search_dir = GetFoundryLocalDllDirectory();
  }

  auto load_dll = [&](const char* dll_name) -> HMODULE {
    // Already loaded — nothing to do
    HMODULE existing = GetModuleHandleA(dll_name);
    if (existing != nullptr) {
      return existing;
    }

    if (!search_dir.empty()) {
      auto full_path = (std::filesystem::path(search_dir) / dll_name).wstring();
      return LoadLibraryExW(full_path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    }

    // No directory resolved — fall back to default OS search
    return LoadLibraryA(dll_name);
  };

  // ORT first — GenAI depends on it
  load_dll("onnxruntime.dll");

  // GenAI second — its load-time ORT import resolves to the already-loaded ORT
  if (load_dll("onnxruntime-genai.dll") != nullptr) {
    MarkOrtLoaded();
  }
}

}  // anonymous namespace
#endif

std::mutex Manager::s_mutex_;
std::unique_ptr<Manager> Manager::s_instance_;

Manager::Manager(const Configuration& config)
    : config_(config) {
  config_.Validate();

  logger_ = std::make_unique<SpdlogLogger>(config_.log_level, config_.logs_dir.value_or(""));

#if defined(__ANDROID__) && !defined(NDEBUG)
  CheckSslCertSetup(*logger_);
#endif

  // Set ORT runtime library path before anything touches ORT (model load, EP detection).
  // On Windows this feeds the delay-load hook; on Linux it's stored for future use.
  if (config_.runtime_library_path.has_value() && !config_.runtime_library_path->empty()) {
    if (!SetRuntimeLibraryPath(config_.runtime_library_path->c_str())) {
      logger_->Log(LogLevel::Warning, "RuntimeLibraryPath ignored: ORT libraries already loaded");
    }
  }

  // Eagerly load ORT DLLs so they're resident before any concurrent Model::Load() calls.
  // ORT must be loaded before GenAI — GenAI has a load-time import on ORT, and if ORT
  // isn't already loaded from our path, Windows may resolve it from the system directory.
  // This mirrors the contract in delay_load_hook_windows.cc but runs it deterministically
  // during single-threaded Manager init rather than racing during the first delay-load.
#ifdef _WIN32
  EagerLoadOrtDlls();
#endif

  // Build the EP registration callback. When a bootstrapper successfully
  // prepares an EP, this callback registers it with ORT via the C API.
  // OrtEnv is a singleton — CreateEnv returns the existing instance if GenAI
  // (or any other ORT consumer) already created one.
  const OrtApi* ort_api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
  if (!ort_api) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL, "ORT API not available");
  }

  OrtEnv* ort_env = nullptr;
  {
    OrtStatus* status = ort_api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "foundry_local", &ort_env);
    if (status != nullptr) {
      const char* msg = ort_api->GetErrorMessage(status);
      std::string err = std::string("Failed to create OrtEnv: ") + (msg ? msg : "unknown");
      ort_api->ReleaseStatus(status);
      FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL, err);
    }
  }

  LogRuntimeVersions(*logger_);

  EpRegistrationCallback register_ep = [ort_api, ort_env, &log = *logger_](
                                           const std::string& registration_name,
                                           const std::filesystem::path& library_path) -> bool {
    OrtStatus* status = ort_api->RegisterExecutionProviderLibrary(
        ort_env, registration_name.c_str(), library_path.c_str());
    if (status != nullptr) {
      const char* msg = ort_api->GetErrorMessage(status);
      log.Log(LogLevel::Warning,
              std::string("EP registration: RegisterExecutionProviderLibrary failed for '") +
                  registration_name + "': " + (msg ? msg : "unknown"));
      ort_api->ReleaseStatus(status);
      return false;
    }

    auto version = GetEpVersion(*ort_api, *ort_env, registration_name);
    log.Log(LogLevel::Information,
            std::string("EP registration: '") + registration_name +
                "' registered successfully (library=" + library_path.string() +
                ", version=" + version + ")");
    return true;
  };

  // Discover bootstrappers from available EP sources
  std::vector<std::unique_ptr<IEpBootstrapper>> bootstrappers;

#ifdef _WIN32
  // WinML EPs — enumerate from the OS EP catalog (Windows 11 24H2+)
  auto winml_providers = WinMLEpBootstrapper::DiscoverProviders(register_ep, *logger_);
  for (auto& p : winml_providers) {
    bootstrappers.push_back(std::move(p));
  }
#endif

  if (config_.model_cache_dir.has_value()) {
    // CUDA EP — only if an NVIDIA GPU is detected
    if (CudaEpBootstrapper::HasNvidiaGpu()) {
      auto cuda_ep_dir = *config_.model_cache_dir + "/cuda-ep";
      bootstrappers.push_back(std::make_unique<CudaEpBootstrapper>(std::move(cuda_ep_dir), register_ep));
    }

    // WebGPU EP — always available (no hardware detection needed).
    // Skipped in WinML builds because the WinML-aligned ORT (1.23.2) is older
    // than the ORT API version required by the WebGPU EP plugin (>= 24).
#if !(defined(FOUNDRY_LOCAL_USE_WINML) && FOUNDRY_LOCAL_USE_WINML)
    auto webgpu_ep_dir = *config_.model_cache_dir + "/webgpu-ep";
    bootstrappers.push_back(std::make_unique<WebGpuEpBootstrapper>(std::move(webgpu_ep_dir), register_ep));
#endif
  }

  ep_detector_ = std::make_unique<EpDetector>(*ort_api, *ort_env, std::move(bootstrappers), *logger_);

  // Read configurable download concurrency (default 64)
  int download_concurrency = 64;
  auto it = config_.additional_options.find("NumModelDownloadThreads");
  if (it != config_.additional_options.end()) {
    try {
      download_concurrency = std::stoi(it->second);
      if (download_concurrency < 1) {
        download_concurrency = 1;
      }
    } catch (const std::exception&) {
      // Ignore invalid values, use default
    }
  }

  download_manager_ = std::make_unique<DownloadManager>(
      *config_.model_cache_dir,
      config_.catalog_region.value_or("eastus"),
      download_concurrency);
  model_load_manager_ = std::make_unique<ModelLoadManager>(*ep_detector_, *logger_);
  session_manager_ = std::make_unique<SessionManager>(*logger_);
  telemetry_ = std::make_unique<TelemetryLogger>(config_.app_name, *logger_);
  catalog_ = std::make_unique<AzureModelCatalog>(
      config_.catalog_urls,
      download_manager_->GetCacheDirectory(),
      [this](ModelInfo info, std::string local_path) {
        return CreateModel(std::move(info), std::move(local_path));
      },
      *ep_detector_, *logger_,
      config_.external_service_url.has_value());
}

Manager::~Manager() {
  // Signal subsystems to drain before tearing down infrastructure
  Shutdown();

  // Best-effort stop of web service on destruction
  if (web_service_running_) {
    try {
      StopWebService();
    } catch (const std::exception& e) {
      logger_->Log(LogLevel::Error,
                   std::string("Exception while stopping web service during Manager destruction: ") + e.what());
    } catch (...) {
      // Suppress exceptions during destruction
      logger_->Log(LogLevel::Error, "Unknown exception while stopping web service during Manager destruction.");
    }
  }

  logger_->Log(LogLevel::Information, "Manager is being disposed.");
}

Manager& Manager::Create(const Configuration& config) {
  std::lock_guard<std::mutex> lock(s_mutex_);

  if (s_instance_ != nullptr) {
    FL_LOG_AND_THROW(s_instance_->GetLogger(), FOUNDRY_LOCAL_ERROR_INVALID_USAGE,
                     "Manager already created. Call Destroy() first.");
  }

  // Construct into a local unique_ptr so a throw between construction and the post-init
  // telemetry/log calls cleans up the partially-initialized Manager instead of leaking it.
  // The constructor validates and resolves defaults; if it throws, no Manager exists.
  auto created = std::unique_ptr<Manager>(new Manager(config));

  // Telemetry/log failure during init must not leave the singleton in a half-initialized
  // state: catch and log, then proceed. The Manager itself is fully constructed at this
  // point — only the post-construction signaling can fail, and it's not load-bearing.
  try {
    created->telemetry_->RecordAction(Action::kCoreInitialize, ActionStatus::kSuccess, "", false, 0);
  } catch (const std::exception& ex) {
    created->GetLogger().Log(LogLevel::Error,
                             fmt::format("telemetry RecordAction failed during Create: {}", ex.what()));
  }

  created->GetLogger().Log(LogLevel::Information, "Manager initialized successfully.");

  s_instance_ = std::move(created);
  return *s_instance_;
}

Manager& Manager::Instance() {
  std::lock_guard<std::mutex> lock(s_mutex_);
  if (s_instance_ == nullptr) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_USAGE,
             "Manager not created. Call Create() first. "
             "Manager must remain valid until all Model and Session instances are destroyed.");
  }

  return *s_instance_;
}

void Manager::Destroy() {
  std::lock_guard<std::mutex> lock(s_mutex_);
  s_instance_.reset();
}

ICatalog& Manager::GetCatalog() {
  return *catalog_;
}

void Manager::StartWebService() {
  if (web_service_running_) {
    FL_LOG_AND_THROW(*logger_, FOUNDRY_LOCAL_ERROR_INVALID_USAGE, "web service is already running");
  }

  if (config_.external_service_url.has_value()) {
    FL_LOG_AND_THROW(*logger_, FOUNDRY_LOCAL_ERROR_INVALID_USAGE,
                     "cannot start local web service when external_service_url is configured");
  }

  ActionTracker tracker(Action::kCoreServiceStart, *telemetry_);

#ifdef FOUNDRY_LOCAL_HAS_WEB_SERVICE
  web_service_ = std::make_unique<WebService>(*catalog_, *logger_, *config_.model_cache_dir, *model_load_manager_,
                                              *session_manager_, *telemetry_,
                                              [this]() { Shutdown(); });

  auto& endpoints = config_.web_service_endpoints;
  if (endpoints.empty()) {
    endpoints.push_back("http://127.0.0.1:0");
  }

  bound_urls_ = web_service_->Start(endpoints);
  web_service_running_ = true;
  tracker.SetStatus(ActionStatus::kSuccess);
#else
  FL_LOG_AND_THROW(*logger_, FOUNDRY_LOCAL_ERROR_INVALID_USAGE,
                   "web service requires oatpp (build with FOUNDRY_LOCAL_BUILD_SERVICE=ON)");
#endif
}

const std::vector<std::string>& Manager::GetWebServiceUrls() const {
  if (!web_service_running_) {
    FL_LOG_AND_THROW(*logger_, FOUNDRY_LOCAL_ERROR_INVALID_USAGE, "web service is not running");
  }

  return bound_urls_;
}

void Manager::StopWebService() {
  if (!web_service_running_) {
    FL_LOG_AND_THROW(*logger_, FOUNDRY_LOCAL_ERROR_INVALID_USAGE, "web service is not running");
  }

  ActionTracker tracker(Action::kCoreServiceStop, *telemetry_);

#ifdef FOUNDRY_LOCAL_HAS_WEB_SERVICE
  web_service_->Stop();
  web_service_.reset();
  web_service_running_ = false;
  bound_urls_.clear();
  tracker.SetStatus(ActionStatus::kSuccess);
#else
  FL_LOG_AND_THROW(*logger_, FOUNDRY_LOCAL_ERROR_INVALID_USAGE,
                   "web service requires oatpp (build with FOUNDRY_LOCAL_BUILD_SERVICE=ON)");
#endif
}

void Manager::Shutdown() {
  bool expected = false;
  if (!shutdown_requested_.compare_exchange_strong(expected, true)) {
    return;  // already shutting down
  }

  logger_->Log(LogLevel::Information, "Shutdown requested");

  // Order matters:
  //   1. Reject new loads so callers gated on IsShutdownRequested can stop early.
  //   2. Cancel + drain HTTP-tracked sessions (web service path).
  //   3. Unload all models, polling per-model session refcount for direct-API users
  //      who haven't dropped their flSession* yet. Bounded by timeout so a stuck
  //      caller can't block process shutdown indefinitely.
  model_load_manager_->RejectNewLoads();
  session_manager_->CancelAll();
  session_manager_->WaitForDrain();
  model_load_manager_->UnloadAll();
}

bool Manager::IsShutdownRequested() const {
  return shutdown_requested_.load();
}

const Configuration& Manager::GetConfiguration() const {
  return config_;
}

Model Manager::CreateModel(ModelInfo info, std::string local_path) {
  return Model::FromModelInfo(std::move(info),
                              std::move(local_path),
                              download_manager_.get(),
                              model_load_manager_.get());
}

DownloadManager& Manager::GetDownloadManager() {
  return *download_manager_;
}

ModelLoadManager& Manager::GetModelLoadManager() {
  return *model_load_manager_;
}

SessionManager& Manager::GetSessionManager() {
  return *session_manager_;
}

ILogger& Manager::GetLogger() {
  return *logger_;
}

ITelemetry& Manager::GetTelemetry() {
  return *telemetry_;
}

const IEpDetector& Manager::GetEpDetector() const {
  return *ep_detector_;
}

IEpDetector& Manager::GetEpDetector() {
  return *ep_detector_;
}

}  // namespace fl
