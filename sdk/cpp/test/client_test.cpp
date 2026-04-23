// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <gtest/gtest.h>

#include "mock_core.h"
#include "mock_object_factory.h"
#include "parser.h"
#include "foundry_local_exception.h"

#include <nlohmann/json.hpp>

using namespace foundry_local;
using namespace foundry_local::Testing;

using Factory = MockObjectFactory;

class OpenAIChatClientTest : public ::testing::Test {
protected:
    MockCore core_;
    NullLogger logger_;

    std::string MakeChatResponseJson(const std::string& content = "Hello!") {
        nlohmann::json resp = {
            {"created", 1700000000},
            {"id", "chatcmpl-test"},
            {"IsDelta", false},
            {"Successful", true},
            {"HttpStatusCode", 200},
            {"choices",
             {{{"index", 0}, {"finish_reason", "stop"}, {"message", {{"role", "assistant"}, {"content", content}}}}}}};
        return resp.dump();
    }

    ModelVariant MakeLoadedVariant(const std::string& name = "chat-model") {
        core_.OnCall("list_loaded_models", "[\"" + name + ":1\"]");
        return Factory::CreateModelVariant(&core_, Factory::MakeModelInfo(name, "alias"), &logger_);
    }
};

TEST_F(OpenAIChatClientTest, CompleteChat_BasicResponse) {
    core_.OnCall("chat_completions", MakeChatResponseJson("Hello world!"));
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    std::vector<ChatMessage> messages = {{"user", "Say hello", {}}};
    ChatSettings settings;
    auto response = client.CompleteChat(messages, settings);

    EXPECT_TRUE(response.successful);
    ASSERT_EQ(1u, response.choices.size());
    EXPECT_EQ("Hello world!", response.choices[0].message->content);
}

TEST_F(OpenAIChatClientTest, CompleteChat_WithSettings) {
    core_.OnCall("chat_completions", MakeChatResponseJson());
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    std::vector<ChatMessage> messages = {{"user", "test", {}}};
    ChatSettings settings;
    settings.temperature = 0.7f;
    settings.max_tokens = 100;
    settings.top_p = 0.9f;
    settings.frequency_penalty = 0.5f;
    settings.presence_penalty = 0.3f;
    settings.n = 2;
    settings.random_seed = 42;
    settings.top_k = 10;

    auto response = client.CompleteChat(messages, settings);

    // Verify the request JSON contains the settings
    auto requestJson = nlohmann::json::parse(core_.GetLastDataArg("chat_completions"));
    auto openAiReq = nlohmann::json::parse(requestJson["Params"]["OpenAICreateRequest"].get<std::string>());

    EXPECT_NEAR(0.7f, openAiReq["temperature"].get<float>(), 0.001f);
    EXPECT_EQ(100, openAiReq["max_completion_tokens"].get<int>());
    EXPECT_NEAR(0.9f, openAiReq["top_p"].get<float>(), 0.001f);
    EXPECT_NEAR(0.5f, openAiReq["frequency_penalty"].get<float>(), 0.001f);
    EXPECT_NEAR(0.3f, openAiReq["presence_penalty"].get<float>(), 0.001f);
    EXPECT_EQ(2, openAiReq["n"].get<int>());
    EXPECT_EQ(42, openAiReq["seed"].get<int>());
    EXPECT_EQ(10, openAiReq["metadata"]["top_k"].get<int>());
}

TEST_F(OpenAIChatClientTest, CompleteChat_RequestFormat) {
    core_.OnCall("chat_completions", MakeChatResponseJson());
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    std::vector<ChatMessage> messages = {{"system", "You are helpful", {}}, {"user", "Hello", {}}};
    ChatSettings settings;
    auto response = client.CompleteChat(messages, settings);

    auto requestJson = nlohmann::json::parse(core_.GetLastDataArg("chat_completions"));
    auto openAiReq = nlohmann::json::parse(requestJson["Params"]["OpenAICreateRequest"].get<std::string>());

    EXPECT_EQ("chat-model", openAiReq["model"].get<std::string>());
    EXPECT_FALSE(openAiReq["stream"].get<bool>());
    ASSERT_EQ(2u, openAiReq["messages"].size());
    EXPECT_EQ("system", openAiReq["messages"][0]["role"].get<std::string>());
    EXPECT_EQ("user", openAiReq["messages"][1]["role"].get<std::string>());
}

