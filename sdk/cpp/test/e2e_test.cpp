// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// End-to-end tests that exercise the public API with the real Core DLL.
// Tests marked DISABLED_ are skipped in CI (no Core DLL / no network).
// Run locally with:  --gtest_also_run_disabled_tests

#include <gtest/gtest.h>

#include "foundry_local.h"

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace foundry_local;

// ---------------------------------------------------------------------------
// Helper: detect CI environment (mirrors C# SkipInCI logic)
// ---------------------------------------------------------------------------
static bool IsRunningInCI() {
    auto check = [](const char* var) -> bool {
        const char* val = std::getenv(var);
        if (!val)
            return false;
        std::string s(val);
        for (auto& c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s == "true" || s == "1";
    };
    return check("TF_BUILD") || check("GITHUB_ACTIONS") || check("CI");
}

// ---------------------------------------------------------------------------
// Fixture: creates a real Manager with the Core DLL.
// All tests in this fixture require the native DLLs next to the test binary.
// ---------------------------------------------------------------------------
class EndToEndTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        Configuration config("CppSdkE2ETest");
        config.log_level = LogLevel::Information;
        try {
            Manager::Create(std::move(config));
        }
        catch (const std::exception& ex) {
            std::cerr << "[E2E] Failed to create Manager: " << ex.what() << "\n";
            GTEST_SKIP() << "Core DLL not available: " << ex.what();
        }
    }

    static void TearDownTestSuite() { Manager::Destroy(); }

    void SetUp() override {
        if (!Manager::IsInitialized()) {
            GTEST_SKIP() << "Manager not available (Core DLL missing?)";
        }
    }

    static bool IsAudioModel(const std::string& alias) { return alias.find("whisper") != std::string::npos; }

    /// Find a chat-capable model, preferring cached, then known small models, then any.
    /// Selects the CPU variant when available to avoid GPU/EP dependency issues.
    static IModel* FindChatModel(Catalog& catalog) {
        IModel* target = nullptr;

        auto cached = catalog.GetCachedModels();
        for (auto* variant : cached) {
            if (!IsAudioModel(variant->GetAlias())) {
                target = catalog.GetModel(variant->GetAlias());
                if (target)
                    break;
            }
        }

        if (!target) {
            for (const auto& alias : {"qwen2.5-0.5b", "qwen2.5-coder-0.5b", "phi-4-mini"}) {
                target = catalog.GetModel(alias);
                if (target)
                    break;
            }
        }

        if (!target) {
            auto models = catalog.ListModels();
            for (auto* model : models) {
                if (!IsAudioModel(model->GetAlias())) {
                    target = model;
                    break;
                }
            }
        }

        if (target) {
            auto* model = dynamic_cast<Model*>(target);
            if (model) {
                for (const auto& variant : model->GetAllModelVariants()) {
                    if (variant.GetInfo().runtime.has_value() &&
                        variant.GetInfo().runtime->device_type == DeviceType::CPU) {
                        model->SelectVariant(variant);
                        break;
                    }
                }
            }
        }

        return target;
    }

    /// Find an audio model, preferring cached.
    static IModel* FindAudioModel(Catalog& catalog) {
        IModel* target = nullptr;

        auto cached = catalog.GetCachedModels();
        for (auto* variant : cached) {
            if (IsAudioModel(variant->GetAlias())) {
                target = catalog.GetModel(variant->GetAlias());
                if (target)
                    break;
            }
        }

        if (!target) {
            for (const auto& alias : {"whisper-small", "whisper-tiny"}) {
                target = catalog.GetModel(alias);
                if (target)
                    break;
            }
        }

        return target;
    }
};

// ===========================================================================
// Catalog tests (no model download required)
// ===========================================================================

