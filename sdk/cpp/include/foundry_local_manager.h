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
#include "openai/openai_responses_client.h"

namespace foundry_local::Internal {
    struct IFoundryLocalCore;
}

namespace foundry_local {

    class Manager final {
    public:
        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;
        Manager(Manager&&) = delete;
        Manager& operator=(Manager&&) = delete;

        /// Create the Manager singleton instance.
        /// Throws if an instance has already been created. Call Destroy() first to release the current instance.
        /// @param configuration Configuration to use.
        /// @param logger Optional application logger. Pass nullptr to suppress log output.
        static void Create(Configuration configuration, ILogger* logger = nullptr);

        /// Get the singleton instance.
        /// Throws if Create() has not been called.
        static Manager& Instance();

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

        /// Creates an OpenAIResponsesClient for the Responses API.
        /// Requires the web service to be started (StartWebService()).
        /// @param modelId The model ID to use for requests.
        OpenAIResponsesClient CreateResponsesClient(const std::string& modelId) const;

        /// Ensure execution providers are downloaded and registered.
        /// Once downloaded, EPs are not re-downloaded unless a new version is available.
        void EnsureEpsDownloaded() const;

    private:
        explicit Manager(Configuration configuration, ILogger* logger);
        ~Manager();

        struct Deleter {
            void operator()(Manager* p) const noexcept { delete p; }
        };

        void Initialize();
        void Cleanup() noexcept;

        static std::unique_ptr<Manager, Deleter> instance_;

        Configuration config_;
        NullLogger defaultLogger_;
        std::unique_ptr<Internal::IFoundryLocalCore> core_;
        std::unique_ptr<Catalog> catalog_;
        ILogger* logger_;
        std::vector<std::string> urls_;
    };

} // namespace foundry_local