TEST_F(OpenAIChatClientTest, CompleteChatStreaming) {
    nlohmann::json chunk1 = {
        {"created", 1700000000},
        {"id", "chatcmpl-1"},
        {"IsDelta", true},
        {"Successful", true},
        {"HttpStatusCode", 200},
        {"choices",
         {{{"index", 0}, {"finish_reason", nullptr}, {"delta", {{"role", "assistant"}, {"content", "Hello"}}}}}}};
    nlohmann::json chunk2 = {
        {"created", 1700000000},
        {"id", "chatcmpl-1"},
        {"IsDelta", true},
        {"Successful", true},
        {"HttpStatusCode", 200},
        {"choices", {{{"index", 0}, {"finish_reason", "stop"}, {"delta", {{"content", " world"}}}}}}};

    core_.OnCall("chat_completions",
                 [&](std::string_view, const std::string*, NativeCallbackFn callback, void* userData) -> std::string {
                     if (callback && userData) {
                         std::string s1 = chunk1.dump();
                         std::string s2 = chunk2.dump();
                         callback(s1.data(), static_cast<int32_t>(s1.size()), userData);
                         callback(s2.data(), static_cast<int32_t>(s2.size()), userData);
                     }
                     return "";
                 });
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    std::vector<ChatMessage> messages = {{"user", "test", {}}};
    ChatSettings settings;

    std::vector<ChatCompletionCreateResponse> chunks;
    client.CompleteChatStreaming(messages, settings,
                                 [&](const ChatCompletionCreateResponse& chunk) { chunks.push_back(chunk); });

    ASSERT_EQ(2u, chunks.size());
    EXPECT_TRUE(chunks[0].is_delta);
    ASSERT_TRUE(chunks[0].choices[0].delta.has_value());
    EXPECT_EQ("Hello", chunks[0].choices[0].delta->content);
    EXPECT_EQ(" world", chunks[1].choices[0].delta->content);
}

TEST_F(OpenAIChatClientTest, CompleteChatStreaming_PropagatesCallbackException) {
    nlohmann::json chunk = {
        {"created", 1700000000},
        {"id", "chatcmpl-1"},
        {"IsDelta", true},
        {"Successful", true},
        {"HttpStatusCode", 200},
        {"choices",
         {{{"index", 0}, {"finish_reason", nullptr}, {"delta", {{"role", "assistant"}, {"content", "Hi"}}}}}}};

    core_.OnCall("chat_completions",
                 [&](std::string_view, const std::string*, NativeCallbackFn callback, void* userData) -> std::string {
                     if (callback && userData) {
                         std::string s = chunk.dump();
                         callback(s.data(), static_cast<int32_t>(s.size()), userData);
                     }
                     return "";
                 });
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    std::vector<ChatMessage> messages = {{"user", "test", {}}};
    ChatSettings settings;

    EXPECT_THROW(client.CompleteChatStreaming(
                     messages, settings,
                     [](const ChatCompletionCreateResponse&) { throw std::runtime_error("callback error"); }),
                 std::runtime_error);
}

TEST_F(OpenAIChatClientTest, Constructor_ThrowsIfNotLoaded) {
    core_.OnCall("list_loaded_models", R"([])");
    auto variant = Factory::CreateModelVariant(&core_, Factory::MakeModelInfo("unloaded-model", "alias"), &logger_);
    EXPECT_THROW(OpenAIChatClient client(variant), Exception);
}

TEST_F(OpenAIChatClientTest, GetModelId) {
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");
    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);
    EXPECT_EQ("chat-model", client.GetModelId());
}

// ---------- Tool calling tests ----------

TEST_F(OpenAIChatClientTest, CompleteChat_WithTools_IncludesToolsInRequest) {
    core_.OnCall("chat_completions", MakeChatResponseJson());
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    std::vector<ChatMessage> messages = {{"user", "What is 7 * 6?", {}}};

    std::vector<ToolDefinition> tools = {
        {"function",
         FunctionDefinition{"multiply_numbers", "A tool for multiplying two numbers.",
                            PropertyDefinition{"object", std::nullopt,
                                               std::unordered_map<std::string, PropertyDefinition>{
                                                   {"first", PropertyDefinition{"integer", "The first number"}},
                                                   {"second", PropertyDefinition{"integer", "The second number"}}},
                                               std::vector<std::string>{"first", "second"}}}}};

    ChatSettings settings;
    settings.tool_choice = ToolChoiceKind::Required;

    auto response = client.CompleteChat(messages, tools, settings);

    // Verify the request JSON contains tools and tool_choice
    auto requestJson = nlohmann::json::parse(core_.GetLastDataArg("chat_completions"));
    auto openAiReq = nlohmann::json::parse(requestJson["Params"]["OpenAICreateRequest"].get<std::string>());

    ASSERT_TRUE(openAiReq.contains("tools"));
    ASSERT_TRUE(openAiReq["tools"].is_array());
    EXPECT_EQ(1u, openAiReq["tools"].size());
    EXPECT_EQ("function", openAiReq["tools"][0]["type"].get<std::string>());
    EXPECT_EQ("multiply_numbers", openAiReq["tools"][0]["function"]["name"].get<std::string>());
    EXPECT_EQ("A tool for multiplying two numbers.",
              openAiReq["tools"][0]["function"]["description"].get<std::string>());
    EXPECT_EQ("object", openAiReq["tools"][0]["function"]["parameters"]["type"].get<std::string>());
    EXPECT_TRUE(openAiReq["tools"][0]["function"]["parameters"].contains("properties"));
    EXPECT_TRUE(openAiReq["tools"][0]["function"]["parameters"]["properties"].contains("first"));
    EXPECT_TRUE(openAiReq["tools"][0]["function"]["parameters"]["properties"].contains("second"));

    EXPECT_EQ("required", openAiReq["tool_choice"].get<std::string>());
}