TEST_F(EndToEndTest, BrowseCatalog_ListsModels) {
auto& catalog = Manager::Instance().GetCatalog();
    EXPECT_FALSE(catalog.GetName().empty());

    auto models = catalog.ListModels();
    EXPECT_GT(models.size(), 0u) << "Catalog should have at least one model";

    for (const auto* model : models) {
        EXPECT_FALSE(model->GetAlias().empty());
        auto* concreteModel = dynamic_cast<const Model*>(model);
        ASSERT_NE(nullptr, concreteModel);
        EXPECT_FALSE(concreteModel->GetAllModelVariants().empty());

        for (const auto& variant : concreteModel->GetAllModelVariants()) {
            const auto& info = variant.GetInfo();
            EXPECT_FALSE(info.id.empty());
            EXPECT_FALSE(info.name.empty());
            EXPECT_FALSE(info.alias.empty());
            EXPECT_FALSE(info.provider_type.empty());
            EXPECT_FALSE(info.model_type.empty());
        }
    }
}

TEST_F(EndToEndTest, GetCachedModels_Succeeds) {
auto& catalog = Manager::Instance().GetCatalog();
    auto cached = catalog.GetCachedModels();
    for (auto* variant : cached) {
        EXPECT_FALSE(variant->GetId().empty());
        EXPECT_TRUE(variant->IsCached());
    }
}

TEST_F(EndToEndTest, GetLoadedModels_Succeeds) {
auto& catalog = Manager::Instance().GetCatalog();
    auto loaded = catalog.GetLoadedModels();
    for (auto* variant : loaded) {
        EXPECT_FALSE(variant->GetId().empty());
        EXPECT_TRUE(variant->IsLoaded());
    }
}

TEST_F(EndToEndTest, GetModel_NotFound_ReturnsNull) {
auto& catalog = Manager::Instance().GetCatalog();
    auto* model = catalog.GetModel("this-model-does-not-exist-12345");
    EXPECT_EQ(model, nullptr);
}

TEST_F(EndToEndTest, GetModelVariant_NotFound_ReturnsNull) {
auto& catalog = Manager::Instance().GetCatalog();
    auto* variant = catalog.GetModelVariant("nonexistent-model:999");
    EXPECT_EQ(variant, nullptr);
}

TEST_F(EndToEndTest, GetModelVariant_Found) {
auto& catalog = Manager::Instance().GetCatalog();
    auto models = catalog.ListModels();
    if (models.empty()) {
        GTEST_SKIP() << "No models in catalog";
    }

    const auto* firstConcreteModel = dynamic_cast<const Model*>(models[0]);
    ASSERT_NE(nullptr, firstConcreteModel);
    const auto& firstVariant = firstConcreteModel->GetAllModelVariants()[0];
    auto* found = catalog.GetModelVariant(firstVariant.GetId());
    ASSERT_NE(nullptr, found);
    EXPECT_EQ(firstVariant.GetId(), found->GetId());
}

TEST_F(EndToEndTest, ModelVariantInfo_HasRequiredFields) {
auto& catalog = Manager::Instance().GetCatalog();
    auto models = catalog.ListModels();
    if (models.empty()) {
        GTEST_SKIP() << "No models in catalog";
    }

    for (const auto* model : models) {
        auto* concreteModel = dynamic_cast<const Model*>(model);
        ASSERT_NE(nullptr, concreteModel);
        for (const auto& variant : concreteModel->GetAllModelVariants()) {
            const auto& info = variant.GetInfo();
            EXPECT_FALSE(info.id.empty());
            EXPECT_FALSE(info.name.empty());
            EXPECT_GT(info.version, 0u);
            EXPECT_FALSE(info.alias.empty());
            EXPECT_FALSE(info.uri.empty());
        }
    }
}

