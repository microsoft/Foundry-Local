#include <gtest/gtest.h>

#include "mock_core.h"
#include "mock_object_factory.h"
#include "parser.h"
#include "foundry_local_exception.h"
#include "core_interop_request.h"

#include <nlohmann/json.hpp>

using namespace FoundryLocal;
using namespace FoundryLocal::Testing;

class ParserTest : public ::testing::Test {
protected:
    static nlohmann::json MinimalModelJson() {
        return nlohmann::json{{"id", "model-1"},     {"name", "model-1"},      {"version", 1},
                              {"alias", "my-model"}, {"providerType", "onnx"}, {"uri", "https://example.com/model"},
                              {"modelType", "text"}, {"cached", false},        {"createdAt", 1700000000}};
    }
};

TEST_F(ParserTest, ParseDeviceType_CPU) {
    EXPECT_EQ(DeviceType::CPU, parse_device_type("CPU"));
}

TEST_F(ParserTest, ParseDeviceType_GPU) {
    EXPECT_EQ(DeviceType::GPU, parse_device_type("GPU"));
}

TEST_F(ParserTest, ParseDeviceType_NPU) {
    EXPECT_EQ(DeviceType::NPU, parse_device_type("NPU"));
}

TEST_F(ParserTest, ParseDeviceType_Unknown) {
    EXPECT_EQ(DeviceType::Invalid, parse_device_type("FPGA"));
}

TEST_F(ParserTest, ParseFinishReason_Stop) {
    EXPECT_EQ(FinishReason::Stop, parse_finish_reason("stop"));
}

TEST_F(ParserTest, ParseFinishReason_Length) {
    EXPECT_EQ(FinishReason::Length, parse_finish_reason("length"));
}

TEST_F(ParserTest, ParseFinishReason_ToolCalls) {
    EXPECT_EQ(FinishReason::ToolCalls, parse_finish_reason("tool_calls"));
}

TEST_F(ParserTest, ParseFinishReason_ContentFilter) {
    EXPECT_EQ(FinishReason::ContentFilter, parse_finish_reason("content_filter"));
}

TEST_F(ParserTest, ParseFinishReason_None) {
    EXPECT_EQ(FinishReason::None, parse_finish_reason("unknown_value"));
}

TEST_F(ParserTest, GetStringOrEmpty_Present) {
    nlohmann::json j = {{"key", "value"}};
    EXPECT_EQ("value", get_string_or_empty(j, "key"));
}

TEST_F(ParserTest, GetStringOrEmpty_Missing) {
    nlohmann::json j = {{"other", "value"}};
    EXPECT_EQ("", get_string_or_empty(j, "key"));
}

TEST_F(ParserTest, GetStringOrEmpty_NonString) {
    nlohmann::json j = {{"key", 42}};
    EXPECT_EQ("", get_string_or_empty(j, "key"));
}

