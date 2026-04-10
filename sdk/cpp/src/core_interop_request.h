// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>
#include <utility>

namespace foundry_local {

    class CoreInteropRequest final {
    public:
        explicit CoreInteropRequest(std::string command) : command_(std::move(command)) {}

        CoreInteropRequest& AddParam(std::string_view key, std::string_view value) {
            params_[std::string(key)] = std::string(value);
            return *this;
        }

        template <typename T> CoreInteropRequest& AddParam(std::string_view key, const T& value) {
            params_[std::string(key)] = value;
            return *this;
        }

        CoreInteropRequest& AddJsonParam(std::string_view key, const nlohmann::json& jsonValue) {
            params_[std::string(key)] = jsonValue.dump();
            return *this;
        }

        std::string ToJson() const {
            nlohmann::json wrapper;
            if (!params_.empty()) {
                wrapper["Params"] = params_;
            }
            return wrapper.dump();
        }

        const std::string& Command() const noexcept { return command_; }

    private:
        std::string command_;
        nlohmann::json params_;
    };

} // namespace foundry_local