TEST_F(OpenAIChatClientTest, CompleteChat_WithoutTools_OmitsToolsField) {
    core_.OnCall("chat_completions", MakeChatResponseJson());
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    std::vector<ChatMessage> messages = {{"user", "Hello", {}}};
    ChatSettings settings;
    auto response = client.CompleteChat(messages, settings);

    auto requestJson = nlohmann::json::parse(core_.GetLastDataArg("chat_completions"));
    auto openAiReq = nlohmann::json::parse(requestJson["Params"]["OpenAICreateRequest"].get<std::string>());

    EXPECT_FALSE(openAiReq.contains("tools"));
    EXPECT_FALSE(openAiReq.contains("tool_choice"));
}

TEST_F(OpenAIChatClientTest, CompleteChat_ToolCallResponse_Parsed) {
    // Simulate a response with tool calls from the model
    nlohmann::json resp = {
        {"created", 1700000000},
        {"id", "chatcmpl-tool"},
        {"IsDelta", false},
        {"Successful", true},
        {"HttpStatusCode", 200},
        {"choices",
         {{{"index", 0},
           {"finish_reason", "tool_calls"},
           {"message",
            {{"role", "assistant"},
             {"content", "<tool_call>[{\"name\": \"multiply_numbers\", \"parameters\": {\"first\": 7, \"second\": "
                         "6}}]</tool_call>"},
             {"tool_calls",
              {{{"id", "call_1"},
                {"type", "function"},
                {"function", {{"name", "multiply_numbers"}, {"arguments", "{\"first\": 7, \"second\": 6}"}}}}}}}}}}}};

    core_.OnCall("chat_completions", resp.dump());
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    std::vector<ChatMessage> messages = {{"user", "What is 7 * 6?", {}}};
    ChatSettings settings;
    auto response = client.CompleteChat(messages, settings);

    ASSERT_EQ(1u, response.choices.size());
    EXPECT_EQ(FinishReason::ToolCalls, response.choices[0].finish_reason);
    ASSERT_TRUE(response.choices[0].message.has_value());

    const auto& msg = *response.choices[0].message;
    ASSERT_EQ(1u, msg.tool_calls.size());
    EXPECT_EQ("call_1", msg.tool_calls[0].id);
    EXPECT_EQ("function", msg.tool_calls[0].type);
    ASSERT_TRUE(msg.tool_calls[0].function_call.has_value());
    EXPECT_EQ("multiply_numbers", msg.tool_calls[0].function_call->name);
    EXPECT_EQ("{\"first\": 7, \"second\": 6}", msg.tool_calls[0].function_call->arguments);
}

TEST_F(OpenAIChatClientTest, CompleteChat_ToolChoiceAuto) {
    core_.OnCall("chat_completions", MakeChatResponseJson());
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    std::vector<ChatMessage> messages = {{"user", "test", {}}};
    ChatSettings settings;
    settings.tool_choice = ToolChoiceKind::Auto;

    client.CompleteChat(messages, settings);

    auto requestJson = nlohmann::json::parse(core_.GetLastDataArg("chat_completions"));
    auto openAiReq = nlohmann::json::parse(requestJson["Params"]["OpenAICreateRequest"].get<std::string>());
    EXPECT_EQ("auto", openAiReq["tool_choice"].get<std::string>());
}

TEST_F(OpenAIChatClientTest, CompleteChat_ToolChoiceNone) {
    core_.OnCall("chat_completions", MakeChatResponseJson());
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    std::vector<ChatMessage> messages = {{"user", "test", {}}};
    ChatSettings settings;
    settings.tool_choice = ToolChoiceKind::None;

    client.CompleteChat(messages, settings);

    auto requestJson = nlohmann::json::parse(core_.GetLastDataArg("chat_completions"));
    auto openAiReq = nlohmann::json::parse(requestJson["Params"]["OpenAICreateRequest"].get<std::string>());
    EXPECT_EQ("none", openAiReq["tool_choice"].get<std::string>());
}

TEST_F(OpenAIChatClientTest, CompleteChat_ToolMessageWithToolCallId) {
    core_.OnCall("chat_completions", MakeChatResponseJson());
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    ChatMessage toolMsg;
    toolMsg.role = "tool";
    toolMsg.content = "42";
    toolMsg.tool_call_id = "call_1";

    std::vector<ChatMessage> messages = {{"user", "What is 7 * 6?", {}}, std::move(toolMsg)};
    ChatSettings settings;
    client.CompleteChat(messages, settings);

    auto requestJson = nlohmann::json::parse(core_.GetLastDataArg("chat_completions"));
    auto openAiReq = nlohmann::json::parse(requestJson["Params"]["OpenAICreateRequest"].get<std::string>());

    ASSERT_EQ(2u, openAiReq["messages"].size());
    EXPECT_FALSE(openAiReq["messages"][0].contains("tool_call_id"));
    EXPECT_EQ("call_1", openAiReq["messages"][1]["tool_call_id"].get<std::string>());
    EXPECT_EQ("tool", openAiReq["messages"][1]["role"].get<std::string>());
}

