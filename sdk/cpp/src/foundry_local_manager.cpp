// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include <string_view>
#include <vector>
#include <memory>

#include <gsl/span>

#include "foundry_local.h"
#include "foundry_local_internal_core.h"
#include "foundry_local_exception.h"
#include "core_interop_request.h"
#include "core.h"
#include "logger.h"

namespace foundry_local {

FoundryLocalManager::FoundryLocalManager(Configuration configuration, ILogger* logger)
    : config_(std::move(configuration)), core_(std::make_unique<Core>()),
      logger_(logger ? logger : &defaultLogger_) {
    static_cast<Core*>(core_.get())->loadEmbedded();
    Initialize();
    catalog_ = Catalog::Create(core_.get(), logger_);
}

FoundryLocalManager::FoundryLocalManager(FoundryLocalManager&& other) noexcept
    : config_(std::move(other.config_)), core_(std::move(other.core_)), catalog_(std::move(other.catalog_)),
      logger_(other.OwnsLogger() ? &defaultLogger_ : other.logger_), urls_(std::move(other.urls_)) {
    other.logger_ = &other.defaultLogger_;
}

FoundryLocalManager& FoundryLocalManager::operator=(FoundryLocalManager&& other) noexcept {
    if (this != &other) {
        config_ = std::move(other.config_);
        core_ = std::move(other.core_);
        catalog_ = std::move(other.catalog_);
        logger_ = other.OwnsLogger() ? &defaultLogger_ : other.logger_;
        urls_ = std::move(other.urls_);
        other.logger_ = &other.defaultLogger_;
    }
    return *this;
}

FoundryLocalManager::~FoundryLocalManager() {
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

const Catalog& FoundryLocalManager::GetCatalog() const {
    return *catalog_;
}

Catalog& FoundryLocalManager::GetCatalog() {
    return *catalog_;
}

void FoundryLocalManager::StartWebService() {
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

void FoundryLocalManager::StopWebService() {
    if (!config_.web) {
        throw Exception("Web service configuration was not provided.", *logger_);
    }

    auto response = core_->call("stop_service", *logger_);
    if (response.HasError()) {
        throw Exception(std::string("Error stopping web service: ") + response.error, *logger_);
    }
    urls_.clear();
}

gsl::span<const std::string> FoundryLocalManager::GetUrls() const noexcept {
    return urls_;
}

void FoundryLocalManager::EnsureEpsDownloaded() const {
    auto response = core_->call("ensure_eps_downloaded", *logger_);
    if (response.HasError()) {
        throw Exception(std::string("Error ensuring execution providers downloaded: ") + response.error,
                        *logger_);
    }
}

void FoundryLocalManager::Initialize() {
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
        throw Exception(std::string("FoundryLocalManager::Initialize failed: ") + initResponse.error, *logger_);
    }

    if (config_.model_cache_dir) {
        auto cacheResponse = core_->call("get_cache_directory", *logger_);
        if (cacheResponse.HasError()) {
            throw Exception(std::string("FoundryLocalManager::Initialize failed: ") + cacheResponse.error, *logger_);
        }

        if (cacheResponse.data != config_.model_cache_dir->string()) {
            CoreInteropRequest setReq("set_cache_directory");
            setReq.AddParam("Directory", config_.model_cache_dir->string());
            std::string setJson = setReq.ToJson();
            auto setResponse = core_->call(setReq.Command(), *logger_, &setJson);
            if (setResponse.HasError()) {
                throw Exception(std::string("FoundryLocalManager::Initialize failed: ") + setResponse.error,
                                *logger_);
            }
        }
    }
}

} // namespace foundry_local
