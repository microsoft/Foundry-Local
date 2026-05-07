// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>

#include <gsl/pointers>
#include <gsl/span>

#include "configuration.h"
#include "logger.h"
#include "catalog.h"

namespace foundry_local::Internal {
    struct IFoundryLocalCore;
}

namespace foundry_local {

    /// Information about a discoverable execution provider.
    struct EpInfo {
        std::string name;
        bool is_registered = false;
    };

    /// Result of an EP download and registration operation.
    struct EpDownloadResult {
        bool success = false;
        std::string status;
        std::vector<std::string> registered_eps;
        std::vector<std::string> failed_eps;
    };

    /// Callback for EP download progress. Parameters: (ep_name, percent 0-100).
    using EpProgressCallback = std::function<void(const std::string& ep_name, double percent)>;

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
        static Manager& Create(Configuration configuration, ILogger* logger = nullptr);

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

        /// Start the embedded web service.
        /// Requires Configuration::web to be set.
        void StartWebService();

        /// Stop the embedded web service.
        /// Requires Configuration::web to be set.
        void StopWebService();

        /// Get the URLs the web service is bound to. Valid after StartWebService() and until StopWebService().
        gsl::span<const std::string> GetWebServiceEndpoints() const noexcept;

        /// Ensure execution providers are downloaded and registered.
        /// Once downloaded, EPs are not re-downloaded unless a new version is available.
        void EnsureEpsDownloaded() const;

        /// Discover available execution providers and their registration status.
        /// @return Vector of EpInfo describing each available EP.
        std::vector<EpInfo> DiscoverEps() const;

        /// Download and register all available execution providers.
        /// @param progressCallback Optional callback invoked with (ep_name, percent) during download.
        /// @return Result describing which EPs were registered or failed.
        EpDownloadResult DownloadAndRegisterEps(EpProgressCallback progressCallback = nullptr) const;

        /// Download and register specific execution providers by name.
        /// @param names EP names to download (as returned by DiscoverEps).
        /// @param progressCallback Optional callback invoked with (ep_name, percent) during download.
        /// @return Result describing which EPs were registered or failed.
        EpDownloadResult DownloadAndRegisterEps(const std::vector<std::string>& names,
                                                 EpProgressCallback progressCallback = nullptr) const;

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