TEST_F(OpenAIChatClientTest, CompleteChat_AssistantToolCallsSerialized) {
    // Multi-turn tool calling: the assistant message with tool_calls must be sent back
    // alongside the tool result message for the model to match the tool response.
    core_.OnCall("chat_completions", MakeChatResponseJson("The answer is 42."));
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    ChatMessage assistantMsg;
    assistantMsg.role = "assistant";
    assistantMsg.content = "";
    assistantMsg.tool_calls = {
        {"call_1", "function", FunctionCall{"multiply_numbers", "{\"first\": 7, \"second\": 6}"}}};

    ChatMessage toolMsg;
    toolMsg.role = "tool";
    toolMsg.content = "42";
    toolMsg.tool_call_id = "call_1";

    std::vector<ChatMessage> messages = {
        {"user", "What is 7 * 6?", {}}, std::move(assistantMsg), std::move(toolMsg)};
    ChatSettings settings;
    client.CompleteChat(messages, settings);

    auto requestJson = nlohmann::json::parse(core_.GetLastDataArg("chat_completions"));
    auto openAiReq = nlohmann::json::parse(requestJson["Params"]["OpenAICreateRequest"].get<std::string>());

    ASSERT_EQ(3u, openAiReq["messages"].size());

    // User message: no tool_calls
    EXPECT_FALSE(openAiReq["messages"][0].contains("tool_calls"));

    // Assistant message: must include tool_calls
    const auto& assistantJson = openAiReq["messages"][1];
    EXPECT_EQ("assistant", assistantJson["role"].get<std::string>());
    ASSERT_TRUE(assistantJson.contains("tool_calls"));
    ASSERT_TRUE(assistantJson["tool_calls"].is_array());
    ASSERT_EQ(1u, assistantJson["tool_calls"].size());
    EXPECT_EQ("call_1", assistantJson["tool_calls"][0]["id"].get<std::string>());
    EXPECT_EQ("function", assistantJson["tool_calls"][0]["type"].get<std::string>());
    EXPECT_EQ("multiply_numbers", assistantJson["tool_calls"][0]["function"]["name"].get<std::string>());
    EXPECT_EQ("{\"first\": 7, \"second\": 6}",
              assistantJson["tool_calls"][0]["function"]["arguments"].get<std::string>());

    // Tool message: must include tool_call_id
    const auto& toolJson = openAiReq["messages"][2];
    EXPECT_EQ("tool", toolJson["role"].get<std::string>());
    EXPECT_EQ("call_1", toolJson["tool_call_id"].get<std::string>());
    EXPECT_FALSE(toolJson.contains("tool_calls"));
}

TEST_F(OpenAIChatClientTest, CompleteChatStreaming_WithTools) {
    nlohmann::json chunk1 = {
        {"created", 1700000000},
        {"id", "chatcmpl-1"},
        {"IsDelta", true},
        {"Successful", true},
        {"HttpStatusCode", 200},
        {"choices",
         {{{"index", 0}, {"finish_reason", nullptr}, {"delta", {{"role", "assistant"}, {"content", "<tool_call>"}}}}}}};
    nlohmann::json chunk2 = {{"created", 1700000000},
                             {"id", "chatcmpl-1"},
                             {"IsDelta", true},
                             {"Successful", true},
                             {"HttpStatusCode", 200},
                             {"choices",
                              {{{"index", 0},
                                {"finish_reason", "tool_calls"},
                                {"delta",
                                 {{"content", "</tool_call>"},
                                  {"tool_calls",
                                   {{{"id", "call_1"},
                                     {"type", "function"},
                                     {"function", {{"name", "multiply"}, {"arguments", "{\"a\":1}"}}}}}}}}}}}};

    core_.OnCall("chat_completions",
                 [&](std::string_view, const std::string*, NativeCallbackFn callback, void* userData) -> std::string {
                     if (callback && userData) {
                         std::string s1 = chunk1.dump();
                         std::string s2 = chunk2.dump();
                         callback(s1.data(), static_cast<int32_t>(s1.size()), userData);
                         callback(s2.data(), static_cast<int32_t>(s2.size()), userData);
                     }
                     return "";
                 });
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    std::vector<ChatMessage> messages = {{"user", "test", {}}};

    std::vector<ToolDefinition> tools = {{"function", FunctionDefinition{"multiply", "Multiply numbers."}}};

    ChatSettings settings;
    settings.tool_choice = ToolChoiceKind::Required;

    std::vector<ChatCompletionCreateResponse> chunks;
    client.CompleteChatStreaming(messages, tools, settings,
                                 [&](const ChatCompletionCreateResponse& chunk) { chunks.push_back(chunk); });

    ASSERT_EQ(2u, chunks.size());
    EXPECT_EQ(FinishReason::ToolCalls, chunks[1].choices[0].finish_reason);
    ASSERT_TRUE(chunks[1].choices[0].delta.has_value());
    ASSERT_EQ(1u, chunks[1].choices[0].delta->tool_calls.size());
    EXPECT_EQ("multiply", chunks[1].choices[0].delta->tool_calls[0].function_call->name);

    // Verify tools were included in the request
    auto requestJson = nlohmann::json::parse(core_.GetLastDataArg("chat_completions"));
    auto openAiReq = nlohmann::json::parse(requestJson["Params"]["OpenAICreateRequest"].get<std::string>());
    ASSERT_TRUE(openAiReq.contains("tools"));
    EXPECT_EQ("required", openAiReq["tool_choice"].get<std::string>());
}

