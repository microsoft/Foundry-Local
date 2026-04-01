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
        FoundryLocalManager(FoundryLocalManager&&) = delete;
        FoundryLocalManager& operator=(FoundryLocalManager&&) = delete;

        /// Create the FoundryLocalManager singleton instance.
        /// Throws if an instance has already been created. Call Destroy() first to release the current instance.
        /// @param configuration Configuration to use.
        /// @param logger Optional application logger. Pass nullptr to suppress log output.
        static void Create(Configuration configuration, ILogger* logger = nullptr);

        /// Get the singleton instance.
        /// Throws if Create() has not been called.
        static FoundryLocalManager& Instance();

        /// Returns true if the singleton instance has been created.
        static bool IsInitialized() noexcept;

        /// Destroy the singleton instance, performing deterministic cleanup.
        /// Unloads all loaded models and stops the web service if running.
        /// After this call, Create() may be called again.
        static void Destroy() noexcept;

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
        explicit FoundryLocalManager(Configuration configuration, ILogger* logger);
        ~FoundryLocalManager();

        struct Deleter {
            void operator()(FoundryLocalManager* p) const noexcept { delete p; }
        };

        void Initialize();
        void Cleanup() noexcept;

        static std::unique_ptr<FoundryLocalManager, Deleter> instance_;

        Configuration config_;
        NullLogger defaultLogger_;
        std::unique_ptr<Internal::IFoundryLocalCore> core_;
        std::unique_ptr<Catalog> catalog_;
        ILogger* logger_;
        std::vector<std::string> urls_;
    };

} // namespace foundry_local
