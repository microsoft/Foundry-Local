// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <string>
#include <vector>
#include <memory>

#include <gsl/pointers>
#include <gsl/span>

#include "configuration.h"
#include "logger.h"
#include "catalog.h"

namespace foundry_local::Internal {
    struct IFoundryLocalCore;
}

namespace foundry_local {

    class FoundryLocalManager final {
    public:
        FoundryLocalManager(const FoundryLocalManager&) = delete;
        FoundryLocalManager& operator=(const FoundryLocalManager&) = delete;
        FoundryLocalManager(FoundryLocalManager&& other) noexcept;
        FoundryLocalManager& operator=(FoundryLocalManager&& other) noexcept;

        explicit FoundryLocalManager(Configuration configuration, ILogger* logger = nullptr);
        ~FoundryLocalManager();

        const Catalog& GetCatalog() const;
        Catalog& GetCatalog();

        /// Start the optional built-in web service.
        /// Provides an OpenAI-compatible REST endpoint.
        /// After startup, GetUrls() returns the actual bound URL/s.
        /// Requires Configuration::Web to be set.
        void StartWebService();

        /// Stop the web service if started.
        void StopWebService();

        /// Returns the bound URL/s after StartWebService(), or empty if not started.
        gsl::span<const std::string> GetUrls() const noexcept;

        /// Ensure execution providers are downloaded and registered.
        /// Once downloaded, EPs are not re-downloaded unless a new version is available.
        void EnsureEpsDownloaded() const;

    private:
        bool OwnsLogger() const noexcept { return logger_ == &defaultLogger_; }

        Configuration config_;

        void Initialize();

        NullLogger defaultLogger_;
        std::unique_ptr<Internal::IFoundryLocalCore> core_;
        std::unique_ptr<Catalog> catalog_;
        ILogger* logger_;
        std::vector<std::string> urls_;
    };

} // namespace foundry_local
