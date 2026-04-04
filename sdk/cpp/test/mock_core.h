// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <stdexcept>

#include "foundry_local_internal_core.h"
#include "logger.h"

namespace foundry_local::Testing {

    /// A mock implementation of IFoundryLocalCore for unit testing.
    /// Register expected command -> response mappings before use.
    class MockCore final : public Internal::IFoundryLocalCore {
    public:
        /// Handler signature: (command, dataArgument, callback, userData) -> response string.
        using Handler = std::function<std::string(std::string_view command, const std::string* dataArgument,
                                                  NativeCallbackFn callback, void* userData)>;

        /// Register a fixed response for a command.
        void OnCall(std::string command, std::string response) {
            handlers_[std::move(command)] = [r = std::move(response)](std::string_view, const std::string*,
                                                                      NativeCallbackFn, void*) { return r; };
        }

        /// Register a custom handler for a command.
        void OnCall(std::string command, Handler handler) { handlers_[std::move(command)] = std::move(handler); }

        /// Register a handler that returns an error for a command.
        void OnCallThrow(std::string command, std::string errorMessage) {
            errorResponses_[std::move(command)] = std::move(errorMessage);
        }

        /// Returns the number of times a command was called.
        int GetCallCount(const std::string& command) const {
            auto it = callCounts_.find(command);
            return it != callCounts_.end() ? it->second : 0;
        }

        /// Returns the last data argument passed for a command.
        const std::string& GetLastDataArg(const std::string& command) const {
            auto it = lastDataArgs_.find(command);
            if (it == lastDataArgs_.end()) {
                static const std::string empty;
                return empty;
            }
            return it->second;
        }

        // IFoundryLocalCore implementation
        CoreResponse call(std::string_view command, ILogger& /*logger*/, const std::string* dataArgument = nullptr,
                          NativeCallbackFn callback = nullptr, void* data = nullptr) const override {

            std::string cmd(command);
            const_cast<MockCore*>(this)->callCounts_[cmd]++;
            if (dataArgument) {
                const_cast<MockCore*>(this)->lastDataArgs_[cmd] = *dataArgument;
            }

            auto errIt = errorResponses_.find(cmd);
            if (errIt != errorResponses_.end()) {
                CoreResponse resp;
                resp.error = errIt->second;
                return resp;
            }

            auto it = handlers_.find(cmd);
            if (it == handlers_.end()) {
                throw std::runtime_error("MockCore: no handler registered for command '" + cmd + "'");
            }

            CoreResponse resp;
            resp.data = it->second(command, dataArgument, callback, data);
            return resp;
        }

        void unload() override {}

    private:
        std::unordered_map<std::string, Handler> handlers_;
        std::unordered_map<std::string, std::string> errorResponses_;
        std::unordered_map<std::string, int> callCounts_;
        std::unordered_map<std::string, std::string> lastDataArgs_;
    };

    /// Read a file into a string. Throws on failure.
    inline std::string ReadFile(const std::string& path) {
        std::ifstream in(path, std::ios::in | std::ios::binary);
        if (!in)
            throw std::runtime_error("Failed to open test data file: " + path);
        std::ostringstream contents;
        contents << in.rdbuf();
        return contents.str();
    }

    /// A mock core that reads model list, cached models and loaded models from JSON files on disk.
    class FileBackedCore final : public Internal::IFoundryLocalCore {
    public:
        FileBackedCore(std::string modelListPath, std::string cachedModelsPath, std::string loadedModelsPath = "")
            : modelListPath_(std::move(modelListPath)), cachedModelsPath_(std::move(cachedModelsPath)),
              loadedModelsPath_(std::move(loadedModelsPath)) {}

        static FileBackedCore FromModelList(const std::string& path) { return FileBackedCore(path, ""); }

        static FileBackedCore FromBoth(const std::string& modelListPath, const std::string& cachedModelsPath) {
            return FileBackedCore(modelListPath, cachedModelsPath);
        }

        static FileBackedCore FromAll(const std::string& modelListPath, const std::string& cachedModelsPath,
                                      const std::string& loadedModelsPath) {
            return FileBackedCore(modelListPath, cachedModelsPath, loadedModelsPath);
        }

        CoreResponse call(std::string_view command, ILogger& /*logger*/, const std::string* /*dataArgument*/ = nullptr,
                          NativeCallbackFn /*callback*/ = nullptr, void* /*data*/ = nullptr) const override {

            CoreResponse resp;

            if (command == "get_catalog_name") {
                resp.data = "TestCatalog";
                return resp;
            }

            if (command == "get_model_list") {
                resp.data = modelListPath_.empty() ? "[]" : ReadFile(modelListPath_);
                return resp;
            }

            if (command == "get_cached_models") {
                resp.data = cachedModelsPath_.empty() ? "[]" : ReadFile(cachedModelsPath_);
                return resp;
            }

            if (command == "list_loaded_models") {
                resp.data = loadedModelsPath_.empty() ? "[]" : ReadFile(loadedModelsPath_);
                return resp;
            }

            resp.data = "{}";
            return resp;
        }

        void unload() override {}

    private:
        std::string modelListPath_;
        std::string cachedModelsPath_;
        std::string loadedModelsPath_;
    };

} // namespace foundry_local::Testing
