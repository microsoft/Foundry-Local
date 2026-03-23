#include <windows.h>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <ctime>
#include <gsl/span>
#include "core_interop_request.h"
#include "configuration.h"
#include "foundry_local.h"
#include "flcore_native.h"
#include "foundry_local_internal_core.h"
#include "parser.h"
#include "logger.h"
#include <wil/win32_helpers.h>
#include "foundry_local_exception.h"

// Internal private namespace.
namespace {
    std::filesystem::path getExecutableDir() {
        auto exePath = wil::GetModuleFileNameW(nullptr);
        return std::filesystem::path(exePath.get()).parent_path();
    }
} // namespace

namespace {
    // Wrap Params: { ... } into a request object
    inline nlohmann::json MakeParams(nlohmann::json params) {
        return nlohmann::json{ {"Params", std::move(params)} };
    }

    // Most common: Params { "Model": <idOrName> }
    inline nlohmann::json MakeModelParams(std::string_view model) {
        return MakeParams(nlohmann::json{ {"Model", std::string(model)} });
    }

    // Serialize + call
    inline std::string CallWithJson(FoundryLocal::Internal::IFoundryLocalCore* core, std::string_view command,
        const nlohmann::json& requestJson, FoundryLocal::ILogger& logger) {
        std::string payload = requestJson.dump();
        return core->call(command, logger, &payload);
    }

    // Serialize + call with native callback
    inline std::string CallWithJsonAndCallback(FoundryLocal::Internal::IFoundryLocalCore* core,
        std::string_view command, const nlohmann::json& requestJson, FoundryLocal::ILogger& logger,
        void* callback, void* userData) {
        std::string payload = requestJson.dump();
        return core->call(command, logger, &payload, callback, userData);
    }

    // Overload: allow Params object directly
    inline std::string CallWithParams(FoundryLocal::Internal::IFoundryLocalCore* core, std::string_view command,
        const nlohmann::json& params, FoundryLocal::ILogger& logger) {
        return CallWithJson(core, command, MakeParams(params), logger);
    }

    // Overload: no payload
    inline std::string CallNoArgs(FoundryLocal::Internal::IFoundryLocalCore* core, std::string_view command,
        FoundryLocal::ILogger& logger) {
        return core->call(command, logger, nullptr);
    }

    std::vector<std::string> GetLoadedModelsInternal(FoundryLocal::Internal::IFoundryLocalCore* core,
        FoundryLocal::ILogger& logger) {
        std::string raw = core->call("list_loaded_models", logger);
        try {
            auto parsed = nlohmann::json::parse(raw);
            return parsed.get<std::vector<std::string>>();
        }
        catch (const nlohmann::json::exception& e) {
            throw FoundryLocal::FoundryLocalException(
                "Catalog::GetLoadedModelsInternal() JSON error: " + std::string(e.what()), logger);
        }
    }

    std::vector<std::string> GetCachedModelsInternal(FoundryLocal::Internal::IFoundryLocalCore* core,
        FoundryLocal::ILogger& logger) {
        std::string raw = core->call("get_cached_models", logger);

        try {
            auto parsed = nlohmann::json::parse(raw);
            return parsed.get<std::vector<std::string>>();
        }
        catch (const nlohmann::json::exception& e) {
            throw FoundryLocal::FoundryLocalException(
                "Catalog::GetCachedModelsInternal JSON error: " + std::string(e.what()), logger);
        }
    }

    inline void StripSuffixAfterColon(std::string& id) {
        const auto pos = id.find_last_of(':');
        if (pos != std::string::npos) {
            id.erase(pos);
        }
    }

    std::vector<const FoundryLocal::ModelVariant*>
        CollectVariantsByIds(const std::unordered_map<std::string, FoundryLocal::ModelVariant>& modelIdToModelVariant,
            std::vector<std::string> ids) {
        std::vector<const FoundryLocal::ModelVariant*> out;
        out.reserve(ids.size());

        for (auto& id : ids) {
            StripSuffixAfterColon(id);

            auto it = modelIdToModelVariant.find(id);
            if (it != modelIdToModelVariant.end()) {
                out.emplace_back(&it->second);
            }
        }
        return out;
    }

} // namespace

