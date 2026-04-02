// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <gtest/gtest.h>

#include "mock_core.h"
#include "mock_object_factory.h"
#include "parser.h"
#include "foundry_local_exception.h"
#include "core_interop_request.h"

#include <nlohmann/json.hpp>

using namespace foundry_local;
using namespace foundry_local::Testing;

class ParserTest : public ::testing::Test {
protected:
    static nlohmann::json MinimalModelJson() {
        return nlohmann::json{{"id", "model-1:1"},   {"name", "model-1"},      {"version", 1},
                              {"alias", "my-model"}, {"providerType", "onnx"}, {"uri", "https://example.com/model"},
                              {"modelType", "text"}, {"cached", false},        {"createdAt", 1700000000}};
    }
};

TEST_F(ParserTest, ParseDeviceType_CPU) {
    EXPECT_EQ(DeviceType::CPU, ParsingUtils::parse_device_type("CPU"));
}

TEST_F(ParserTest, ParseDeviceType_GPU) {
    EXPECT_EQ(DeviceType::GPU, ParsingUtils::parse_device_type("GPU"));
}

TEST_F(ParserTest, ParseDeviceType_NPU) {
    EXPECT_EQ(DeviceType::NPU, ParsingUtils::parse_device_type("NPU"));
}

TEST_F(ParserTest, ParseDeviceType_Unknown) {
    EXPECT_EQ(DeviceType::Invalid, ParsingUtils::parse_device_type("FPGA"));
}

TEST_F(ParserTest, ParseFinishReason_Stop) {
    EXPECT_EQ(FinishReason::Stop, ParsingUtils::parse_finish_reason("stop"));
}

TEST_F(ParserTest, ParseFinishReason_Length) {
    EXPECT_EQ(FinishReason::Length, ParsingUtils::parse_finish_reason("length"));
}

TEST_F(ParserTest, ParseFinishReason_ToolCalls) {
    EXPECT_EQ(FinishReason::ToolCalls, ParsingUtils::parse_finish_reason("tool_calls"));
}

TEST_F(ParserTest, ParseFinishReason_ContentFilter) {
    EXPECT_EQ(FinishReason::ContentFilter, ParsingUtils::parse_finish_reason("content_filter"));
}

TEST_F(ParserTest, ParseFinishReason_None) {
    EXPECT_EQ(FinishReason::None, ParsingUtils::parse_finish_reason("unknown_value"));
}

TEST_F(ParserTest, GetStringOrEmpty_Present) {
    nlohmann::json j = {{"key", "value"}};
    EXPECT_EQ("value", ParsingUtils::get_string_or_empty(j, "key"));
}

TEST_F(ParserTest, GetStringOrEmpty_Missing) {
    nlohmann::json j = {{"other", "value"}};
    EXPECT_EQ("", ParsingUtils::get_string_or_empty(j, "key"));
}

TEST_F(ParserTest, GetStringOrEmpty_NonString) {
    nlohmann::json j = {{"key", 42}};
    EXPECT_EQ("", ParsingUtils::get_string_or_empty(j, "key"));
}

