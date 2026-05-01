// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Core shared library interop — loads the Foundry Local Core library at runtime.
// Internal header, not part of the public API.

#pragma once

#include <string>
#include <stdexcept>
#include <filesystem>
#include <memory>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <dlfcn.h>
  #include <climits>
  #include <unistd.h>
  #ifdef __APPLE__
    #include <mach-o/dyld.h>
  #endif
#endif

#include "foundry_local_internal_core.h"
#include "foundry_local_exception.h"
#include "flcore_native.h"
#include "logger.h"

namespace foundry_local {

    namespace {

        // RAII wrapper for a dynamically loaded shared library handle.
        struct SharedLibHandle {
            void* handle = nullptr;

            SharedLibHandle() = default;
            explicit SharedLibHandle(void* h) : handle(h) {}
            SharedLibHandle(const SharedLibHandle&) = delete;
            SharedLibHandle& operator=(const SharedLibHandle&) = delete;
            SharedLibHandle(SharedLibHandle&& o) noexcept : handle(o.handle) { o.handle = nullptr; }
            SharedLibHandle& operator=(SharedLibHandle&& o) noexcept {
                reset();
                handle = o.handle;
                o.handle = nullptr;
                return *this;
            }
            ~SharedLibHandle() { reset(); }

            void reset() noexcept {
                if (!handle) return;
#ifdef _WIN32
                ::FreeLibrary(static_cast<HMODULE>(handle));
#else
                ::dlclose(handle);
#endif
                handle = nullptr;
            }

            explicit operator bool() const noexcept { return handle != nullptr; }
        };

        inline std::filesystem::path GetExecutableDir() {
#ifdef _WIN32
            std::wstring buf(MAX_PATH, L'\0');
            for (;;) {
                DWORD len = ::GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
                if (len == 0)
                    throw std::runtime_error("GetModuleFileNameW failed");
                if (len < static_cast<DWORD>(buf.size()))
                    return std::filesystem::path(buf.c_str()).parent_path();
                buf.resize(buf.size() * 2);
            }
#elif defined(__APPLE__)
            char buf[PATH_MAX];
            uint32_t size = sizeof(buf);
            if (_NSGetExecutablePath(buf, &size) != 0)
                throw std::runtime_error("_NSGetExecutablePath failed");
            return std::filesystem::canonical(buf).parent_path();
#else
            return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#endif
        }

        inline std::string GetLoaderError() {
#ifdef _WIN32
            DWORD err = ::GetLastError();
            if (err == 0) return {};
            LPSTR buf = nullptr;
            DWORD len = ::FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<LPSTR>(&buf), 0, nullptr);
            std::string msg(buf, len);
            ::LocalFree(buf);
            // Trim trailing newline
            while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r'))
                msg.pop_back();
            return msg;
#else
            const char* err = ::dlerror();
            return err ? std::string(err) : std::string{};
#endif
        }

        inline void* LoadSharedLib(const std::filesystem::path& path) {
#ifdef _WIN32
            return static_cast<void*>(::LoadLibraryW(path.c_str()));
#else
            return ::dlopen(path.c_str(), RTLD_NOW);
#endif
        }

        inline void* RequireProc(void* mod, const char* name) {
#ifdef _WIN32
            void* p = reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(mod), name));
#else
            void* p = ::dlsym(mod, name);
#endif
            if (!p) {
                std::string msg = std::string("Symbol not found: ") + name;
                std::string detail = GetLoaderError();
                if (!detail.empty())
                    msg += " (" + detail + ")";
                throw std::runtime_error(msg);
            }
            return p;
        }

        inline void* OptionalProc(void* mod, const char* name) noexcept {
#ifdef _WIN32
            return reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(mod), name));
#else
            return ::dlsym(mod, name);
#endif
        }

    } // namespace

    struct Core : Internal::IFoundryLocalCore {
        using ResponseHandle = std::unique_ptr<ResponseBuffer, void (*)(ResponseBuffer*)>;

        Core() = default;
        ~Core() = default;

        void LoadEmbedded() {
            constexpr const char* kCoreLibName =
#ifdef _WIN32
                "Microsoft.AI.Foundry.Local.Core.dll";
#elif defined(__APPLE__)
                "Microsoft.AI.Foundry.Local.Core.dylib";
#else
                "Microsoft.AI.Foundry.Local.Core.so";
#endif
            LoadFromPath(GetExecutableDir() / kCoreLibName);
        }

        void unload() override {
            module_.reset();
            execCmd_ = nullptr;
            execCbCmd_ = nullptr;
            execBinaryCmd_ = nullptr;
            freeResCmd_ = nullptr;
        }

        CoreResponse call(std::string_view command, ILogger& logger, const std::string* dataArgument = nullptr,
                          NativeCallbackFn callback = nullptr, void* data = nullptr) const override {
            if (!static_cast<bool>(module_) || !execCmd_ || !execCbCmd_ || !freeResCmd_) {
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

        CoreResponse callWithBinary(std::string_view command, ILogger& logger,
                                     const std::string* dataArgument,
                                     const uint8_t* binaryData, size_t binaryDataLength) const override {
            if (!static_cast<bool>(module_) || !freeResCmd_) {
                throw Exception("Core is not loaded. Cannot call command: " + std::string(command), logger);
            }
            if (!execBinaryCmd_) {
                throw Exception("execute_command_with_binary is not available in this version of the Core library.", logger);
            }

            StreamingRequestBuffer request{};
            request.Command = command.empty() ? nullptr : command.data();
            request.CommandLength = static_cast<int32_t>(command.size());

            if (dataArgument && !dataArgument->empty()) {
                request.Data = dataArgument->data();
                request.DataLength = static_cast<int32_t>(dataArgument->size());
            }

            if (binaryData && binaryDataLength > 0) {
                if (binaryDataLength > static_cast<size_t>(INT32_MAX)) {
                    throw Exception("Binary data length exceeds maximum supported size (INT32_MAX).", logger);
                }
                request.BinaryData = binaryData;
                request.BinaryDataLength = static_cast<int32_t>(binaryDataLength);
            }

            ResponseBuffer response{};
            auto safeDeleter = [fn = freeResCmd_](ResponseBuffer* buf) {
                if (fn)
                    fn(buf);
            };
            std::unique_ptr<ResponseBuffer, decltype(safeDeleter)> responseGuard(&response, safeDeleter);

            execBinaryCmd_(&request, &response);

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
        SharedLibHandle module_;
        execute_command_fn execCmd_{};
        execute_command_with_callback_fn execCbCmd_{};
        execute_command_with_binary_fn execBinaryCmd_{};
        free_response_fn freeResCmd_{};

        void LoadFromPath(const std::filesystem::path& path) {
            SharedLibHandle m(LoadSharedLib(path));
            if (!m) {
                std::string msg = "Failed to load shared library: " + path.string();
                std::string detail = GetLoaderError();
                if (!detail.empty())
                    msg += " (" + detail + ")";
                throw std::runtime_error(msg);
            }

            execCmd_ = reinterpret_cast<execute_command_fn>(RequireProc(m.handle, "execute_command"));
            execCbCmd_ = reinterpret_cast<execute_command_with_callback_fn>(
                RequireProc(m.handle, "execute_command_with_callback"));
            execBinaryCmd_ = reinterpret_cast<execute_command_with_binary_fn>(
                OptionalProc(m.handle, "execute_command_with_binary"));
            freeResCmd_ = reinterpret_cast<free_response_fn>(RequireProc(m.handle, "free_response"));

            module_ = std::move(m);
        }
    };

} // namespace foundry_local
