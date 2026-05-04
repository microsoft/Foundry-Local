// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include <string_view>
#include <vector>
#include <memory>
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

    std::vector<EpInfo> Manager::DiscoverEps() const {
        auto response = core_->call("discover_eps", *logger_);
        if (response.HasError()) {
            throw Exception(std::string("Error discovering execution providers: ") + response.error, *logger_);
        }

        std::vector<EpInfo> result;
        if (response.data.empty()) {
            return result;
        }

        auto json = nlohmann::json::parse(response.data, nullptr, false);
        if (json.is_discarded()) {
            throw Exception(
                std::string("Failed to parse discover_eps response: ") + response.data, *logger_);
        }
        if (!json.is_array()) {
            throw Exception(
                std::string("Expected JSON array from discover_eps, got: ") + response.data, *logger_);
        }

        result.reserve(json.size());
        for (const auto& item : json) {
            EpInfo ep;
            ep.name = item.value("Name", "");
            ep.is_registered = item.value("IsRegistered", false);
            result.push_back(std::move(ep));
        }
        return result;
    }

    EpDownloadResult Manager::DownloadAndRegisterEps(EpProgressCallback progressCallback,
                                                     CancellationCallback isCancellationRequested) const {
        return DownloadAndRegisterEps({}, std::move(progressCallback), std::move(isCancellationRequested));
    }

    EpDownloadResult Manager::DownloadAndRegisterEps(const std::vector<std::string>& names,
                                                      EpProgressCallback progressCallback,
                                                      CancellationCallback isCancellationRequested) const {
        std::string requestData;
        std::string* requestDataPtr = nullptr;

        if (!names.empty()) {
            CoreInteropRequest request("download_and_register_eps");
            std::string namesList;
            for (size_t i = 0; i < names.size(); ++i) {
                if (i > 0) namesList += ",";
                namesList += names[i];
            }
            request.AddParam("Names", namesList);
            requestData = request.ToJson();
            requestDataPtr = &requestData;
        }

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
        } else {
            response = core_->call("download_and_register_eps", *logger_, requestDataPtr);
            if (response.HasError()) {
                throw Exception(std::string("Error downloading execution providers: ") + response.error, *logger_);
            }
        }

        EpDownloadResult result;
        if (!response.data.empty()) {
            auto json = nlohmann::json::parse(response.data, nullptr, false);
            if (json.is_discarded()) {
                throw Exception(
                    std::string("Failed to parse download_and_register_eps response: ") + response.data, *logger_);
            }
            result.success = json.value("Success", false);
            result.status = json.value("Status", "");
            if (json.contains("RegisteredEps") && json["RegisteredEps"].is_array()) {
                for (const auto& ep : json["RegisteredEps"]) {
                    result.registered_eps.push_back(ep.get<std::string>());
                }
            }
            if (json.contains("FailedEps") && json["FailedEps"].is_array()) {
                for (const auto& ep : json["FailedEps"]) {
                    result.failed_eps.push_back(ep.get<std::string>());
                }
            }
        } else {
            result.success = true;
            result.status = "Completed";
        }

        // Invalidate the catalog cache so the next access re-fetches models
        // with the updated set of available EPs.
        if (result.success || !result.registered_eps.empty()) {
            catalog_->InvalidateCache();
        }

        return result;
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