TEST_F(EndToEndTest, ModelVariant_SelectVariant) {
auto& catalog = Manager::Instance().GetCatalog();
    auto models = catalog.ListModels();

    // Find a model with multiple variants
    Model* multiVariantModel = nullptr;
    for (auto* model : models) {
        auto* concreteModel = dynamic_cast<Model*>(model);
        if (concreteModel && concreteModel->GetAllModelVariants().size() > 1) {
            multiVariantModel = concreteModel;
            break;
        }
    }

    if (!multiVariantModel) {
        GTEST_SKIP() << "No model with multiple variants found";
    }

    const auto& variants = multiVariantModel->GetAllModelVariants();
    const auto& secondVariant = variants[1];
    multiVariantModel->SelectVariant(secondVariant);
    EXPECT_EQ(secondVariant.GetId(), multiVariantModel->GetId());

    // Select back the first variant
    multiVariantModel->SelectVariant(variants[0]);
    EXPECT_EQ(variants[0].GetId(), multiVariantModel->GetId());
}

// ===========================================================================
// EnsureEpsDownloaded (no model download, but may download EPs)
// ===========================================================================

TEST_F(EndToEndTest, DISABLED_EnsureEpsDownloaded_Succeeds) {
    if (IsRunningInCI()) {
        GTEST_SKIP() << "Skipped in CI (may require network)";
    }

    EXPECT_NO_THROW(Manager::Instance().EnsureEpsDownloaded());
}

// ===========================================================================
// Web service tests
// ===========================================================================

TEST_F(EndToEndTest, DISABLED_WebService_StartAndStop) {
    if (IsRunningInCI()) {
        GTEST_SKIP() << "Skipped in CI";
    }

    auto& manager = Manager::Instance();

    // GetUrls should be empty before starting
    EXPECT_TRUE(manager.GetUrls().empty());

    // StartWebService without web config should throw
    // Note: the manager was created without web config, so this verifies the guard.
    EXPECT_THROW(manager.StartWebService(), Exception);
}

// ===========================================================================
// Download, load, chat (non-streaming), unload
// ===========================================================================

TEST_F(EndToEndTest, DISABLED_DownloadLoadChatUnload) {
if (IsRunningInCI()) {
    GTEST_SKIP() << "Skipped in CI (requires model download)";
}

auto& catalog = Manager::Instance().GetCatalog();
    auto* target = FindChatModel(catalog);
    if (!target) {
        GTEST_SKIP() << "No chat-capable model found in catalog";
    }

    std::cout << "[E2E] Using model: " << target->GetAlias() << " variant: " << target->GetId() << "\n";

    // Download (no-op if already cached)
    bool progressCallbackInvoked = false;
    target->Download([&](float pct) {
        progressCallbackInvoked = true;
        std::cout << "\r[E2E] Download: " << pct << "%   " << std::flush;
    });
    std::cout << "\n";

    EXPECT_TRUE(target->IsCached());

    // Load
    target->Load();
    EXPECT_TRUE(target->IsLoaded());

    // Verify it appears in loaded models
    auto loaded = catalog.GetLoadedModels();
    bool foundInLoaded = false;
    for (auto* v : loaded) {
        if (v->GetId() == target->GetId()) {
            foundInLoaded = true;
            break;
        }
    }
    EXPECT_TRUE(foundInLoaded) << "Model should appear in GetLoadedModels() after Load()";

    // Chat (non-streaming)
    OpenAIChatClient client(*target);

    std::vector<ChatMessage> messages = {{"user", "Say hello in one word.", {}}};
    ChatSettings settings;
    settings.max_tokens = 32;
    auto response = client.CompleteChat(messages, settings);
    EXPECT_TRUE(response.successful);
    ASSERT_FALSE(response.choices.empty());
    ASSERT_TRUE(response.choices[0].message.has_value());
    EXPECT_FALSE(response.choices[0].message->content.empty());
    EXPECT_EQ(FinishReason::Stop, response.choices[0].finish_reason);
    std::cout << "[E2E] Response: " << response.choices[0].message->content << "\n";

    // Unload
    target->Unload();
    EXPECT_FALSE(target->IsLoaded());
}

// ===========================================================================
// Streaming chat
// ===========================================================================

