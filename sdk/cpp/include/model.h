// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <optional>
#include <memory>
#include <functional>
#include <filesystem>

#include <gsl/pointers>
#include <gsl/span>

#include "logger.h"
#include "openai/openai_chat_client.h"
#include "openai/openai_audio_client.h"

namespace FoundryLocal::Internal {
    struct IFoundryLocalCore;
}

namespace FoundryLocal {
#ifdef FL_TESTS
    namespace Testing {
        struct MockObjectFactory;
    }
#endif

    enum class DeviceType {
        Invalid,
        CPU,
        GPU,
        NPU
    };

    struct Runtime {
        DeviceType device_type = DeviceType::Invalid;
        std::string execution_provider;
    };

    struct PromptTemplate {
        std::string system;
        std::string user;
        std::string assistant;
        std::string prompt;
    };

    using DownloadProgressCallback = std::function<void(float percentage)>;

    // Forward declarations
    class ModelVariant;

    struct Parameter {
        std::string name;
        std::optional<std::string> value;
    };

    struct ModelSettings {
        std::vector<Parameter> parameters;
    };

    struct ModelInfo {
        std::string id;
        std::string name;
        uint32_t version = 0;
        std::string alias;
        std::optional<std::string> display_name;
        std::string provider_type;
        std::string uri;
        std::string model_type;
        std::optional<PromptTemplate> prompt_template;
        std::optional<std::string> publisher;
        std::optional<ModelSettings> model_settings;
        std::optional<std::string> license;
        std::optional<std::string> license_description;
        bool cached = false;
        std::optional<std::string> task;
        std::optional<Runtime> runtime;
        std::optional<uint32_t> file_size_mb;
        std::optional<bool> supports_tool_calling;
        std::optional<int64_t> max_output_tokens;
        std::optional<std::string> min_fl_version;
        int64_t created_at_unix = 0;
    };

    class ModelVariant final {
    public:
        const ModelInfo& GetInfo() const;
        const std::filesystem::path& GetPath() const;
        void Download(DownloadProgressCallback onProgress = nullptr) const;
        void Load() const;

        bool IsLoaded() const;
        bool IsCached() const;
        void Unload() const;
        void RemoveFromCache();

        [[deprecated("Use OpenAIAudioClient(model) constructor instead")]]
        OpenAIAudioClient GetAudioClient() const;

        [[deprecated("Use OpenAIChatClient(model) constructor instead")]]
        OpenAIChatClient GetChatClient() const;

        const std::string& GetId() const noexcept;
        const std::string& GetAlias() const noexcept;
        uint32_t GetVersion() const noexcept;

    private:
        static std::string MakeModelParamRequest(std::string_view modelId);
        explicit ModelVariant(gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core, ModelInfo info,
                              gsl::not_null<ILogger*> logger);

        ModelInfo info_;
        mutable std::filesystem::path cachedPath_;
        gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core_;
        gsl::not_null<ILogger*> logger_;

        friend class Catalog;
        friend class OpenAIAudioClient;
        friend class OpenAIChatClient;
#ifdef FL_TESTS
        friend struct Testing::MockObjectFactory;
#endif
    };

    class Model final {
    public:
        gsl::span<const ModelVariant> GetAllModelVariants() const;
        const ModelVariant* GetLatestVariant(gsl::not_null<const ModelVariant*> variant) const;

        bool IsLoaded() const { return SelectedVariant().IsLoaded(); }
        bool IsCached() const { return SelectedVariant().IsCached(); }
        const std::filesystem::path& GetPath() const { return SelectedVariant().GetPath(); }
        void Download(DownloadProgressCallback onProgress = nullptr) const {
            SelectedVariant().Download(std::move(onProgress));
        }
        void Load() const { SelectedVariant().Load(); }
        void Unload() const { SelectedVariant().Unload(); }
        void RemoveFromCache() { SelectedVariant().RemoveFromCache(); }
        [[deprecated("Use OpenAIAudioClient(model) constructor instead")]]
        OpenAIAudioClient GetAudioClient() const {
            return SelectedVariant().GetAudioClient();
        }

        [[deprecated("Use OpenAIChatClient(model) constructor instead")]]
        OpenAIChatClient GetChatClient() const {
            return SelectedVariant().GetChatClient();
        }

        const std::string& GetId() const;
        const std::string& GetAlias() const;
        void SelectVariant(gsl::not_null<const ModelVariant*> variant) const;

    private:
        explicit Model(gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core, gsl::not_null<ILogger*> logger);
        ModelVariant& SelectedVariant();
        const ModelVariant& SelectedVariant() const;

        gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core_;

        std::vector<ModelVariant> variants_;
        mutable std::optional<size_t> selectedVariantIndex_;
        gsl::not_null<ILogger*> logger_;

        friend class Catalog;
#ifdef FL_TESTS
        friend struct Testing::MockObjectFactory;
#endif
    };

} // namespace FoundryLocal
