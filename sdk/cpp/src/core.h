// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Core DLL interop – loads Microsoft.AI.Foundry.Local.Core.dll at runtime.
// Internal header, not part of the public API.

#pragma once

#include <windows.h>
#include <string>
#include <stdexcept>
#include <filesystem>
#include <memory>

#include <wil/win32_helpers.h>

#include "foundry_local_internal_core.h"
#include "foundry_local_exception.h"
#include "flcore_native.h"
#include "logger.h"

namespace foundry_local {

    namespace {
        inline std::filesystem::path getExecutableDir() {
            auto exePath = wil::GetModuleFileNameW(nullptr);
            return std::filesystem::path(exePath.get()).parent_path();
        }

        inline void* RequireProc(HMODULE mod, const char* name) {
            if (void* p = ::GetProcAddress(mod, name))
                return p;
            throw std::runtime_error(std::string("GetProcAddress failed for ") + name);
        }
    } // namespace

    struct Core : Internal::IFoundryLocalCore {
        using ResponseHandle = std::unique_ptr<ResponseBuffer, void (*)(ResponseBuffer*)>;

        Core() = default;
        ~Core() = default;

        void loadEmbedded() { loadFromPath(getExecutableDir() / "Microsoft.AI.Foundry.Local.Core.dll"); }

        void unload() override {
            module_.reset();
            execCmd_ = nullptr;
            execCbCmd_ = nullptr;
            freeResCmd_ = nullptr;
        }

        CoreResponse call(std::string_view command, ILogger& logger, const std::string* dataArgument = nullptr,
                          NativeCallbackFn callback = nullptr, void* data = nullptr) const override {
            if (!module_ || !execCmd_ || !execCbCmd_ || !freeResCmd_) {
                throw Exception("Core is not loaded. Cannot call command: " + std::string(command), logger);
            }

            RequestBuffer request{};
            request.Command = command.empty() ? nullptr : command.data();
            request.CommandLength = static_cast<int32_t>(command.size());

            if (dataArgument && !dataArgument->empty()) {
                request.Data = dataArgument->data();
                request.DataLength = static_cast<int32_t>(dataArgument->size());
            }

            ResponseBuffer response{};
            auto safeDeleter = [fn = freeResCmd_](ResponseBuffer* buf) {
                if (fn)
                    fn(buf);
            };
            std::unique_ptr<ResponseBuffer, decltype(safeDeleter)> responseGuard(&response, safeDeleter);

            if (callback != nullptr) {
                execCbCmd_(&request, &response, reinterpret_cast<void*>(callback), data);
            }
            else {
                execCmd_(&request, &response);
            }

            CoreResponse result;
            if (response.Error && response.ErrorLength > 0) {
                result.error.assign(static_cast<const char*>(response.Error), response.ErrorLength);
                return result;
            }

            if (response.Data && response.DataLength > 0) {
                result.data.assign(static_cast<const char*>(response.Data), response.DataLength);
            }

            return result;
        }

    private:
        wil::unique_hmodule module_;
        execute_command_fn execCmd_{};
        execute_command_with_callback_fn execCbCmd_{};
        free_response_fn freeResCmd_{};

        void loadFromPath(const std::filesystem::path& path) {
            wil::unique_hmodule m(::LoadLibraryW(path.c_str()));
            if (!m)
                throw std::runtime_error("LoadLibraryW failed");

            execCmd_ = reinterpret_cast<execute_command_fn>(RequireProc(m.get(), "execute_command"));
            execCbCmd_ = reinterpret_cast<execute_command_with_callback_fn>(
                RequireProc(m.get(), "execute_command_with_callback"));
            freeResCmd_ = reinterpret_cast<free_response_fn>(RequireProc(m.get(), "free_response"));

            module_ = std::move(m);
        }
    };

} // namespace foundry_local