TEST_F(EndToEndTest, DISABLED_StreamingChat) {
if (IsRunningInCI()) {
    GTEST_SKIP() << "Skipped in CI (requires model download)";
}

auto& catalog = Manager::Instance().GetCatalog();
    auto* target = FindChatModel(catalog);
    if (!target) {
        GTEST_SKIP() << "No chat-capable model found in catalog";
    }

    target->Download();
    target->Load();
    ASSERT_TRUE(target->IsLoaded());

    std::cout << "[E2E] Streaming with model: " << target->GetAlias() << "\n";

    OpenAIChatClient client(*target);

    std::vector<ChatMessage> messages = {{"user", "Count from 1 to 5.", {}}};
    ChatSettings settings;
    settings.max_tokens = 64;
    settings.temperature = 0.0f;

    std::vector<ChatCompletionCreateResponse> chunks;
    std::string fullContent;
    client.CompleteChatStreaming(messages, settings, [&](const ChatCompletionCreateResponse& chunk) {
        chunks.push_back(chunk);
        if (!chunk.choices.empty() && chunk.choices[0].delta.has_value() && !chunk.choices[0].delta->content.empty()) {
            fullContent += chunk.choices[0].delta->content;
        }
    });

    EXPECT_GT(chunks.size(), 0u) << "Should have received at least one streaming chunk";
    EXPECT_FALSE(fullContent.empty()) << "Accumulated streaming content should not be empty";
    std::cout << "[E2E] Streaming response: " << fullContent << "\n";

    // Last chunk should have a stop finish reason
    ASSERT_FALSE(chunks.empty());
    const auto& lastChunk = chunks.back();
    if (!lastChunk.choices.empty()) {
        EXPECT_EQ(FinishReason::Stop, lastChunk.choices[0].finish_reason);
    }

    target->Unload();
}

// ===========================================================================
// Chat with tool calling
// ===========================================================================

TEST_F(EndToEndTest, DISABLED_ChatWithToolCalling) {
    if (IsRunningInCI()) {
        GTEST_SKIP() << "Skipped in CI (requires model download)";
    }

    auto& catalog = Manager::Instance().GetCatalog();
    auto* target = FindChatModel(catalog);
    if (!target) {
        GTEST_SKIP() << "No chat-capable model found in catalog";
    }

    // Check if the selected variant supports tool calling
    bool supportsCalling = false;
    auto* targetModel = dynamic_cast<Model*>(target);
    if (targetModel) {
        for (const auto& v : targetModel->GetAllModelVariants()) {
            if (v.GetInfo().supports_tool_calling.has_value() && *v.GetInfo().supports_tool_calling) {
                supportsCalling = true;
                break;
            }
        }
    }
    if (!supportsCalling) {
        GTEST_SKIP() << "Model does not support tool calling";
    }

    target->Download();
    target->Load();
    ASSERT_TRUE(target->IsLoaded());

    std::cout << "[E2E] Tool calling with model: " << target->GetAlias() << "\n";

    OpenAIChatClient client(*target);

    std::vector<ToolDefinition> tools = {
        {"function", FunctionDefinition{"get_weather", "Get the current weather for a city.",
                                        PropertyDefinition{"object", std::nullopt,
                                                           std::unordered_map<std::string, PropertyDefinition>{
                                                               {"city", PropertyDefinition{"string", "The city name"}}},
                                                           std::vector<std::string>{"city"}}}}};

    std::vector<ChatMessage> messages = {
        {"system", "You are a helpful assistant. Use the provided tools when asked about weather."},
        {"user", "What is the weather in Seattle?"}};

    ChatSettings settings;
    settings.temperature = 0.0f;
    settings.max_tokens = 256;
    settings.tool_choice = ToolChoiceKind::Required;

    auto response = client.CompleteChat(messages, tools, settings);
    EXPECT_TRUE(response.successful);
    ASSERT_FALSE(response.choices.empty());

    const auto& choice = response.choices[0];
    // With tool_choice = Required, the model should produce a tool call
    if (choice.finish_reason == FinishReason::ToolCalls) {
        ASSERT_TRUE(choice.message.has_value());
        ASSERT_FALSE(choice.message->tool_calls.empty());
        const auto& tc = choice.message->tool_calls[0];
        EXPECT_FALSE(tc.id.empty());
        ASSERT_TRUE(tc.function_call.has_value());
        EXPECT_EQ("get_weather", tc.function_call->name);
        EXPECT_FALSE(tc.function_call->arguments.empty());
        std::cout << "[E2E] Tool call: " << tc.function_call->name << " args: " << tc.function_call->arguments << "\n";
    }

    target->Unload();
}