namespace FoundryLocal {
    inline static void* RequireProc(HMODULE mod, const char* name) {
        if (void* p = ::GetProcAddress(mod, name))
            return p;
        throw std::runtime_error(std::string("GetProcAddress failed for ") + name);
    }

    struct Core : FoundryLocal::Internal::IFoundryLocalCore {
        using ResponseHandle = std::unique_ptr<ResponseBuffer, void (*)(ResponseBuffer*)>;

        Core() = default;
        ~Core() = default;

        void loadEmbedded() {
            loadFromPath(getExecutableDir() / "Microsoft.AI.Foundry.Local.Core.dll");
        }

        void unload() {
            module_.reset();
            execCmd_ = nullptr;
            execCbCmd_ = nullptr;
            freeResCmd_ = nullptr;
        }
        std::string call(std::string_view command, ILogger& logger, const std::string* dataArgument = nullptr,
            void* callback = nullptr, void* data = nullptr) const override {
            if (!module_ || !execCmd_ || !execCbCmd_ || !freeResCmd_) {
                throw FoundryLocalException(
                    "Core is not loaded. Cannot call command: " + std::string(command), logger);
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
                if (fn) fn(buf);
            };
            std::unique_ptr<ResponseBuffer, decltype(safeDeleter)> responseGuard(&response, safeDeleter);

            using CallbackFn = void (*)(void*, int32_t, void*);

            if (callback != nullptr) {
                auto cb = reinterpret_cast<CallbackFn>(callback);
                execCbCmd_(&request, &response, reinterpret_cast<void*>(cb), data);
            }
            else {
                execCmd_(&request, &response);
            }

            std::string result;
            if (response.Error && response.ErrorLength > 0) {
                std::string err(static_cast<const char*>(response.Error), response.ErrorLength);
                throw FoundryLocalException(
                    std::string("Command failed [").append(command).append("]: ").append(err), logger);
            }

            if (response.Data && response.DataLength > 0) {
                result.assign(static_cast<const char*>(response.Data), response.DataLength);
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

    /// <summary>
    /// AudioClient
    /// </summary>

    AudioClient::AudioClient(gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core, std::string_view modelId,
        gsl::not_null<ILogger*> logger)
        : core_(core), modelId_(modelId), logger_(logger) {
    }

    AudioCreateTranscriptionResponse AudioClient::TranscribeAudio(const std::filesystem::path& audioFilePath) const {
        nlohmann::json openAiReq = { {"Model", modelId_}, {"FileName", audioFilePath.string()} };
        CoreInteropRequest req("audio_transcribe");
        req.AddParam("OpenAICreateRequest", openAiReq.dump());

        std::string json = req.ToJson();

        AudioCreateTranscriptionResponse response;
        response.text = core_->call(req.Command(), *logger_, &json);

        return response;
    }

    void AudioClient::TranscribeAudioStreaming(const std::filesystem::path& audioFilePath, const StreamCallback& onChunk) const {
        nlohmann::json openAiReq = { {"Model", modelId_}, {"FileName", audioFilePath.string()} };
        CoreInteropRequest req("audio_transcribe");
        req.AddParam("OpenAICreateRequest", openAiReq.dump());

        std::string json = req.ToJson();

        struct State {
            const StreamCallback* cb;
            std::exception_ptr exception;
        } state{ &onChunk, nullptr };

        auto streamCallback = [](void* data, int32_t len, void* user) {
            if (!data || len <= 0)
                return;

            auto* st = static_cast<State*>(user);
            if (st->exception)
                return;

            try {
                std::string text(static_cast<const char*>(data), static_cast<size_t>(len));
                AudioCreateTranscriptionResponse chunk;
                chunk.text = std::move(text);
                (*(st->cb))(chunk);
            }
            catch (...) {
                st->exception = std::current_exception();
            }
        };

        core_->call(req.Command(), *logger_, &json, reinterpret_cast<void*>(+streamCallback),
            reinterpret_cast<void*>(&state));

        if (state.exception) {
            std::rethrow_exception(state.exception);
        }
    }


    std::string ChatCompletionCreateResponse::GetCreatedAtIso() const {
        if (created == 0) return {};
        std::time_t t = static_cast<std::time_t>(created);
        std::tm tm{};
#ifdef _WIN32
        gmtime_s(&tm, &t);
#else
        gmtime_r(&t, &tm);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
        return buf;
    }

    /// <summary>
    /// ChatClient
    /// </summary>

    ChatClient::ChatClient(gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core, std::string_view modelId,
        gsl::not_null<ILogger*> logger)
        : core_(core), modelId_(modelId), logger_(logger) {
    }

    std::string ChatClient::BuildChatRequestJson(gsl::span<const ChatMessage> messages, gsl::span<const ToolDefinition> tools,
        const ChatSettings& settings, bool stream) const {
        nlohmann::json jMessages = nlohmann::json::array();
        for (const auto& msg : messages) {
            nlohmann::json jMsg = { {"role", msg.role}, {"content", msg.content} };
            if (msg.tool_call_id)
                jMsg["tool_call_id"] = *msg.tool_call_id;
            jMessages.push_back(std::move(jMsg));
        }

        nlohmann::json req = { {"model", modelId_}, {"messages", std::move(jMessages)}, {"stream", stream} };

        if (!tools.empty()) {
            nlohmann::json jTools = nlohmann::json::array();
            for (const auto& tool : tools) {
                nlohmann::json jTool;
                to_json(jTool, tool);
                jTools.push_back(std::move(jTool));
            }
            req["tools"] = std::move(jTools);
        }

        if (settings.tool_choice)
            req["tool_choice"] = tool_choice_to_string(*settings.tool_choice);
        if (settings.top_k)
            req["metadata"] = { {"top_k", *settings.top_k} };
        if (settings.frequency_penalty)
            req["frequency_penalty"] = *settings.frequency_penalty;
        if (settings.presence_penalty)
            req["presence_penalty"] = *settings.presence_penalty;
        if (settings.max_tokens)
            req["max_completion_tokens"] = *settings.max_tokens;
        if (settings.n)
            req["n"] = *settings.n;
        if (settings.temperature)
            req["temperature"] = *settings.temperature;
        if (settings.top_p)
            req["top_p"] = *settings.top_p;
        if (settings.random_seed)
            req["seed"] = *settings.random_seed;

        return req.dump();
    }

    ChatCompletionCreateResponse ChatClient::CompleteChat(gsl::span<const ChatMessage> messages,
        const ChatSettings& settings) const {
        return CompleteChat(messages, {}, settings);
    }

    ChatCompletionCreateResponse ChatClient::CompleteChat(gsl::span<const ChatMessage> messages,
        gsl::span<const ToolDefinition> tools, const ChatSettings& settings) const {
        std::string openAiReqJson = BuildChatRequestJson(messages, tools, settings, /*stream=*/false);

        CoreInteropRequest req("chat_completions");
        req.AddParam("OpenAICreateRequest", openAiReqJson);

        std::string json = req.ToJson();
        std::string rawResult = core_->call(req.Command(), *logger_, &json);

        return nlohmann::json::parse(rawResult).get<FoundryLocal::ChatCompletionCreateResponse>();
    }

    void ChatClient::CompleteChatStreaming(gsl::span<const ChatMessage> messages, const ChatSettings& settings,
        const StreamCallback& onChunk) const {
        CompleteChatStreaming(messages, {}, settings, onChunk);
    }

    void ChatClient::CompleteChatStreaming(gsl::span<const ChatMessage> messages, gsl::span<const ToolDefinition> tools,
        const ChatSettings& settings, const StreamCallback& onChunk) const {
        std::string openAiReqJson = BuildChatRequestJson(messages, tools, settings, /*stream=*/true);

        CoreInteropRequest req("chat_completions");
        req.AddParam("OpenAICreateRequest", openAiReqJson);
        std::string json = req.ToJson();

        struct State {
            const StreamCallback* cb;
            std::exception_ptr exception;
        } state{ &onChunk, nullptr };

        auto streamCallback = [](void* data, int32_t len, void* user) {
            if (!data || len <= 0)
                return;

            auto* st = static_cast<State*>(user);
            if (st->exception)
                return;

            std::string s(static_cast<const char*>(data), static_cast<size_t>(len));

            try {
                auto parsed = nlohmann::json::parse(s).get<FoundryLocal::ChatCompletionCreateResponse>();

                (*(st->cb))(parsed);
            }
            catch (const nlohmann::json::exception& e) {
                st->exception = std::make_exception_ptr(
                    FoundryLocalException(std::string("Error while parsing streaming chat chunk: ") + e.what()));
            }
            catch (...) {
                st->exception = std::current_exception();
            }
            };

        core_->call(req.Command(), *logger_, &json, reinterpret_cast<void*>(+streamCallback),
            reinterpret_cast<void*>(&state));

        if (state.exception) {
            std::rethrow_exception(state.exception);
        }
    }

    /// <summary>
    /// ModelVariant
    /// </summary>

    ModelVariant::ModelVariant(gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core, ModelInfo info,
        gsl::not_null<ILogger*> logger)
        : core_(core), info_(std::move(info)), logger_(logger) {
    }

    const ModelInfo& ModelVariant::GetInfo() const {
        return info_;
    }

    void ModelVariant::RemoveFromCache() {
        try {
            CallWithJson(core_, "remove_cached_model", MakeModelParams(info_.name), *logger_);
            cachedPath_.clear();
        }
        catch (const std::exception& ex) {
            throw FoundryLocalException("Error removing model from cache [" + info_.name + "]: " + ex.what(), *logger_);
        }
    }

    void ModelVariant::Unload() const {
        try {
            CallWithJson(core_, "unload_model", MakeModelParams(info_.name), *logger_);
        }
        catch (const std::exception& ex) {
            throw FoundryLocalException("Error unloading model [" + info_.name + "]: " + ex.what(), *logger_);
        }
    }

    bool ModelVariant::IsLoaded() const {
        std::vector<std::string> loadedModelIds = GetLoadedModelsInternal(core_, *logger_);
        for (auto& id : loadedModelIds) {
            auto pos = id.find_last_of(':');
            if (pos != std::string::npos) {
                id.erase(pos);
            }

            if (id == info_.name) {
                return true;
            }
        }

        return false;
    }

    bool ModelVariant::IsCached() const {
        auto cachedModels = GetCachedModelsInternal(core_, *logger_);
        for (auto& id : cachedModels) {
            StripSuffixAfterColon(id);
            if (id == info_.name) {
                return true;
            }
        }
        return false;
    }

    void ModelVariant::Download(DownloadProgressCallback onProgress) const {
        if (IsCached()) {
            logger_->Log(LogLevel::Information, "Model '" + info_.name + "' is already cached, skipping download.");
            return;
        }

        if (onProgress) {
            struct ProgressState {
                DownloadProgressCallback* cb;
                ILogger* logger;
            } state{ &onProgress, logger_ };

            auto nativeCallback = [](void* data, int32_t len, void* user) {
                if (!data || len <= 0)
                    return;
                auto* st = static_cast<ProgressState*>(user);
                std::string perc(static_cast<char*>(data), static_cast<size_t>((std::min)(4, static_cast<int>(len))));
                try {
                    float value = std::stof(perc);
                    (*(st->cb))(value);
                } catch (...) {
                    st->logger->Log(LogLevel::Warning, "Failed to parse download progress: " + perc);
                }
            };

            CallWithJsonAndCallback(core_, "download_model", MakeModelParams(info_.name), *logger_,
                reinterpret_cast<void*>(+nativeCallback), reinterpret_cast<void*>(&state));
        } else {
            CallWithJson(core_, "download_model", MakeModelParams(info_.name), *logger_);
        }
    }

    void ModelVariant::Load() const {
        CallWithJson(core_, "load_model", MakeModelParams(info_.name), *logger_);
    }

    const std::filesystem::path& ModelVariant::GetPath() const {
        if (cachedPath_.empty()) {
            cachedPath_ = std::filesystem::path(CallWithJson(core_, "get_model_path", MakeModelParams(info_.name), *logger_));
        }
        return cachedPath_;
    }

    const std::string& ModelVariant::GetId() const noexcept {
        return info_.id;
    }

    const std::string& ModelVariant::GetAlias() const noexcept {
        return info_.alias;
    }

    uint32_t ModelVariant::GetVersion() const noexcept {
        return info_.version;
    }

    AudioClient::AudioClient(gsl::not_null<const ModelVariant*> model)
        : AudioClient(model->core_, model->info_.name, model->logger_) {
        if (!model->IsLoaded()) {
            throw FoundryLocalException("Model " + model->info_.name + " is not loaded. Call Load() first.", *model->logger_);
        }
    }

    AudioClient ModelVariant::GetAudioClient() const {
        return AudioClient(this);
    }

    ChatClient::ChatClient(gsl::not_null<const ModelVariant*> model)
        : ChatClient(model->core_, model->info_.name, model->logger_) {
        if (!model->IsLoaded()) {
            throw FoundryLocalException("Model " + model->info_.name + " is not loaded. Call Load() first.", *model->logger_);
        }
    }

    ChatClient ModelVariant::GetChatClient() const {
        return ChatClient(this);
    }

    /// <summary>
    /// Model
    /// </summary>
    Model::Model(gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> core, gsl::not_null<ILogger*> logger)
        : core_(core), logger_(logger) {
    }

    ModelVariant& Model::SelectedVariant() {
        if (!selectedVariantIndex_ || *selectedVariantIndex_ >= variants_.size()) {
            throw FoundryLocalException("Model has no selected variant", *logger_);
        }
        return variants_[*selectedVariantIndex_];
    }

    const ModelVariant& Model::SelectedVariant() const {
        if (!selectedVariantIndex_ || *selectedVariantIndex_ >= variants_.size()) {
            throw FoundryLocalException("Model has no selected variant", *logger_);
        }
        return variants_[*selectedVariantIndex_];
    }

    gsl::span<const ModelVariant> Model::GetAllModelVariants() const {
        return variants_;
    }

    const ModelVariant* Model::GetLatestVariant(gsl::not_null<const ModelVariant*> variant) const {
        const auto& targetName = variant->GetInfo().name;

        for (const auto& v : variants_) {
            if (v.GetInfo().name == targetName) {
                return &v;
            }
        }

        throw FoundryLocalException(
            "Model " + GetAlias() + " does not have a " + variant->GetId() + " variant.", *logger_);
    }

    const std::string& Model::GetId() const {
        return SelectedVariant().GetId();
    }

    const std::string& Model::GetAlias() const {
        return SelectedVariant().GetAlias();
    }

    void Model::SelectVariant(gsl::not_null<const ModelVariant*> variant) const {
        auto it = std::find_if(variants_.begin(), variants_.end(),
            [&](const ModelVariant& v) { return &v == variant.get(); });

        if (it == variants_.end()) {
            throw FoundryLocalException("Model " + GetAlias() + " does not have a " + variant->GetId() + " variant.",
                *logger_);
        }

        selectedVariantIndex_ = static_cast<size_t>(std::distance(variants_.begin(), it));
    }

    /// <summary>
    /// Catalog
    /// </summary>

    Catalog::Catalog(gsl::not_null<FoundryLocal::Internal::IFoundryLocalCore*> injected, gsl::not_null<ILogger*> logger)
        : core_(injected), logger_(logger) {
        try {
            name_ = core_->call("get_catalog_name", *logger_, /*dataArgument*/ nullptr);
        }
        catch (const std::exception& ex) {
            throw FoundryLocalException(std::string("Error getting catalog name: ") + ex.what(), *logger_);
        }
    }

    std::vector<const ModelVariant*> Catalog::GetLoadedModels() const {
        return CollectVariantsByIds(modelIdToModelVariant_, GetLoadedModelsInternal(core_, *logger_));
    }

    std::vector<const ModelVariant*> Catalog::GetCachedModels() const {
        return CollectVariantsByIds(modelIdToModelVariant_, GetCachedModelsInternal(core_, *logger_));
    }

    const Model* Catalog::GetModel(std::string_view modelId) const {
        auto it = byAlias_.find(std::string(modelId));
        if (it != byAlias_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    std::vector<const Model*> Catalog::ListModels() const {
        UpdateModels();

        std::vector<const Model*> out;
        out.reserve(byAlias_.size());
        for (auto& kv : byAlias_)
            out.emplace_back(&kv.second);

        return out;
    }

    void Catalog::UpdateModels() const {
        using clock = std::chrono::steady_clock;

        // TODO: make this configurable
        constexpr auto kRefreshInterval = std::chrono::hours(6);

        const auto now = clock::now();
        if (lastFetch_.time_since_epoch() != clock::duration::zero() && (now - lastFetch_) < kRefreshInterval) {
            return;
        }

        const std::string raw = core_->call("get_model_list", *logger_);
        const auto arr = nlohmann::json::parse(raw);

        byAlias_.clear();
        modelIdToModelVariant_.clear();

        for (const auto& j : arr) {
            const std::string alias = j.at("alias").get<std::string>();
            if (alias.rfind("openai-", 0) == 0)
                continue;

            auto it = byAlias_.find(alias);
            if (it == byAlias_.end()) {
                Model m(core_, logger_);
                it = byAlias_.emplace(alias, std::move(m)).first;
            }

            ModelInfo modelVariantInfo;
            from_json(j, modelVariantInfo);
            std::string variantId = modelVariantInfo.name;
            ModelVariant modelVariant(core_, modelVariantInfo, logger_);
            modelIdToModelVariant_.emplace(variantId, modelVariant);

            it->second.variants_.emplace_back(std::move(modelVariant));
        }

        // Auto-select the first variant for each model.
        for (auto& [alias, model] : byAlias_) {
            if (!model.variants_.empty()) {
                model.selectedVariantIndex_ = 0;
            }
        }

        lastFetch_ = now;
    }

    const ModelVariant* Catalog::GetModelVariant(std::string_view id) const {
        auto it = modelIdToModelVariant_.find(std::string(id));
        if (it != modelIdToModelVariant_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    /// <summary>
    /// FoundryLocalManager
    /// </summary>

    FoundryLocalManager::FoundryLocalManager(Configuration configuration, ILogger* logger)
        : config_(std::move(configuration)), core_(std::make_unique<Core>()), logger_(logger ? logger : &defaultLogger_) {
        static_cast<Core*>(core_.get())->loadEmbedded();
        Initialize();
        catalog_ = Catalog::Create(core_.get(), logger_);
    }

    FoundryLocalManager::FoundryLocalManager(FoundryLocalManager&& other) noexcept
        : config_(std::move(other.config_)),
          core_(std::move(other.core_)),
          catalog_(std::move(other.catalog_)),
          logger_(other.OwnsLogger() ? &defaultLogger_ : other.logger_),
          urls_(std::move(other.urls_)) {
        other.logger_ = &other.defaultLogger_;
    }

    FoundryLocalManager& FoundryLocalManager::operator=(FoundryLocalManager&& other) noexcept {
        if (this != &other) {
            config_ = std::move(other.config_);
            core_ = std::move(other.core_);
            catalog_ = std::move(other.catalog_);
            logger_ = other.OwnsLogger() ? &defaultLogger_ : other.logger_;
            urls_ = std::move(other.urls_);
            other.logger_ = &other.defaultLogger_;
        }
        return *this;
    }

    FoundryLocalManager::~FoundryLocalManager() {
        // Unload all loaded models before tearing down.
        if (catalog_) {
            try {
                auto loadedModels = catalog_->GetLoadedModels();
                for (const auto* variant : loadedModels) {
                    try {
                        variant->Unload();
                    } catch (const std::exception& ex) {
                        logger_->Log(LogLevel::Warning,
                            std::string("Error unloading model during destruction: ") + ex.what());
                    }
                }
            } catch (const std::exception& ex) {
                logger_->Log(LogLevel::Warning,
                    std::string("Error retrieving loaded models during destruction: ") + ex.what());
            }
        }

        if (!urls_.empty()) {
            try {
                StopWebService();
            } catch (const std::exception& ex) {
                logger_->Log(LogLevel::Warning, std::string("Error stopping web service during destruction: ") + ex.what());
            }
        }
    }

    const Catalog& FoundryLocalManager::GetCatalog() const {
        return *catalog_;
    }

    void FoundryLocalManager::StartWebService() {
        if (!config_.web) {
            throw FoundryLocalException("Web service configuration was not provided.", *logger_);
        }

        try {
            std::string raw = core_->call("start_service", *logger_);
            auto arr = nlohmann::json::parse(raw);
            urls_ = arr.get<std::vector<std::string>>();
        } catch (const std::exception& ex) {
            throw FoundryLocalException(std::string("Error starting web service: ") + ex.what(), *logger_);
        }
    }

    void FoundryLocalManager::StopWebService() {
        if (!config_.web) {
            throw FoundryLocalException("Web service configuration was not provided.", *logger_);
        }

        try {
            core_->call("stop_service", *logger_);
            urls_.clear();
        } catch (const std::exception& ex) {
            throw FoundryLocalException(std::string("Error stopping web service: ") + ex.what(), *logger_);
        }
    }

    gsl::span<const std::string> FoundryLocalManager::GetUrls() const noexcept {
        return urls_;
    }

    void FoundryLocalManager::EnsureEpsDownloaded() const {
        try {
            core_->call("ensure_eps_downloaded", *logger_);
        } catch (const std::exception& ex) {
            throw FoundryLocalException(
                std::string("Error ensuring execution providers downloaded: ") + ex.what(), *logger_);
        }
    }

    void FoundryLocalManager::Initialize() {
        config_.Validate();

        try {
            CoreInteropRequest initReq("initialize");
            initReq.AddParam("AppName", config_.app_name);
            initReq.AddParam("LogLevel", std::string(LogLevelToString(config_.log_level)));

            if (config_.app_data_dir) {
                initReq.AddParam("AppDataDir", config_.app_data_dir->string());
            }
            if (config_.logs_dir) {
                initReq.AddParam("LogsDir", config_.logs_dir->string());
            }
            if (config_.web && config_.web->urls) {
                initReq.AddParam("WebServiceUrls", *config_.web->urls);
            }
            if (config_.additional_settings) {
                for (const auto& [key, value] : *config_.additional_settings) {
                    if (!key.empty()) {
                        initReq.AddParam(key, value);
                    }
                }
            }

            std::string initJson = initReq.ToJson();
            core_->call(initReq.Command(), *logger_, &initJson);

            if (config_.model_cache_dir) {
                std::string current = core_->call("get_cache_directory", *logger_);

                if (current != config_.model_cache_dir->string()) {
                    CoreInteropRequest setReq("set_cache_directory");
                    setReq.AddParam("Directory", config_.model_cache_dir->string());
                    std::string setJson = setReq.ToJson();
                    core_->call(setReq.Command(), *logger_, &setJson);

                    logger_->Log(LogLevel::Information,
                        std::string("Model cache directory updated: ") + config_.model_cache_dir->string());
                }
                else {
                    logger_->Log(LogLevel::Information, std::string("Model cache directory already set to: ") + current);
                }
            }
        }
        catch (const std::exception& ex) {
            throw FoundryLocalException(std::string("FoundryLocalManager::Initialize failed: ") + ex.what(), *logger_);
        }
    }

} // namespace FoundryLocal
