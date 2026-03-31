#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <chrono>
#include <optional>
#include <unordered_map>
#include <memory>
#include <functional>
#include <filesystem>

#include <gsl/pointers>
#include <gsl/span>

#include "configuration.h"

#include "logger.h"

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

    /// Reason the model stopped generating tokens.
    enum class FinishReason {
        None,
        Stop,
        Length,
        ToolCalls,
        ContentFilter
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

    struct AudioCreateTranscriptionResponse {
        std::string text;
    };

    /// JSON Schema property definition used to describe tool function parameters.
    struct PropertyDefinition {
        std::string type;
        std::optional<std::string> description;
        std::optional<std::unordered_map<std::string, PropertyDefinition>> properties;
        std::optional<std::vector<std::string>> required;
    };

    /// Describes a function that a model may call.
    struct FunctionDefinition {
        std::string name;
        std::optional<std::string> description;
        std::optional<PropertyDefinition> parameters;
    };

    /// A tool definition following the OpenAI tool calling spec.
    struct ToolDefinition {
        std::string type = "function";
        FunctionDefinition function;
    };

    /// A parsed function call returned by the model.
    struct FunctionCall {
        std::string name;
        std::string arguments;  ///< JSON string of the arguments
    };

    /// A tool call returned by the model in a chat completion response.
    struct ToolCall {
        std::string id;
        std::string type;
        std::optional<FunctionCall> function_call;
    };

    /// Controls whether and how the model calls tools.
    enum class ToolChoiceKind {
        Auto,
        None,
        Required
    };

    struct ChatMessage {
        std::string role;
        std::string content;
        std::optional<std::string> tool_call_id;  ///< For role="tool" responses
        std::vector<ToolCall> tool_calls;
    };

    struct ChatChoice {
        int index = 0;
        FinishReason finish_reason = FinishReason::None;

        // non-streaming
        std::optional<ChatMessage> message;

        // streaming
        std::optional<ChatMessage> delta;
    };

    struct ChatCompletionCreateResponse {
        int64_t created = 0;
        std::string id;

        bool is_delta = false;
        bool successful = false;
        int http_status_code = 0;

        std::vector<ChatChoice> choices;

        /// Returns the object type string. Derived from is_delta — no allocation.
        const char* GetObject() const noexcept { return is_delta ? "chat.completion.chunk" : "chat.completion"; }

        /// Returns the created timestamp as an ISO 8601 string.
        /// Computed lazilym only allocates when called.
        std::string GetCreatedAtIso() const;
    };

    struct ChatSettings {
        std::optional<float> frequency_penalty;
        std::optional<int> max_tokens;
        std::optional<int> n;
        std::optional<float> temperature;
        std::optional<float> presence_penalty;
        std::optional<int> random_seed;
        std::optional<int> top_k;
        std::optional<float> top_p;
        std::optional<ToolChoiceKind> tool_choice;
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

    class AudioClient final {
    public:
        explicit AudioClient(gsl::not_null<const ModelVariant*> model);

        /// Returns the model ID this client was created for.
        const std::string& GetModelId() const noexcept { return modelId_; }

        AudioCreateTranscriptionResponse TranscribeAudio(const std::filesystem::path& audioFilePath) const;

        using StreamCallback = std::function<void(const AudioCreateTranscriptionResponse& chunk)>;
        void TranscribeAudioStreaming(const std::filesystem::path& audioFilePath, const StreamCallback& onChunk) const;

    private:
        AudioClient(gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core, std::string_view modelId,
                    gsl::not_null<ILogger*> logger);

        std::string modelId_;
        gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core_;
        gsl::not_null<ILogger*> logger_;

        friend class ModelVariant;
    };

    class ChatClient final {
    public:
        explicit ChatClient(gsl::not_null<const ModelVariant*> model);

        /// Returns the model ID this client was created for.
        const std::string& GetModelId() const noexcept { return modelId_; }

        ChatCompletionCreateResponse CompleteChat(gsl::span<const ChatMessage> messages,
                                                  const ChatSettings& settings) const;

        ChatCompletionCreateResponse CompleteChat(gsl::span<const ChatMessage> messages,
                                                  gsl::span<const ToolDefinition> tools,
                                                  const ChatSettings& settings) const;

        using StreamCallback = std::function<void(const ChatCompletionCreateResponse& chunk)>;
        void CompleteChatStreaming(gsl::span<const ChatMessage> messages, const ChatSettings& settings,
                                   const StreamCallback& onChunk) const;

        void CompleteChatStreaming(gsl::span<const ChatMessage> messages, gsl::span<const ToolDefinition> tools,
                                   const ChatSettings& settings, const StreamCallback& onChunk) const;

    private:
        ChatClient(gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core, std::string_view modelId,
                   gsl::not_null<ILogger*> logger);

        std::string BuildChatRequestJson(gsl::span<const ChatMessage> messages, gsl::span<const ToolDefinition> tools,
                                         const ChatSettings& settings, bool stream) const;

        std::string modelId_;
        gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core_;
        gsl::not_null<ILogger*> logger_;

        friend class ModelVariant;
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

        [[deprecated("Use AudioClient(model) constructor instead")]]
        AudioClient GetAudioClient() const;

        [[deprecated("Use ChatClient(model) constructor instead")]]
        ChatClient GetChatClient() const;

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
        friend class AudioClient;
        friend class ChatClient;
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
        [[deprecated("Use AudioClient(model) constructor instead")]]
        AudioClient GetAudioClient() const {
            return SelectedVariant().GetAudioClient();
        }

        [[deprecated("Use ChatClient(model) constructor instead")]]
        ChatClient GetChatClient() const {
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

    class Catalog final {
    public:
        Catalog(const Catalog&) = delete;
        Catalog& operator=(const Catalog&) = delete;
        Catalog(Catalog&&) = delete;
        Catalog& operator=(Catalog&&) = delete;

        static std::unique_ptr<Catalog> Create(gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core,
                                               gsl::not_null<ILogger*> logger) {
            return std::unique_ptr<Catalog>(new Catalog(core, logger));
        }

        const std::string& GetName() const { return name_; }
        std::vector<const Model*> ListModels() const;
        std::vector<const ModelVariant*> GetLoadedModels() const;
        std::vector<const ModelVariant*> GetCachedModels() const;

        const Model* GetModel(std::string_view modelId) const;
        const ModelVariant* GetModelVariant(std::string_view modelVariantId) const;

    private:
        void UpdateModels() const;

        mutable std::chrono::steady_clock::time_point lastFetch_{};

        mutable std::unordered_map<std::string, Model> byAlias_;
        mutable std::unordered_map<std::string, ModelVariant> modelIdToModelVariant_;

        explicit Catalog(gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> injected,
                         gsl::not_null<ILogger*> logger);

        gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core_;
        std::string name_;
        gsl::not_null<ILogger*> logger_;

        friend class FoundryLocalManager;
#ifdef FL_TESTS
        friend struct Testing::MockObjectFactory;
#endif
    };

    class FoundryLocalManager final {
    public:
        FoundryLocalManager(const FoundryLocalManager&) = delete;
        FoundryLocalManager& operator=(const FoundryLocalManager&) = delete;
        FoundryLocalManager(FoundryLocalManager&& other) noexcept;
        FoundryLocalManager& operator=(FoundryLocalManager&& other) noexcept;

        explicit FoundryLocalManager(Configuration configuration, ILogger* logger = nullptr);
        ~FoundryLocalManager();

        const Catalog& GetCatalog() const;

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

} // namespace FoundryLocal