TEST_F(ParserTest, GetOptString_Present) {
    nlohmann::json j = {{"key", "hello"}};
    auto result = get_opt_string(j, "key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ("hello", *result);
}

TEST_F(ParserTest, GetOptString_Null) {
    nlohmann::json j = {{"key", nullptr}};
    EXPECT_FALSE(get_opt_string(j, "key").has_value());
}

TEST_F(ParserTest, GetOptString_Missing) {
    nlohmann::json j = {{"other", "v"}};
    EXPECT_FALSE(get_opt_string(j, "key").has_value());
}

TEST_F(ParserTest, GetOptInt_Present) {
    nlohmann::json j = {{"key", 42}};
    auto result = get_opt_int(j, "key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(42, *result);
}

TEST_F(ParserTest, GetOptInt_Missing) {
    nlohmann::json j = {};
    EXPECT_FALSE(get_opt_int(j, "key").has_value());
}

TEST_F(ParserTest, GetOptBool_Present) {
    nlohmann::json j = {{"key", true}};
    auto result = get_opt_bool(j, "key");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}

TEST_F(ParserTest, GetOptBool_Missing) {
    nlohmann::json j = {};
    EXPECT_FALSE(get_opt_bool(j, "key").has_value());
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
    EXPECT_EQ("model-1", info.id);
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
    nlohmann::json j = {
        {"role", "assistant"},
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
    nlohmann::json j = {
        {"role", "tool"},
        {"content", "72 degrees and sunny"},
        {"tool_call_id", "call_abc123"}};
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
    nlohmann::json j = {
        {"id", "call_1"},
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
    tool.function.parameters = PropertyDefinition{
        "object",
        std::nullopt,
        std::unordered_map<std::string, PropertyDefinition>{
            {"location", PropertyDefinition{"string", "The city name"}}
        },
        std::vector<std::string>{"location"}
    };

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
    EXPECT_EQ("auto", tool_choice_to_string(ToolChoiceKind::Auto));
    EXPECT_EQ("none", tool_choice_to_string(ToolChoiceKind::None));
    EXPECT_EQ("required", tool_choice_to_string(ToolChoiceKind::Required));
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
// FoundryLocalException tests
// =============================================================================

TEST(FoundryLocalExceptionTest, MessageOnly) {
    FoundryLocalException ex("test error");
    EXPECT_STREQ("test error", ex.what());
}

TEST(FoundryLocalExceptionTest, MessageAndLogger) {
    NullLogger logger;
    FoundryLocalException ex("logged error", logger);
    EXPECT_STREQ("logged error", ex.what());
}

// =============================================================================
// File-based parser tests (read JSON from testdata/)
// =============================================================================

class FileBasedParserTest : public ::testing::Test {
protected:
    static std::string TestDataPath(const std::string& filename) { return "testdata/" + filename; }

    static nlohmann::json LoadJsonArray(const std::string& filename) {
        std::string raw = Testing::ReadFile(TestDataPath(filename));
        return nlohmann::json::parse(raw);
    }
};

TEST_F(FileBasedParserTest, AllFields_RequiredFields) {
    auto arr = LoadJsonArray("model_all_fields.json");
    ModelInfo info = arr.at(0).get<ModelInfo>();
    EXPECT_EQ("model-all-fields", info.id);
    EXPECT_EQ("model-all-fields", info.name);
    EXPECT_EQ(3u, info.version);
    EXPECT_EQ("full-model", info.alias);
    EXPECT_EQ("onnx", info.provider_type);
    EXPECT_EQ("https://example.com/full-model", info.uri);
    EXPECT_EQ("text", info.model_type);
    EXPECT_TRUE(info.cached);
    EXPECT_EQ(1710000000, info.created_at_unix);
}

TEST_F(FileBasedParserTest, AllFields_OptionalStrings) {
    auto arr = LoadJsonArray("model_all_fields.json");
    ModelInfo info = arr.at(0).get<ModelInfo>();

    ASSERT_TRUE(info.display_name.has_value());
    EXPECT_EQ("Full Model Display Name", *info.display_name);
    ASSERT_TRUE(info.publisher.has_value());
    EXPECT_EQ("TestPublisher", *info.publisher);
    ASSERT_TRUE(info.license.has_value());
    EXPECT_EQ("Apache-2.0", *info.license);
    ASSERT_TRUE(info.license_description.has_value());
    EXPECT_EQ("Permissive open source license", *info.license_description);
    ASSERT_TRUE(info.task.has_value());
    EXPECT_EQ("text-generation", *info.task);
    ASSERT_TRUE(info.min_fl_version.has_value());
    EXPECT_EQ("1.0.0", *info.min_fl_version);
}

TEST_F(FileBasedParserTest, AllFields_NumericOptionals) {
    auto arr = LoadJsonArray("model_all_fields.json");
    ModelInfo info = arr.at(0).get<ModelInfo>();

    ASSERT_TRUE(info.file_size_mb.has_value());
    EXPECT_EQ(16384u, *info.file_size_mb);
    ASSERT_TRUE(info.supports_tool_calling.has_value());
    EXPECT_TRUE(*info.supports_tool_calling);
    ASSERT_TRUE(info.max_output_tokens.has_value());
    EXPECT_EQ(8192, *info.max_output_tokens);
}

TEST_F(FileBasedParserTest, AllFields_Runtime) {
    auto arr = LoadJsonArray("model_all_fields.json");
    ModelInfo info = arr.at(0).get<ModelInfo>();

    ASSERT_TRUE(info.runtime.has_value());
    EXPECT_EQ(DeviceType::NPU, info.runtime->device_type);
    EXPECT_EQ("QNN", info.runtime->execution_provider);
}

TEST_F(FileBasedParserTest, AllFields_PromptTemplate) {
    auto arr = LoadJsonArray("model_all_fields.json");
    ModelInfo info = arr.at(0).get<ModelInfo>();

    ASSERT_TRUE(info.prompt_template.has_value());
    EXPECT_EQ("<|system|>\n", info.prompt_template->system);
    EXPECT_EQ("<|user|>\n", info.prompt_template->user);
    EXPECT_EQ("<|assistant|>\n", info.prompt_template->assistant);
    EXPECT_EQ("<|endoftext|>", info.prompt_template->prompt);
}

TEST_F(FileBasedParserTest, AllFields_ModelSettings) {
    auto arr = LoadJsonArray("model_all_fields.json");
    ModelInfo info = arr.at(0).get<ModelInfo>();

    ASSERT_TRUE(info.model_settings.has_value());
    ASSERT_EQ(3u, info.model_settings->parameters.size());
    EXPECT_EQ("temperature", info.model_settings->parameters[0].name);
    ASSERT_TRUE(info.model_settings->parameters[0].value.has_value());
    EXPECT_EQ("0.7", *info.model_settings->parameters[0].value);
    EXPECT_EQ("top_p", info.model_settings->parameters[1].name);
    ASSERT_TRUE(info.model_settings->parameters[1].value.has_value());
    EXPECT_EQ("0.9", *info.model_settings->parameters[1].value);
    EXPECT_EQ("max_tokens", info.model_settings->parameters[2].name);
    EXPECT_FALSE(info.model_settings->parameters[2].value.has_value());
}

TEST_F(FileBasedParserTest, MinimalFields_RequiredOnly) {
    auto arr = LoadJsonArray("model_minimal_fields.json");
    ModelInfo info = arr.at(0).get<ModelInfo>();

    EXPECT_EQ("minimal-model", info.id);
    EXPECT_EQ("minimal-model", info.name);
    EXPECT_EQ(1u, info.version);
    EXPECT_EQ("minimal", info.alias);
    EXPECT_EQ("onnx", info.provider_type);
    EXPECT_EQ("text", info.model_type);
    EXPECT_FALSE(info.cached);
    EXPECT_EQ(0, info.created_at_unix);
}

TEST_F(FileBasedParserTest, MinimalFields_AllOptionalsAbsent) {
    auto arr = LoadJsonArray("model_minimal_fields.json");
    ModelInfo info = arr.at(0).get<ModelInfo>();

    EXPECT_FALSE(info.display_name.has_value());
    EXPECT_FALSE(info.publisher.has_value());
    EXPECT_FALSE(info.license.has_value());
    EXPECT_FALSE(info.license_description.has_value());
    EXPECT_FALSE(info.task.has_value());
    EXPECT_FALSE(info.file_size_mb.has_value());
    EXPECT_FALSE(info.supports_tool_calling.has_value());
    EXPECT_FALSE(info.max_output_tokens.has_value());
    EXPECT_FALSE(info.min_fl_version.has_value());
    EXPECT_FALSE(info.runtime.has_value());
    EXPECT_FALSE(info.prompt_template.has_value());
    EXPECT_FALSE(info.model_settings.has_value());
}

TEST_F(FileBasedParserTest, NullOptionals_AllOptionalsAbsent) {
    auto arr = LoadJsonArray("model_null_optionals.json");
    ModelInfo info = arr.at(0).get<ModelInfo>();

    EXPECT_EQ("model-null-optionals", info.id);
    EXPECT_EQ("null-opts", info.alias);

    // All explicitly-null fields should parse as absent
    EXPECT_FALSE(info.display_name.has_value());
    EXPECT_FALSE(info.publisher.has_value());
    EXPECT_FALSE(info.license.has_value());
    EXPECT_FALSE(info.license_description.has_value());
    EXPECT_FALSE(info.task.has_value());
    EXPECT_FALSE(info.file_size_mb.has_value());
    EXPECT_FALSE(info.supports_tool_calling.has_value());
    EXPECT_FALSE(info.max_output_tokens.has_value());
    EXPECT_FALSE(info.min_fl_version.has_value());
    EXPECT_FALSE(info.runtime.has_value());
    EXPECT_FALSE(info.prompt_template.has_value());
    EXPECT_FALSE(info.model_settings.has_value());
}

TEST_F(FileBasedParserTest, RealModelsList_ParseAllEntries) {
    auto arr = LoadJsonArray("real_models_list.json");
    ASSERT_EQ(4u, arr.size());

    for (const auto& j : arr) {
        EXPECT_NO_THROW({
            auto info = j.get<ModelInfo>();
            EXPECT_FALSE(info.id.empty());
            EXPECT_FALSE(info.name.empty());
            EXPECT_FALSE(info.alias.empty());
        });
    }
}

TEST_F(FileBasedParserTest, MalformedJson_Throws) {
    EXPECT_ANY_THROW({
        std::string raw = Testing::ReadFile(TestDataPath("malformed_models_list.json"));
        nlohmann::json::parse(raw);
    });
}