class OpenAIAudioClientTest : public ::testing::Test {
protected:
    MockCore core_;
    NullLogger logger_;

    ModelVariant MakeLoadedVariant(const std::string& name = "audio-model") {
        core_.OnCall("list_loaded_models", "[\"" + name + ":1\"]");
        return Factory::CreateModelVariant(&core_, Factory::MakeModelInfo(name, "alias"), &logger_);
    }
};

TEST_F(OpenAIAudioClientTest, TranscribeAudio) {
    core_.OnCall("audio_transcribe", "Hello world transcribed text");
    core_.OnCall("list_loaded_models", R"(["audio-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIAudioClient client(variant);
    auto response = client.TranscribeAudio("test.wav");

    EXPECT_EQ("Hello world transcribed text", response.text);
}

TEST_F(OpenAIAudioClientTest, TranscribeAudio_RequestFormat) {
    core_.OnCall("audio_transcribe", "text");
    core_.OnCall("list_loaded_models", R"(["audio-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIAudioClient client(variant);
    client.TranscribeAudio("audio.wav");

    auto requestJson = nlohmann::json::parse(core_.GetLastDataArg("audio_transcribe"));
    auto openAiReq = nlohmann::json::parse(requestJson["Params"]["OpenAICreateRequest"].get<std::string>());
    EXPECT_EQ("audio-model", openAiReq["Model"].get<std::string>());
    EXPECT_EQ("audio.wav", openAiReq["FileName"].get<std::string>());
}

TEST_F(OpenAIAudioClientTest, TranscribeAudioStreaming) {
    core_.OnCall("audio_transcribe",
                 [](std::string_view, const std::string*, NativeCallbackFn callback, void* userData) -> std::string {
                     if (callback && userData) {
                         std::string text1 = "Hello ";
                         std::string text2 = "world!";
                         callback(text1.data(), static_cast<int32_t>(text1.size()), userData);
                         callback(text2.data(), static_cast<int32_t>(text2.size()), userData);
                     }
                     return "";
                 });
    core_.OnCall("list_loaded_models", R"(["audio-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIAudioClient client(variant);

    std::vector<std::string> chunks;
    client.TranscribeAudioStreaming(
        "test.wav", [&](const AudioCreateTranscriptionResponse& chunk) { chunks.push_back(chunk.text); });

    ASSERT_EQ(2u, chunks.size());
    EXPECT_EQ("Hello ", chunks[0]);
    EXPECT_EQ("world!", chunks[1]);
}

TEST_F(OpenAIAudioClientTest, TranscribeAudioStreaming_PropagatesCallbackException) {
    core_.OnCall("audio_transcribe",
                 [](std::string_view, const std::string*, NativeCallbackFn callback, void* userData) -> std::string {
                     if (callback && userData) {
                         std::string text = "test";
                         callback(text.data(), static_cast<int32_t>(text.size()), userData);
                     }
                     return "";
                 });
    core_.OnCall("list_loaded_models", R"(["audio-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIAudioClient client(variant);

    EXPECT_THROW(
        client.TranscribeAudioStreaming(
            "test.wav", [](const AudioCreateTranscriptionResponse&) { throw std::runtime_error("streaming error"); }),
        std::runtime_error);
}

TEST_F(OpenAIAudioClientTest, Constructor_ThrowsIfNotLoaded) {
    core_.OnCall("list_loaded_models", R"([])");
    auto variant = Factory::CreateModelVariant(&core_, Factory::MakeModelInfo("unloaded-model", "alias"), &logger_);
    EXPECT_THROW(OpenAIAudioClient client(variant), Exception);
}

TEST_F(OpenAIAudioClientTest, GetModelId) {
    core_.OnCall("list_loaded_models", R"(["audio-model:1"])");
    auto variant = MakeLoadedVariant();
    OpenAIAudioClient client(variant);
    EXPECT_EQ("audio-model", client.GetModelId());
}

TEST_F(OpenAIAudioClientTest, TranscribeAudio_CoreError_Throws) {
    core_.OnCallThrow("audio_transcribe", "transcription failed");
    core_.OnCall("list_loaded_models", R"(["audio-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIAudioClient client(variant);

    EXPECT_THROW(client.TranscribeAudio("test.wav"), Exception);
}

TEST_F(OpenAIAudioClientTest, TranscribeAudioStreaming_CoreError_Throws) {
    core_.OnCallThrow("audio_transcribe", "streaming transcription failed");
    core_.OnCall("list_loaded_models", R"(["audio-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIAudioClient client(variant);

    EXPECT_THROW(client.TranscribeAudioStreaming("test.wav", [](const AudioCreateTranscriptionResponse&) {}),
                 Exception);
}

// =====================================================================
// Multi-turn conversation tests
// =====================================================================

TEST_F(OpenAIChatClientTest, CompleteChat_MultiTurn) {
    // First turn: user asks a question
    core_.OnCall("chat_completions", MakeChatResponseJson("42"));
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    std::vector<ChatMessage> messages = {{"user", "What is 7 * 6?", {}}};
    ChatSettings settings;
    auto response = client.CompleteChat(messages, settings);

    ASSERT_TRUE(response.successful);
    ASSERT_EQ(1u, response.choices.size());
    EXPECT_EQ("42", response.choices[0].message->content);

    // Second turn: add assistant response + user follow-up
    messages.push_back({"assistant", response.choices[0].message->content, {}});
    messages.push_back({"user", "Is that a real number?", {}});

    core_.OnCall("chat_completions", MakeChatResponseJson("Yes"));
    auto response2 = client.CompleteChat(messages, settings);

    ASSERT_TRUE(response2.successful);
    EXPECT_EQ("Yes", response2.choices[0].message->content);

    // Verify the second request contained all 3 messages
    auto requestJson = nlohmann::json::parse(core_.GetLastDataArg("chat_completions"));
    auto openAiReq = nlohmann::json::parse(requestJson["Params"]["OpenAICreateRequest"].get<std::string>());
    ASSERT_EQ(3u, openAiReq["messages"].size());
    EXPECT_EQ("user", openAiReq["messages"][0]["role"].get<std::string>());
    EXPECT_EQ("assistant", openAiReq["messages"][1]["role"].get<std::string>());
    EXPECT_EQ("user", openAiReq["messages"][2]["role"].get<std::string>());
}

TEST_F(OpenAIChatClientTest, CompleteChat_CoreError_Throws) {
    core_.OnCallThrow("chat_completions", "inference failed");
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    std::vector<ChatMessage> messages = {{"user", "Hello", {}}};
    ChatSettings settings;

    EXPECT_THROW(client.CompleteChat(messages, settings), Exception);
}

TEST_F(OpenAIChatClientTest, CompleteChatStreaming_CoreError_Throws) {
    core_.OnCallThrow("chat_completions", "streaming failed");
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    std::vector<ChatMessage> messages = {{"user", "Hello", {}}};
    ChatSettings settings;

    EXPECT_THROW(client.CompleteChatStreaming(messages, settings, [](const ChatCompletionCreateResponse&) {}),
                 Exception);
}

// =====================================================================
// Full tool-call round-trip
// =====================================================================

TEST_F(OpenAIChatClientTest, CompleteChat_ToolCallRoundTrip) {
    // Step 1: model returns a tool call
    nlohmann::json toolCallResp = {
        {"created", 1700000000},
        {"id", "chatcmpl-tool"},
        {"IsDelta", false},
        {"Successful", true},
        {"HttpStatusCode", 200},
        {"choices",
         {{{"index", 0},
           {"finish_reason", "tool_calls"},
           {"message",
            {{"role", "assistant"},
             {"content", "<tool_call>[{\"name\": \"multiply_numbers\", \"parameters\": {\"first\": 7, \"second\": "
                         "6}}]</tool_call>"},
             {"tool_calls",
              {{{"id", "call_1"},
                {"type", "function"},
                {"function", {{"name", "multiply_numbers"}, {"arguments", "{\"first\": 7, \"second\": 6}"}}}}}}}}}}}};

    core_.OnCall("chat_completions", toolCallResp.dump());
    core_.OnCall("list_loaded_models", R"(["chat-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIChatClient client(variant);

    std::vector<ChatMessage> messages = {{"system", "You are a helpful AI assistant.", {}},
                                         {"user", "What is 7 multiplied by 6?", {}}};

    std::vector<ToolDefinition> tools = {
        {"function",
         FunctionDefinition{"multiply_numbers", "A tool for multiplying two numbers.",
                            PropertyDefinition{"object", std::nullopt,
                                               std::unordered_map<std::string, PropertyDefinition>{
                                                   {"first", PropertyDefinition{"integer", "The first number"}},
                                                   {"second", PropertyDefinition{"integer", "The second number"}}},
                                               std::vector<std::string>{"first", "second"}}}}};

    ChatSettings settings;
    settings.tool_choice = ToolChoiceKind::Required;

    auto response = client.CompleteChat(messages, tools, settings);

    ASSERT_EQ(1u, response.choices.size());
    EXPECT_EQ(FinishReason::ToolCalls, response.choices[0].finish_reason);
    ASSERT_EQ(1u, response.choices[0].message->tool_calls.size());
    EXPECT_EQ("multiply_numbers", response.choices[0].message->tool_calls[0].function_call->name);

    // Step 2: send tool response back, model continues with the answer
    messages.push_back({"assistant", response.choices[0].message->content, {}});

    ChatMessage toolMsg;
    toolMsg.role = "tool";
    toolMsg.content = "7 x 6 = 42.";
    toolMsg.tool_call_id = "call_1";
    messages.push_back(std::move(toolMsg));

    messages.push_back({"system", "Respond only with the answer generated by the tool.", {}});

    core_.OnCall("chat_completions", MakeChatResponseJson("42"));
    settings.tool_choice = ToolChoiceKind::Auto;

    auto response2 = client.CompleteChat(messages, tools, settings);

    ASSERT_TRUE(response2.successful);
    EXPECT_EQ("42", response2.choices[0].message->content);

    // Verify the second request contained tool response message
    auto requestJson = nlohmann::json::parse(core_.GetLastDataArg("chat_completions"));
    auto openAiReq = nlohmann::json::parse(requestJson["Params"]["OpenAICreateRequest"].get<std::string>());

    // 5 messages: system, user, assistant (tool_call), tool, system (continue)
    ASSERT_EQ(5u, openAiReq["messages"].size());
    EXPECT_EQ("tool", openAiReq["messages"][3]["role"].get<std::string>());
    EXPECT_EQ("call_1", openAiReq["messages"][3]["tool_call_id"].get<std::string>());
    EXPECT_EQ("auto", openAiReq["tool_choice"].get<std::string>());
}

// =====================================================================
// OpenAIEmbeddingClient tests
// =====================================================================

class OpenAIEmbeddingClientTest : public ::testing::Test {
protected:
    MockCore core_;
    NullLogger logger_;

    static std::string MakeEmbeddingResponseJson(const std::vector<std::vector<float>>& vectors,
                                                  const std::string& modelName = "embedding-model") {
        nlohmann::json data = nlohmann::json::array();
        for (size_t i = 0; i < vectors.size(); ++i) {
            data.push_back({{"index", static_cast<int>(i)}, {"object", "embedding"}, {"embedding", vectors[i]}});
        }
        nlohmann::json resp = {{"model", modelName},
                               {"object", "list"},
                               {"data", std::move(data)},
                               {"usage", {{"prompt_tokens", 5}, {"total_tokens", 5}}}};
        return resp.dump();
    }

    ModelVariant MakeLoadedVariant(const std::string& name = "embedding-model") {
        core_.OnCall("list_loaded_models", "[\"" + name + ":1\"]");
        return Factory::CreateModelVariant(&core_, Factory::MakeModelInfo(name, "alias"), &logger_);
    }
};

TEST_F(OpenAIEmbeddingClientTest, GenerateEmbedding_BasicResponse) {
    core_.OnCall("embeddings", MakeEmbeddingResponseJson({{0.1f, 0.2f, 0.3f, 0.4f}}));
    core_.OnCall("list_loaded_models", R"(["embedding-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIEmbeddingClient client(variant);
    auto response = client.GenerateEmbedding("hello world");

    EXPECT_EQ("embedding-model", response.model);
    EXPECT_EQ("list", response.object);
    ASSERT_EQ(1u, response.data.size());
    EXPECT_EQ(0, response.data[0].index);
    ASSERT_EQ(4u, response.data[0].embedding.size());
    EXPECT_NEAR(0.1f, response.data[0].embedding[0], 1e-5f);
    EXPECT_NEAR(0.4f, response.data[0].embedding[3], 1e-5f);
    ASSERT_TRUE(response.usage.has_value());
    EXPECT_EQ(5, *response.usage->prompt_tokens);
}

TEST_F(OpenAIEmbeddingClientTest, GenerateEmbedding_RequestFormat) {
    core_.OnCall("embeddings", MakeEmbeddingResponseJson({{0.0f}}));
    core_.OnCall("list_loaded_models", R"(["embedding-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIEmbeddingClient client(variant);
    client.GenerateEmbedding("hello world");

    auto requestJson = nlohmann::json::parse(core_.GetLastDataArg("embeddings"));
    auto openAiReq = nlohmann::json::parse(requestJson["Params"]["OpenAICreateRequest"].get<std::string>());
    EXPECT_EQ("embedding-model", openAiReq["model"].get<std::string>());
    EXPECT_EQ("hello world", openAiReq["input"].get<std::string>());
}

TEST_F(OpenAIEmbeddingClientTest, GenerateEmbeddings_BasicResponse) {
    core_.OnCall("embeddings", MakeEmbeddingResponseJson({{0.1f, 0.2f}, {0.3f, 0.4f}, {0.5f, 0.6f}}));
    core_.OnCall("list_loaded_models", R"(["embedding-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIEmbeddingClient client(variant);

    std::vector<std::string> inputs = {"first", "second", "third"};
    auto response = client.GenerateEmbeddings(inputs);

    ASSERT_EQ(3u, response.data.size());
    EXPECT_EQ(0, response.data[0].index);
    EXPECT_EQ(1, response.data[1].index);
    EXPECT_EQ(2, response.data[2].index);
    EXPECT_NEAR(0.5f, response.data[2].embedding[0], 1e-5f);
}

TEST_F(OpenAIEmbeddingClientTest, GenerateEmbeddings_RequestFormat) {
    core_.OnCall("embeddings", MakeEmbeddingResponseJson({{0.0f}, {0.0f}}));
    core_.OnCall("list_loaded_models", R"(["embedding-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIEmbeddingClient client(variant);

    std::vector<std::string> inputs = {"a", "b"};
    client.GenerateEmbeddings(inputs);

    auto requestJson = nlohmann::json::parse(core_.GetLastDataArg("embeddings"));
    auto openAiReq = nlohmann::json::parse(requestJson["Params"]["OpenAICreateRequest"].get<std::string>());
    EXPECT_EQ("embedding-model", openAiReq["model"].get<std::string>());
    ASSERT_TRUE(openAiReq["input"].is_array());
    ASSERT_EQ(2u, openAiReq["input"].size());
    EXPECT_EQ("a", openAiReq["input"][0].get<std::string>());
    EXPECT_EQ("b", openAiReq["input"][1].get<std::string>());
}

TEST_F(OpenAIEmbeddingClientTest, GenerateEmbedding_EmptyInput_Throws) {
    core_.OnCall("list_loaded_models", R"(["embedding-model:1"])");
    auto variant = MakeLoadedVariant();
    OpenAIEmbeddingClient client(variant);

    EXPECT_THROW(client.GenerateEmbedding(""), Exception);
}

TEST_F(OpenAIEmbeddingClientTest, GenerateEmbedding_WhitespaceOnlyInput_Throws) {
    core_.OnCall("list_loaded_models", R"(["embedding-model:1"])");
    auto variant = MakeLoadedVariant();
    OpenAIEmbeddingClient client(variant);

    EXPECT_THROW(client.GenerateEmbedding("   \t\n  "), Exception);
}

TEST_F(OpenAIEmbeddingClientTest, GenerateEmbeddings_EmptyList_Throws) {
    core_.OnCall("list_loaded_models", R"(["embedding-model:1"])");
    auto variant = MakeLoadedVariant();
    OpenAIEmbeddingClient client(variant);

    std::vector<std::string> empty;
    EXPECT_THROW(client.GenerateEmbeddings(empty), Exception);
}

TEST_F(OpenAIEmbeddingClientTest, GenerateEmbeddings_ListWithEmptyString_Throws) {
    core_.OnCall("list_loaded_models", R"(["embedding-model:1"])");
    auto variant = MakeLoadedVariant();
    OpenAIEmbeddingClient client(variant);

    std::vector<std::string> inputs = {"valid", "", "also valid"};
    EXPECT_THROW(client.GenerateEmbeddings(inputs), Exception);
}

TEST_F(OpenAIEmbeddingClientTest, GenerateEmbeddings_ListWithWhitespaceOnlyString_Throws) {
    core_.OnCall("list_loaded_models", R"(["embedding-model:1"])");
    auto variant = MakeLoadedVariant();
    OpenAIEmbeddingClient client(variant);

    std::vector<std::string> inputs = {"valid", "   ", "also valid"};
    EXPECT_THROW(client.GenerateEmbeddings(inputs), Exception);
}

TEST_F(OpenAIEmbeddingClientTest, Constructor_ThrowsIfNotLoaded) {
    core_.OnCall("list_loaded_models", R"([])");
    auto variant = Factory::CreateModelVariant(&core_, Factory::MakeModelInfo("unloaded-model", "alias"), &logger_);
    EXPECT_THROW(OpenAIEmbeddingClient client(variant), Exception);
}

TEST_F(OpenAIEmbeddingClientTest, GetModelId) {
    core_.OnCall("list_loaded_models", R"(["embedding-model:1"])");
    auto variant = MakeLoadedVariant();
    OpenAIEmbeddingClient client(variant);
    EXPECT_EQ("embedding-model", client.GetModelId());
}

TEST_F(OpenAIEmbeddingClientTest, GenerateEmbedding_CoreError_Throws) {
    core_.OnCallThrow("embeddings", "embedding generation failed");
    core_.OnCall("list_loaded_models", R"(["embedding-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIEmbeddingClient client(variant);

    EXPECT_THROW(client.GenerateEmbedding("test"), Exception);
}

TEST_F(OpenAIEmbeddingClientTest, GenerateEmbeddings_CoreError_Throws) {
    core_.OnCallThrow("embeddings", "batch embedding generation failed");
    core_.OnCall("list_loaded_models", R"(["embedding-model:1"])");

    auto variant = MakeLoadedVariant();
    OpenAIEmbeddingClient client(variant);

    std::vector<std::string> inputs = {"a", "b"};
    EXPECT_THROW(client.GenerateEmbeddings(inputs), Exception);
}
