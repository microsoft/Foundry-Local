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

namespace foundry_local::Internal {
    struct IFoundryLocalCore;
}

namespace foundry_local {
#ifdef FL_TESTS
    namespace Testing {
        struct MockObjectFactory;
    }
#endif

    using DownloadProgressCallback = std::function<void(float percentage)>;

    class IModel {
    public:
        virtual ~IModel() = default;

        virtual const std::string& GetId() const = 0;
        virtual const std::string& GetAlias() const = 0;
        virtual bool IsLoaded() const = 0;
        virtual bool IsCached() const = 0;
        virtual const std::filesystem::path& GetPath() const = 0;
        virtual void Download(DownloadProgressCallback onProgress = nullptr) = 0;
        virtual void Load() = 0;
        virtual void Unload() = 0;
        virtual void RemoveFromCache() = 0;

    protected:
        struct CoreAccess {
            gsl::not_null<Internal::IFoundryLocalCore*> core;
            std::string modelName;
            gsl::not_null<ILogger*> logger;
        };

        virtual CoreAccess GetCoreAccess() const = 0;

        friend class OpenAIChatClient;
        friend class OpenAIAudioClient;
    };

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

    class ModelVariant final : public IModel {
    public:
        const ModelInfo& GetInfo() const;
        const std::filesystem::path& GetPath() const override;
        void Download(DownloadProgressCallback onProgress = nullptr) override;
        void Load() override;

        bool IsLoaded() const override;
        bool IsCached() const override;
        void Unload() override;
        void RemoveFromCache() override;

        [[deprecated("Use OpenAIAudioClient(model) constructor instead")]]
        OpenAIAudioClient GetAudioClient() const;

        [[deprecated("Use OpenAIChatClient(model) constructor instead")]]
        OpenAIChatClient GetChatClient() const;

        const std::string& GetId() const noexcept override;
        const std::string& GetAlias() const noexcept override;
        uint32_t GetVersion() const noexcept;

    protected:
        CoreAccess GetCoreAccess() const override;

    private:
        static std::string MakeModelParamRequest(std::string_view modelId);
        explicit ModelVariant(gsl::not_null<foundry_local::Internal::IFoundryLocalCore*> core, ModelInfo info,
                              gsl::not_null<ILogger*> logger);

        ModelInfo info_;
        mutable std::filesystem::path cachedPath_;
        gsl::not_null<foundry_local::Internal::IFoundryLocalCore*> core_;
        gsl::not_null<ILogger*> logger_;

        friend class Catalog;
        friend class Model;
#ifdef FL_TESTS
        friend struct Testing::MockObjectFactory;
#endif
    };

    class Model final : public IModel {
    public:
        gsl::span<const ModelVariant> GetAllModelVariants() const;
        const ModelVariant& GetLatestVersion(const ModelVariant& variant) const;

        bool IsLoaded() const override { return SelectedVariant().IsLoaded(); }
        bool IsCached() const override { return SelectedVariant().IsCached(); }
        const std::filesystem::path& GetPath() const override { return SelectedVariant().GetPath(); }
        void Download(DownloadProgressCallback onProgress = nullptr) override {
            SelectedVariant().Download(std::move(onProgress));
        }
        void Load() override { SelectedVariant().Load(); }
        void Unload() override { SelectedVariant().Unload(); }
        void RemoveFromCache() override { SelectedVariant().RemoveFromCache(); }
        [[deprecated("Use OpenAIAudioClient(model) constructor instead")]]
        OpenAIAudioClient GetAudioClient() const {
            return SelectedVariant().GetAudioClient();
        }

        [[deprecated("Use OpenAIChatClient(model) constructor instead")]]
        OpenAIChatClient GetChatClient() const {
            return SelectedVariant().GetChatClient();
        }

        const std::string& GetId() const override;
        const std::string& GetAlias() const override;
        void SelectVariant(const ModelVariant& variant) const;

    protected:
        CoreAccess GetCoreAccess() const override;

    private:
        explicit Model(gsl::not_null<foundry_local::Internal::IFoundryLocalCore*> core, gsl::not_null<ILogger*> logger);
        ModelVariant& SelectedVariant();
        const ModelVariant& SelectedVariant() const;

        gsl::not_null<foundry_local::Internal::IFoundryLocalCore*> core_;

        std::vector<ModelVariant> variants_;
        mutable std::optional<size_t> selectedVariantIndex_;
        gsl::not_null<ILogger*> logger_;

        friend class Catalog;
#ifdef FL_TESTS
        friend struct Testing::MockObjectFactory;
#endif
    };

} // namespace foundry_local