// ===========================================================================
// Audio transcription
// ===========================================================================

TEST_F(EndToEndTest, DISABLED_AudioTranscription) {
if (IsRunningInCI()) {
    GTEST_SKIP() << "Skipped in CI (requires model download + audio file)";
}

auto& catalog = Manager::Instance().GetCatalog();
    auto* target = FindAudioModel(catalog);
    if (!target) {
        GTEST_SKIP() << "No audio model found in catalog";
    }

    target->Download();
    target->Load();
    ASSERT_TRUE(target->IsLoaded());

    std::cout << "[E2E] Audio model: " << target->GetAlias() << "\n";

    OpenAIAudioClient client(*target);

    // Note: this test requires a valid audio file to be present.
    // Skip if no test audio file is available.
    const char* audioPath = std::getenv("FL_TEST_AUDIO_PATH");
    if (!audioPath) {
        target->Unload();
        GTEST_SKIP() << "Set FL_TEST_AUDIO_PATH env var to a .wav file to run audio tests";
    }

    auto result = client.TranscribeAudio(audioPath);
    EXPECT_FALSE(result.text.empty());
    std::cout << "[E2E] Transcription: " << result.text << "\n";

    target->Unload();
}

TEST_F(EndToEndTest, DISABLED_AudioTranscriptionStreaming) {
if (IsRunningInCI()) {
    GTEST_SKIP() << "Skipped in CI (requires model download + audio file)";
}

auto& catalog = Manager::Instance().GetCatalog();
    auto* target = FindAudioModel(catalog);
    if (!target) {
        GTEST_SKIP() << "No audio model found in catalog";
    }

    target->Download();
    target->Load();
    ASSERT_TRUE(target->IsLoaded());

    const char* audioPath = std::getenv("FL_TEST_AUDIO_PATH");
    if (!audioPath) {
        target->Unload();
        GTEST_SKIP() << "Set FL_TEST_AUDIO_PATH env var to a .wav file to run audio tests";
    }

    OpenAIAudioClient client(*target);

    std::string fullText;
    int chunkCount = 0;
    client.TranscribeAudioStreaming(audioPath, [&](const AudioCreateTranscriptionResponse& chunk) {
        fullText += chunk.text;
        chunkCount++;
    });

    EXPECT_GT(chunkCount, 0) << "Should have received at least one streaming chunk";
    EXPECT_FALSE(fullText.empty());
    std::cout << "[E2E] Streaming transcription (" << chunkCount << " chunks): " << fullText << "\n";

    target->Unload();
}

// ===========================================================================
// RemoveFromCache
// ===========================================================================

TEST_F(EndToEndTest, DISABLED_DownloadAndRemoveFromCache) {
    if (IsRunningInCI()) {
        GTEST_SKIP() << "Skipped in CI (requires model download)";
    }

    auto& catalog = Manager::Instance().GetCatalog();
    auto* target = FindChatModel(catalog);
    if (!target) {
        GTEST_SKIP() << "No chat-capable model found in catalog";
    }

    target->Download();
    EXPECT_TRUE(target->IsCached());

    // RemoveFromCache should succeed without throwing.
    EXPECT_NO_THROW(target->RemoveFromCache());

    std::cout << "[E2E] RemoveFromCache completed for: " << target->GetAlias()
              << " (IsCached=" << (target->IsCached() ? "true" : "false") << ")\n";
}
