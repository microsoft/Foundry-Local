// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <exception>
#include <functional>
#include <utility>

#include <gsl/span>
#include <nlohmann/json.hpp>

#include "foundry_local.h"
#include "foundry_local_internal_core.h"
#include "foundry_local_exception.h"
#include "core_interop_request.h"
#include "core_helpers.h"
#include "core.h"
#include "logger.h"

namespace foundry_local {

namespace {
    std::vector<std::string> GetStringArray(const nlohmann::json& j, const char* key) {
        auto it = j.find(key);
        if (it == j.end() || !it->is_array()) {
            return {};
        }
        return it->get<std::vector<std::string>>();
    }

    std::vector<EpInfo> ParseEpInfoList(const std::string& data, ILogger& logger) {
        if (data.empty()) {
            return {};
        }

        try {
            auto parsed = nlohmann::json::parse(data, nullptr, false);
            if (parsed.is_discarded()) {
                throw Exception("Failed to parse discover_eps response: " + data, logger);
            }
            if (!parsed.is_array()) {
                throw Exception("Expected JSON array from discover_eps, got: " + data, logger);
            }

            std::vector<EpInfo> eps;
            eps.reserve(parsed.size());
            for (const auto& item : parsed) {
                eps.push_back(EpInfo{
                    item.value("Name", std::string{}),
                    item.value("IsRegistered", false)
                });
            }
            return eps;
        }
        catch (const Exception&) {
            throw;
        }
        catch (const nlohmann::json::exception& e) {
            throw Exception("Failed to parse execution provider discovery response: " + std::string(e.what()), logger);
        }
    }

    EpDownloadResult ParseEpDownloadResult(const std::string& data, ILogger& logger) {
        if (data.empty()) {
            return EpDownloadResult{true, "Completed", {}, {}};
        }

        try {
            auto parsed = nlohmann::json::parse(data);
            return EpDownloadResult{
                parsed.value("Success", false),
                parsed.value("Status", std::string{}),
                GetStringArray(parsed, "RegisteredEps"),
                GetStringArray(parsed, "FailedEps")
            };
        }
        catch (const nlohmann::json::exception& e) {
            throw Exception("Failed to parse execution provider download response: " + std::string(e.what()), logger);
        }
    }

    std::string BuildEpDownloadPayload(gsl::span<const std::string> names) {
        if (names.empty()) {
            return {};
        }

        std::string joinedNames;
        for (const auto& name : names) {
            if (!joinedNames.empty()) {
                joinedNames += ',';
            }
            joinedNames += name;
        }

        CoreInteropRequest request("download_and_register_eps");
        request.AddParam("Names", joinedNames);
        return request.ToJson();
    }

    bool TryParseEpProgressPercent(std::string_view percentText, double& percent) {
        auto begin = percentText.data();
        auto end = begin + percentText.size();
        auto [ptr, ec] = std::from_chars(begin, end, percent);
        return ec == std::errc{};
    }
} // namespace

std::unique_ptr<Manager, Manager::Deleter> Manager::instance_;

Manager& Manager::Create(Configuration configuration, ILogger* logger) {
    if (instance_) {
        NullLogger fallback;
        ILogger& log = logger ? *logger : fallback;
        throw Exception("Manager has already been created. Call Destroy() first.", log);
    }

    // Use a local to ensure full initialization before assigning to the static instance.
    std::unique_ptr<Manager, Deleter> manager(
        new Manager(std::move(configuration), logger));
    instance_ = std::move(manager);
    return *instance_;
}

Manager& Manager::Instance() {
    if (!instance_) {
        throw Exception("Manager has not been created. Call Create() first.");
    }
    return *instance_;
}

bool Manager::IsInitialized() noexcept {
    return instance_ != nullptr;
}

void Manager::Destroy() noexcept {
    instance_.reset();
}

Manager::Manager(Configuration configuration, ILogger* logger)
    : config_(std::move(configuration)), core_(std::make_unique<Core>()),
      logger_(logger ? logger : &defaultLogger_) {
    static_cast<Core*>(core_.get())->LoadEmbedded();
    Initialize();
    catalog_ = Catalog::Create(core_.get(), logger_);
}

Manager::~Manager() {
    Cleanup();
}

void Manager::Cleanup() noexcept {
        // Unload all loaded models before tearing down.
        if (catalog_) {
            try {
                auto loadedModels = catalog_->GetLoadedModels();
                for (auto* variant : loadedModels) {
                    try {
                        variant->Unload();
                    }
                    catch (const std::exception& ex) {
                        logger_->Log(LogLevel::Warning,
                                     std::string("Error unloading model during destruction: ") + ex.what());
                    }
                }
            }
            catch (const std::exception& ex) {
                logger_->Log(LogLevel::Warning,
                             std::string("Error retrieving loaded models during destruction: ") + ex.what());
            }
        }

        if (!urls_.empty()) {
            try {
                StopWebService();
            }
            catch (const std::exception& ex) {
                logger_->Log(LogLevel::Warning,
                             std::string("Error stopping web service during destruction: ") + ex.what());
            }
        }
    }

    const Catalog& Manager::GetCatalog() const {
        return *catalog_;
    }

    Catalog& Manager::GetCatalog() {
        return *catalog_;
    }