TEST_F(ParserTest, GetOptString_Present) {
    nlohmann::json j = {{"key", "hello"}};
    auto result = ParsingUtils::get_opt_string(j, "key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ("hello", *result);
}

TEST_F(ParserTest, GetOptString_Null) {
    nlohmann::json j = {{"key", nullptr}};
    EXPECT_FALSE(ParsingUtils::get_opt_string(j, "key").has_value());
}

TEST_F(ParserTest, GetOptString_Missing) {
    nlohmann::json j = {{"other", "v"}};
    EXPECT_FALSE(ParsingUtils::get_opt_string(j, "key").has_value());
}

TEST_F(ParserTest, GetOptInt_Present) {
    nlohmann::json j = {{"key", 42}};
    auto result = ParsingUtils::get_opt_int(j, "key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(42, *result);
}

TEST_F(ParserTest, GetOptInt_Missing) {
    nlohmann::json j = {};
    EXPECT_FALSE(ParsingUtils::get_opt_int(j, "key").has_value());
}

TEST_F(ParserTest, GetOptBool_Present) {
    nlohmann::json j = {{"key", true}};
    auto result = ParsingUtils::get_opt_bool(j, "key");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}

TEST_F(ParserTest, GetOptBool_Missing) {
    nlohmann::json j = {};
    EXPECT_FALSE(ParsingUtils::get_opt_bool(j, "key").has_value());
}

TEST_F(ParserTest, ParseRuntime) {
    nlohmann::json j = {{"deviceType", "GPU"}, {"executionProvider", "DML"}};
    Runtime r = j.get<Runtime>();
    EXPECT_EQ(DeviceType::GPU, r.device_type);
    EXPECT_EQ("DML", r.execution_provider);
}

TEST_F(ParserTest, ParsePromptTemplate) {
    nlohmann::json j = {{"system", "sys"}, {"user", "usr"}, {"assistant", "asst"}, {"prompt", "p"}};
    PromptTemplate pt = j.get<PromptTemplate>();
    EXPECT_EQ("sys", pt.system);
    EXPECT_EQ("usr", pt.user);
    EXPECT_EQ("asst", pt.assistant);
    EXPECT_EQ("p", pt.prompt);
}

TEST_F(ParserTest, ParsePromptTemplate_MissingFields) {
    nlohmann::json j = {{"system", "sys"}};
    PromptTemplate pt = j.get<PromptTemplate>();
    EXPECT_EQ("sys", pt.system);
    EXPECT_EQ("", pt.user);
    EXPECT_EQ("", pt.assistant);
    EXPECT_EQ("", pt.prompt);
}

TEST_F(ParserTest, ParseModelInfo_Minimal) {
    auto j = MinimalModelJson();
    ModelInfo info = j.get<ModelInfo>();
    EXPECT_EQ("model-1:1", info.id);
    EXPECT_EQ("model-1", info.name);
    EXPECT_EQ(1u, info.version);
    EXPECT_EQ("my-model", info.alias);
    EXPECT_EQ("onnx", info.provider_type);
    EXPECT_EQ("https://example.com/model", info.uri);
    EXPECT_EQ("text", info.model_type);
    EXPECT_FALSE(info.cached);
    EXPECT_EQ(1700000000, info.created_at_unix);
    EXPECT_FALSE(info.display_name.has_value());
    EXPECT_FALSE(info.publisher.has_value());
    EXPECT_FALSE(info.runtime.has_value());
    EXPECT_FALSE(info.prompt_template.has_value());
    EXPECT_FALSE(info.model_settings.has_value());
}

TEST_F(ParserTest, ParseModelInfo_WithOptionals) {
    auto j = MinimalModelJson();
    j["displayName"] = "My Model";
    j["publisher"] = "TestPublisher";
    j["license"] = "MIT";
    j["fileSizeMb"] = 512;
    j["supportsToolCalling"] = true;
    j["maxOutputTokens"] = 4096;
    j["runtime"] = {{"deviceType", "CPU"}, {"executionProvider", "ORT"}};

    ModelInfo info = j.get<ModelInfo>();
    ASSERT_TRUE(info.display_name.has_value());
    EXPECT_EQ("My Model", *info.display_name);
    ASSERT_TRUE(info.publisher.has_value());
    EXPECT_EQ("TestPublisher", *info.publisher);
    ASSERT_TRUE(info.license.has_value());
    EXPECT_EQ("MIT", *info.license);
    ASSERT_TRUE(info.file_size_mb.has_value());
    EXPECT_EQ(512u, *info.file_size_mb);
    ASSERT_TRUE(info.supports_tool_calling.has_value());
    EXPECT_TRUE(*info.supports_tool_calling);
    ASSERT_TRUE(info.max_output_tokens.has_value());
    EXPECT_EQ(4096, *info.max_output_tokens);
    ASSERT_TRUE(info.runtime.has_value());
    EXPECT_EQ(DeviceType::CPU, info.runtime->device_type);
    EXPECT_EQ("ORT", info.runtime->execution_provider);
}

TEST_F(ParserTest, ParseModelSettings) {
    nlohmann::json j = {{"parameters", {{{"name", "p1"}, {"value", "v1"}}, {{"name", "p2"}}}}};
    ModelSettings ms = j.get<ModelSettings>();
    ASSERT_EQ(2u, ms.parameters.size());
    EXPECT_EQ("p1", ms.parameters[0].name);
    ASSERT_TRUE(ms.parameters[0].value.has_value());
    EXPECT_EQ("v1", *ms.parameters[0].value);
    EXPECT_EQ("p2", ms.parameters[1].name);
    EXPECT_FALSE(ms.parameters[1].value.has_value());
}

TEST_F(ParserTest, ParseChatMessage) {
    nlohmann::json j = {{"role", "user"}, {"content", "hello"}};
    ChatMessage msg = j.get<ChatMessage>();
    EXPECT_EQ("user", msg.role);
    EXPECT_EQ("hello", msg.content);
    EXPECT_TRUE(msg.tool_calls.empty());
    EXPECT_FALSE(msg.tool_call_id.has_value());
}

TEST_F(ParserTest, ParseChatMessage_WithToolCalls) {
    nlohmann::json j = {{"role", "assistant"},
                        {"content", "I'll call a tool."},
                        {"tool_calls",
                         {{{"id", "call_abc123"},
                           {"type", "function"},
                           {"function", {{"name", "get_weather"}, {"arguments", "{\"city\": \"Seattle\"}"}}}}}}};
    ChatMessage msg = j.get<ChatMessage>();
    EXPECT_EQ("assistant", msg.role);
    ASSERT_EQ(1u, msg.tool_calls.size());
    EXPECT_EQ("call_abc123", msg.tool_calls[0].id);
    EXPECT_EQ("function", msg.tool_calls[0].type);
    ASSERT_TRUE(msg.tool_calls[0].function_call.has_value());
    EXPECT_EQ("get_weather", msg.tool_calls[0].function_call->name);
    EXPECT_EQ("{\"city\": \"Seattle\"}", msg.tool_calls[0].function_call->arguments);
}

TEST_F(ParserTest, ParseChatMessage_WithToolCallId) {
    nlohmann::json j = {{"role", "tool"}, {"content", "72 degrees and sunny"}, {"tool_call_id", "call_abc123"}};
    ChatMessage msg = j.get<ChatMessage>();
    EXPECT_EQ("tool", msg.role);
    EXPECT_EQ("72 degrees and sunny", msg.content);
    ASSERT_TRUE(msg.tool_call_id.has_value());
    EXPECT_EQ("call_abc123", *msg.tool_call_id);
}

TEST_F(ParserTest, ParseFunctionCall) {
    nlohmann::json j = {{"name", "multiply"}, {"arguments", "{\"a\": 1, \"b\": 2}"}};
    FunctionCall fc = j.get<FunctionCall>();
    EXPECT_EQ("multiply", fc.name);
    EXPECT_EQ("{\"a\": 1, \"b\": 2}", fc.arguments);
}

TEST_F(ParserTest, ParseFunctionCall_ObjectArguments) {
    nlohmann::json j = {{"name", "add"}, {"arguments", {{"x", 10}}}};
    FunctionCall fc = j.get<FunctionCall>();
    EXPECT_EQ("add", fc.name);
    EXPECT_EQ("{\"x\":10}", fc.arguments);
}

TEST_F(ParserTest, ParseToolCall) {
    nlohmann::json j = {{"id", "call_1"},
                        {"type", "function"},
                        {"function", {{"name", "search"}, {"arguments", "{\"query\": \"test\"}"}}}};
    ToolCall tc = j.get<ToolCall>();
    EXPECT_EQ("call_1", tc.id);
    EXPECT_EQ("function", tc.type);
    ASSERT_TRUE(tc.function_call.has_value());
    EXPECT_EQ("search", tc.function_call->name);
}

TEST_F(ParserTest, SerializeToolDefinition) {
    ToolDefinition tool;
    tool.type = "function";
    tool.function.name = "get_weather";
    tool.function.description = "Get the current weather";
    tool.function.parameters = PropertyDefinition{"object", std::nullopt,
                                                  std::unordered_map<std::string, PropertyDefinition>{
                                                      {"location", PropertyDefinition{"string", "The city name"}}},
                                                  std::vector<std::string>{"location"}};

    nlohmann::json j;
    to_json(j, tool);

    EXPECT_EQ("function", j["type"].get<std::string>());
    EXPECT_EQ("get_weather", j["function"]["name"].get<std::string>());
    EXPECT_EQ("Get the current weather", j["function"]["description"].get<std::string>());
    EXPECT_EQ("object", j["function"]["parameters"]["type"].get<std::string>());
    ASSERT_TRUE(j["function"]["parameters"]["properties"].contains("location"));
    EXPECT_EQ("string", j["function"]["parameters"]["properties"]["location"]["type"].get<std::string>());
    ASSERT_EQ(1u, j["function"]["parameters"]["required"].size());
    EXPECT_EQ("location", j["function"]["parameters"]["required"][0].get<std::string>());
}

TEST_F(ParserTest, SerializeToolDefinition_MinimalFunction) {
    ToolDefinition tool;
    tool.function.name = "noop";

    nlohmann::json j;
    to_json(j, tool);

    EXPECT_EQ("function", j["type"].get<std::string>());
    EXPECT_EQ("noop", j["function"]["name"].get<std::string>());
    EXPECT_FALSE(j["function"].contains("description"));
    EXPECT_FALSE(j["function"].contains("parameters"));
}

TEST_F(ParserTest, ToolChoiceToString) {
    EXPECT_EQ("auto", ParsingUtils::tool_choice_to_string(ToolChoiceKind::Auto));
    EXPECT_EQ("none", ParsingUtils::tool_choice_to_string(ToolChoiceKind::None));
    EXPECT_EQ("required", ParsingUtils::tool_choice_to_string(ToolChoiceKind::Required));
}

TEST_F(ParserTest, ParseChatChoice_NonStreaming) {
    nlohmann::json j = {
        {"index", 0}, {"finish_reason", "stop"}, {"message", {{"role", "assistant"}, {"content", "Hi there!"}}}};
    ChatChoice c = j.get<ChatChoice>();
    EXPECT_EQ(0, c.index);
    EXPECT_EQ(FinishReason::Stop, c.finish_reason);
    ASSERT_TRUE(c.message.has_value());
    EXPECT_EQ("assistant", c.message->role);
    EXPECT_EQ("Hi there!", c.message->content);
    EXPECT_FALSE(c.delta.has_value());
}

TEST_F(ParserTest, ParseChatChoice_Streaming) {
    nlohmann::json j = {
        {"index", 0}, {"finish_reason", nullptr}, {"delta", {{"role", "assistant"}, {"content", "Hi"}}}};
    ChatChoice c = j.get<ChatChoice>();
    EXPECT_EQ(FinishReason::None, c.finish_reason);
    EXPECT_FALSE(c.message.has_value());
    ASSERT_TRUE(c.delta.has_value());
    EXPECT_EQ("Hi", c.delta->content);
}

TEST_F(ParserTest, ParseChatCompletionCreateResponse) {
    nlohmann::json j = {
        {"created", 1700000000},
        {"id", "chatcmpl-123"},
        {"IsDelta", false},
        {"Successful", true},
        {"HttpStatusCode", 200},
        {"choices",
         {{{"index", 0}, {"finish_reason", "stop"}, {"message", {{"role", "assistant"}, {"content", "Hello!"}}}}}}};
    ChatCompletionCreateResponse r = j.get<ChatCompletionCreateResponse>();
    EXPECT_EQ(1700000000, r.created);
    EXPECT_EQ("chatcmpl-123", r.id);
    EXPECT_FALSE(r.is_delta);
    EXPECT_TRUE(r.successful);
    EXPECT_EQ(200, r.http_status_code);
    ASSERT_EQ(1u, r.choices.size());
    EXPECT_EQ("Hello!", r.choices[0].message->content);
}

TEST(ChatCompletionCreateResponseTest, GetObject_NonDelta) {
    ChatCompletionCreateResponse r;
    r.is_delta = false;
    EXPECT_STREQ("chat.completion", r.GetObject());
}

TEST(ChatCompletionCreateResponseTest, GetObject_Delta) {
    ChatCompletionCreateResponse r;
    r.is_delta = true;
    EXPECT_STREQ("chat.completion.chunk", r.GetObject());
}

TEST(ChatCompletionCreateResponseTest, GetCreatedAtIso_Zero) {
    ChatCompletionCreateResponse r;
    r.created = 0;
    EXPECT_EQ("", r.GetCreatedAtIso());
}

TEST(ChatCompletionCreateResponseTest, GetCreatedAtIso_ValidTimestamp) {
    ChatCompletionCreateResponse r;
    r.created = 1700000000; // 2023-11-14T22:13:20Z
    std::string iso = r.GetCreatedAtIso();
    EXPECT_FALSE(iso.empty());
    EXPECT_EQ('Z', iso.back());
    EXPECT_NE(std::string::npos, iso.find("2023"));
}

// =============================================================================
// CoreInteropRequest tests
// =============================================================================

TEST(CoreInteropRequestTest, Command) {
    CoreInteropRequest req("test_command");
    EXPECT_EQ("test_command", req.Command());
}

TEST(CoreInteropRequestTest, ToJson_NoParams) {
    CoreInteropRequest req("cmd");
    std::string json = req.ToJson();
    auto parsed = nlohmann::json::parse(json);
    EXPECT_FALSE(parsed.contains("Params"));
}

TEST(CoreInteropRequestTest, ToJson_WithParams) {
    CoreInteropRequest req("cmd");
    req.AddParam("key1", "value1");
    req.AddParam("key2", "value2");
    std::string json = req.ToJson();
    auto parsed = nlohmann::json::parse(json);
    ASSERT_TRUE(parsed.contains("Params"));
    EXPECT_EQ("value1", parsed["Params"]["key1"].get<std::string>());
    EXPECT_EQ("value2", parsed["Params"]["key2"].get<std::string>());
}

TEST(CoreInteropRequestTest, AddParam_Chaining) {
    CoreInteropRequest req("cmd");
    auto& ref = req.AddParam("a", "1").AddParam("b", "2");
    EXPECT_EQ(&req, &ref);
}

// =============================================================================
// Exception tests
// =============================================================================

TEST(ExceptionTest, MessageOnly) {
    Exception ex("test error");
    EXPECT_STREQ("test error", ex.what());
}

TEST(ExceptionTest, MessageAndLogger) {
    NullLogger logger;
    Exception ex("logged error", logger);
    EXPECT_STREQ("logged error", ex.what());
}