    void Manager::StartWebService() {
        if (!config_.web) {
            throw Exception("Web service configuration was not provided.", *logger_);
        }

        auto response = core_->call("start_service", *logger_);
        if (response.HasError()) {
            throw Exception(std::string("Error starting web service: ") + response.error, *logger_);
        }
        auto arr = nlohmann::json::parse(response.data);
        urls_ = arr.get<std::vector<std::string>>();
    }

    void Manager::StopWebService() {
        if (!config_.web) {
            throw Exception("Web service configuration was not provided.", *logger_);
        }

        auto response = core_->call("stop_service", *logger_);
        if (response.HasError()) {
            throw Exception(std::string("Error stopping web service: ") + response.error, *logger_);
        }
        urls_.clear();
    }

    gsl::span<const std::string> Manager::GetWebServiceEndpoints() const noexcept {
        return urls_;
    }

    void Manager::EnsureEpsDownloaded() const {
        auto result = DownloadAndRegisterEps();
        if (!result.success) {
            throw Exception(std::string("Error ensuring execution providers downloaded: ") + result.status, *logger_);
        }
    }

    std::vector<EpInfo> Manager::DiscoverEps() const {
        auto response = core_->call("discover_eps", *logger_);
        if (response.HasError()) {
            throw Exception(std::string("Error discovering execution providers: ") + response.error, *logger_);
        }

        return ParseEpInfoList(response.data, *logger_);
    }

    EpDownloadResult Manager::DownloadAndRegisterEps(
        EpProgressCallback progressCallback,
        CancellationCallback isCancellationRequested) const {
        return DownloadAndRegisterEps(std::vector<std::string>{}, std::move(progressCallback),
                                      std::move(isCancellationRequested));
    }

    EpDownloadResult Manager::DownloadAndRegisterEps(
        const std::vector<std::string>& names,
        EpProgressCallback progressCallback,
        CancellationCallback isCancellationRequested) const {
        auto payload = BuildEpDownloadPayload(gsl::span<const std::string>(names));
        const std::string* payloadPtr = payload.empty() ? nullptr : &payload;

        CoreResponse response;
        if (progressCallback || isCancellationRequested) {
            auto onChunk = [&progressCallback](const std::string& chunk) {
                if (!progressCallback) {
                    return;
                }

                const auto sep = chunk.find('|');
                if (sep == std::string::npos) {
                    return;
                }

                double percent = 0.0;
                if (detail::TryParseDoubleToken(std::string_view(chunk).substr(sep + 1), percent)) {
                    progressCallback(chunk.substr(0, sep), percent);
                }
            };

            response = detail::CallWithStreamingCallback(core_.get(), "download_and_register_eps",
                                                         requestDataPtr, *logger_, onChunk,
                                                         "Error downloading execution providers: ",
                                                         std::move(isCancellationRequested));
        }
        else {
            response = core_->call("download_and_register_eps", *logger_, requestDataPtr);
            if (response.HasError()) {
                throw Exception(std::string("Error downloading execution providers: ") + response.error, *logger_);
            }
        }

        auto result = ParseEpDownloadResult(response.data, *logger_);
        if ((result.success || !result.registered_eps.empty()) && catalog_) {
            catalog_->InvalidateCache();
        }
        return result;
    }

    EpDownloadResult Manager::DownloadAndRegisterEps(gsl::span<const std::string> names,
                                                     EpProgressCallback progressCallback) const {
        return DownloadAndRegisterEps(std::vector<std::string>(names.begin(), names.end()),
                                      std::move(progressCallback), nullptr);
    }

    void Manager::Initialize() {
        config_.Validate();

        CoreInteropRequest initReq("initialize");
        initReq.AddParam("AppName", config_.app_name);
        initReq.AddParam("LogLevel", std::string(LogLevelToString(config_.log_level)));

        if (config_.app_data_dir) {
            initReq.AddParam("AppDataDir", config_.app_data_dir->string());
        }
        if (config_.model_cache_dir) {
            initReq.AddParam("ModelCacheDir", config_.model_cache_dir->string());
        }
        if (config_.logs_dir) {
            initReq.AddParam("LogsDir", config_.logs_dir->string());
        }
        if (config_.web && config_.web->urls) {
            initReq.AddParam("WebServiceUrls", *config_.web->urls);
        }
        if (config_.additional_settings) {
            for (const auto& [key, value] : *config_.additional_settings) {
                if (!key.empty()) {
                    initReq.AddParam(key, value);
                }
            }
        }

        std::string initJson = initReq.ToJson();
        auto initResponse = core_->call(initReq.Command(), *logger_, &initJson);
        if (initResponse.HasError()) {
            throw Exception(std::string("Manager::Initialize failed: ") + initResponse.error, *logger_);
        }

        if (config_.model_cache_dir) {
            auto cacheResponse = core_->call("get_cache_directory", *logger_);
            if (cacheResponse.HasError()) {
                throw Exception(std::string("Manager::Initialize failed: ") + cacheResponse.error,
                                *logger_);
            }

            if (cacheResponse.data != config_.model_cache_dir->string()) {
                CoreInteropRequest setReq("set_cache_directory");
                setReq.AddParam("Directory", config_.model_cache_dir->string());
                std::string setJson = setReq.ToJson();
                auto setResponse = core_->call(setReq.Command(), *logger_, &setJson);
                if (setResponse.HasError()) {
                    throw Exception(std::string("Manager::Initialize failed: ") + setResponse.error,
                                    *logger_);
                }
            }
        }
    }

} // namespace foundry_local
