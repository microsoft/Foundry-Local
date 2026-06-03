// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Tests for response_converter.cc — BuildFailedResponseObject,
// BuildInitialResponseObject, EchoRequestParams (via Build*), and ToInputItems.
//
#include "inferencing/generative/openresponses/response_converter.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <string>

#include "items/image_item.h"
#include "items/message_item.h"
#include "items/text_item.h"

using namespace fl;
using namespace fl::responses;
using namespace fl::ResponseConverter;

// ========================================================================
// Helper: minimal ResponseCreateParams for echo tests
// ========================================================================

static ResponseCreateParams MakeTestParams() {
  ResponseCreateParams params;
  params.model = "test-model";
  params.input = std::string("Hello");
  params.instructions = "Be helpful";
  params.temperature = 0.7f;
  params.top_p = 0.9f;
  params.max_output_tokens = 100;
  params.presence_penalty = 0.1f;
  params.frequency_penalty = 0.2f;
  params.store = true;
  params.metadata["key1"] = "value1";
  params.user = "test-user";
  return params;
}

// ========================================================================
// BuildFailedResponseObject
// ========================================================================

TEST(ResponseConverterTest, BuildFailedResponse_HasErrorFields) {
  auto params = MakeTestParams();
  auto r = BuildFailedResponseObject("resp_123", 1000, "my-model", params,
                                     "server_error", "Something broke");

  EXPECT_EQ(r.id, "resp_123");
  EXPECT_EQ(r.created_at, 1000);
  EXPECT_EQ(r.model, "my-model");
  EXPECT_EQ(r.status, ResponseStatus::kFailed);
  ASSERT_TRUE(r.error.has_value());
  EXPECT_EQ(r.error->code, "server_error");
  EXPECT_EQ(r.error->message, "Something broke");
}

TEST(ResponseConverterTest, BuildFailedResponse_HasFailedAtTimestamp) {
  auto params = MakeTestParams();
  auto r = BuildFailedResponseObject("resp_1", 500, "m", params, "err", "msg");

  // failed_at should be set to a recent wall-clock time (not created_at)
  ASSERT_TRUE(r.failed_at.has_value());
  EXPECT_GT(*r.failed_at, 0);
}

TEST(ResponseConverterTest, BuildFailedResponse_OutputIsEmpty) {
  auto params = MakeTestParams();
  auto r = BuildFailedResponseObject("resp_1", 500, "m", params, "err", "msg");

  EXPECT_TRUE(r.output.empty());
  EXPECT_TRUE(r.output_text.empty());
}

TEST(ResponseConverterTest, BuildFailedResponse_EchoesRequestParams) {
  auto params = MakeTestParams();
  auto r = BuildFailedResponseObject("resp_1", 100, "m", params, "e", "m");

  EXPECT_EQ(r.instructions, "Be helpful");
  ASSERT_TRUE(r.temperature.has_value());
  EXPECT_FLOAT_EQ(*r.temperature, 0.7f);
  ASSERT_TRUE(r.top_p.has_value());
  EXPECT_FLOAT_EQ(*r.top_p, 0.9f);
  ASSERT_TRUE(r.max_output_tokens.has_value());
  EXPECT_EQ(*r.max_output_tokens, 100);
  ASSERT_TRUE(r.presence_penalty.has_value());
  EXPECT_FLOAT_EQ(*r.presence_penalty, 0.1f);
  ASSERT_TRUE(r.frequency_penalty.has_value());
  EXPECT_FLOAT_EQ(*r.frequency_penalty, 0.2f);
  EXPECT_TRUE(r.store);
  EXPECT_EQ(r.metadata.at("key1"), "value1");
  ASSERT_TRUE(r.user.has_value());
  EXPECT_EQ(*r.user, "test-user");
  EXPECT_EQ(r.truncation, "disabled");
}

// ========================================================================
// BuildInitialResponseObject
// ========================================================================

TEST(ResponseConverterTest, BuildInitialResponse_StatusIsInProgress) {
  auto params = MakeTestParams();
  auto r = BuildInitialResponseObject("resp_42", 2000, "streaming-model", params);

  EXPECT_EQ(r.id, "resp_42");
  EXPECT_EQ(r.created_at, 2000);
  EXPECT_EQ(r.model, "streaming-model");
  EXPECT_EQ(r.status, ResponseStatus::kInProgress);
}

TEST(ResponseConverterTest, BuildInitialResponse_NoCompletedOrFailedTimestamps) {
  auto params = MakeTestParams();
  auto r = BuildInitialResponseObject("resp_1", 100, "m", params);

  EXPECT_FALSE(r.completed_at.has_value());
  EXPECT_FALSE(r.failed_at.has_value());
}

TEST(ResponseConverterTest, BuildInitialResponse_EchoesRequestParams) {
  auto params = MakeTestParams();
  params.parallel_tool_calls = false;

  auto r = BuildInitialResponseObject("resp_1", 100, "m", params);

  EXPECT_EQ(r.instructions, "Be helpful");
  EXPECT_FALSE(r.parallel_tool_calls);
  EXPECT_TRUE(r.store);
}

// ========================================================================
// EchoRequestParams — tested indirectly via BuildResponseObject
// ========================================================================

TEST(ResponseConverterTest, EchoRequestParams_ToolsEchoed) {
  ResponseCreateParams params;
  params.model = "m";
  params.input = std::string("hi");

  responses::FunctionDefinition fn;
  fn.name = "get_weather";
  fn.description = "Get weather info";
  responses::ToolDefinition tool;
  tool.function = fn;
  params.tools = std::vector<responses::ToolDefinition>{tool};
  params.tool_choice = std::string("auto");

  auto r = BuildInitialResponseObject("resp_1", 100, "m", params);

  ASSERT_EQ(r.tools.size(), 1u);
  EXPECT_EQ(r.tools[0].function.name, "get_weather");
  ASSERT_TRUE(r.tool_choice.has_value());
  auto* tc_str = std::get_if<std::string>(&*r.tool_choice);
  ASSERT_NE(tc_str, nullptr);
  EXPECT_EQ(*tc_str, "auto");
}

TEST(ResponseConverterTest, EchoRequestParams_ParallelToolCallsDefaultTrue) {
  ResponseCreateParams params;
  params.model = "m";
  params.input = std::string("hi");
  // parallel_tool_calls not set → should default to true

  auto r = BuildInitialResponseObject("resp_1", 100, "m", params);
  EXPECT_TRUE(r.parallel_tool_calls);
}

TEST(ResponseConverterTest, EchoRequestParams_TextConfigEchoed) {
  ResponseCreateParams params;
  params.model = "m";
  params.input = std::string("hi");
  ResponseTextConfig text_cfg;
  text_cfg.format = "json_schema";
  text_cfg.json_schema = R"({"type":"object"})";
  params.text = text_cfg;

  auto r = BuildInitialResponseObject("resp_1", 100, "m", params);

  ASSERT_TRUE(r.text.has_value());
  EXPECT_EQ(r.text->format, "json_schema");
  ASSERT_TRUE(r.text->json_schema.has_value());
  EXPECT_EQ(*r.text->json_schema, R"({"type":"object"})");
}

TEST(ResponseConverterTest, EchoRequestParams_ReasoningConfigEchoed) {
  ResponseCreateParams params;
  params.model = "m";
  params.input = std::string("hi");
  ReasoningConfig rc;
  rc.effort = "high";
  rc.generate_summary = true;
  params.reasoning = rc;

  auto r = BuildInitialResponseObject("resp_1", 100, "m", params);

  ASSERT_TRUE(r.reasoning.has_value());
  ASSERT_TRUE(r.reasoning->effort.has_value());
  EXPECT_EQ(*r.reasoning->effort, "high");
  ASSERT_TRUE(r.reasoning->generate_summary.has_value());
  EXPECT_TRUE(*r.reasoning->generate_summary);
}

// ========================================================================
// ToInputItems
// ========================================================================

TEST(ResponseConverterTest, ToInputItems_StringInput) {
  nlohmann::json req = {{"input", "Hello world"}};
  auto items = ToInputItems(req);

  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0]["type"], "message");
  EXPECT_EQ(items[0]["role"], "user");
  EXPECT_EQ(items[0]["status"], "completed");
  EXPECT_EQ(items[0]["content"], "Hello world");
  // Should have a generated id
  EXPECT_TRUE(items[0].contains("id"));
  EXPECT_FALSE(items[0]["id"].get<std::string>().empty());
}

TEST(ResponseConverterTest, ToInputItems_WithInstructions) {
  nlohmann::json req = {{"instructions", "Be concise"}, {"input", "Hi"}};
  auto items = ToInputItems(req);

  ASSERT_EQ(items.size(), 2u);
  // First item is the system message from instructions
  EXPECT_EQ(items[0]["role"], "system");
  EXPECT_EQ(items[0]["content"], "Be concise");
  // Second is the user input
  EXPECT_EQ(items[1]["role"], "user");
  EXPECT_EQ(items[1]["content"], "Hi");
}

TEST(ResponseConverterTest, ToInputItems_ArrayInput_PreservesObjects) {
  nlohmann::json req = {
      {"input", nlohmann::json::array({
                    {{"type", "message"}, {"role", "user"}, {"content", "test"}},
                    {{"type", "function_call"}, {"name", "fn1"}, {"arguments", "{}"}},
                })}};

  auto items = ToInputItems(req);
  ASSERT_EQ(items.size(), 2u);
  EXPECT_EQ(items[0]["type"], "message");
  EXPECT_EQ(items[1]["type"], "function_call");
}

TEST(ResponseConverterTest, ToInputItems_ArrayInput_GeneratesIdsWhenMissing) {
  nlohmann::json req = {
      {"input", nlohmann::json::array({
                    {{"type", "message"}, {"role", "user"}, {"content", "test"}},
                })}};

  auto items = ToInputItems(req);
  ASSERT_EQ(items.size(), 1u);
  // ID should be generated with "msg" prefix
  std::string id = items[0]["id"].get<std::string>();
  EXPECT_TRUE(id.find("msg_") == 0);
}

TEST(ResponseConverterTest, ToInputItems_ArrayInput_PreservesExistingIds) {
  nlohmann::json req = {
      {"input", nlohmann::json::array({
                    {{"type", "message"}, {"id", "existing_id"}, {"role", "user"}, {"content", "test"}},
                })}};

  auto items = ToInputItems(req);
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0]["id"], "existing_id");
}

TEST(ResponseConverterTest, ToInputItems_NoInput_ReturnsEmptyArray) {
  nlohmann::json req = {{"model", "test"}};
  auto items = ToInputItems(req);
  EXPECT_TRUE(items.is_array());
  EXPECT_TRUE(items.empty());
}

TEST(ResponseConverterTest, ToInputItems_FunctionCallOutput_GetsFcoPrefix) {
  nlohmann::json req = {
      {"input", nlohmann::json::array({
                    {{"type", "function_call_output"}, {"call_id", "c1"}, {"output", "result"}},
                })}};

  auto items = ToInputItems(req);
  ASSERT_EQ(items.size(), 1u);
  std::string id = items[0]["id"].get<std::string>();
  EXPECT_TRUE(id.find("fco_") == 0);
}

// ========================================================================
// ToSessionRequest — vision input (input_image content)
//
// Verifies that input_image content parts are decoded into owning ImageItems
// and combined with adjacent input_text into MessageItem with typed parts.
// ========================================================================

namespace {

// "PNG\0" magic + a few bytes — just to give Base64Decode something
// non-trivial to round-trip and to provide a known byte count.
constexpr const char* kSamplePngBase64 = "iVBORw0KGgoAAAA=";  // 11 bytes decoded
constexpr size_t kSamplePngDecodedSize = 11;

ResponseCreateParams MakeImageRequest(const std::string& image_url, const std::string& text = "What is this?") {
  ResponseCreateParams params;
  params.model = "test-model";

  InputMessage msg;
  msg.role = "user";
  InputTextContent text_part;
  text_part.text = text;
  msg.content.push_back(text_part);
  InputImageContent image_part;
  image_part.detail = "auto";
  image_part.image_url = image_url;
  msg.content.push_back(image_part);

  params.input = std::vector<InputItem>{msg};
  return params;
}

}  // namespace

TEST(ResponseConverterTest, ToSessionRequest_InputImage_DataUrl_DecodesToImageItem) {
  std::string data_url = std::string("data:image/png;base64,") + kSamplePngBase64;
  auto params = MakeImageRequest(data_url);

  auto request = ToSessionRequest(params);

  // Expect a single MessageItem with [TextItem, ImageItem] parts.
  ASSERT_EQ(request.items.size(), 1u);
  auto* msg = dynamic_cast<MessageItem*>(request.items[0]);
  ASSERT_NE(msg, nullptr);
  ASSERT_EQ(msg->content.size(), 2u);

  // Part 0: text
  ASSERT_NE(msg->content[0].view, nullptr);
  EXPECT_EQ(msg->content[0].view->type, FOUNDRY_LOCAL_ITEM_TEXT);

  // Part 1: image with decoded bytes + correct MIME type.
  ASSERT_NE(msg->content[1].view, nullptr);
  ASSERT_EQ(msg->content[1].view->type, FOUNDRY_LOCAL_ITEM_IMAGE);
  const auto* img = static_cast<const ImageItem*>(msg->content[1].view);
  EXPECT_EQ(img->format, "image/png");
  EXPECT_EQ(img->data_size, kSamplePngDecodedSize);
  EXPECT_NE(img->data, nullptr);
}

TEST(ResponseConverterTest, ToSessionRequest_InputImage_DataUrl_MissingBase64Marker_Throws) {
  // Missing ";base64," — the converter requires base64 encoding.
  auto params = MakeImageRequest("data:image/png,plaintext");
  EXPECT_THROW(ToSessionRequest(params), std::exception);
}

TEST(ResponseConverterTest, ToSessionRequest_InputImage_HttpUrl_NotImplemented) {
  auto params = MakeImageRequest("https://example.com/image.png");
  EXPECT_THROW(ToSessionRequest(params), std::exception);
}

TEST(ResponseConverterTest, ToSessionRequest_InputImage_FileId_NotImplemented) {
  ResponseCreateParams params;
  params.model = "test-model";

  InputMessage msg;
  msg.role = "user";
  InputImageContent image_part;
  image_part.detail = "auto";
  image_part.file_id = "file_abc123";
  msg.content.push_back(image_part);
  params.input = std::vector<InputItem>{msg};

  EXPECT_THROW(ToSessionRequest(params), std::exception);
}

TEST(ResponseConverterTest, ToSessionRequest_InputImage_ImageOnlyMessage_GetsTextSentinel) {
  // Pure-image message (no text). The converter injects a single-space
  // text part so the chat template can render the message.
  std::string data_url = std::string("data:image/jpeg;base64,") + kSamplePngBase64;

  ResponseCreateParams params;
  params.model = "test-model";

  InputMessage msg;
  msg.role = "user";
  InputImageContent image_part;
  image_part.detail = "auto";
  image_part.image_url = data_url;
  msg.content.push_back(image_part);
  params.input = std::vector<InputItem>{msg};

  auto request = ToSessionRequest(params);

  ASSERT_EQ(request.items.size(), 1u);
  auto* m = dynamic_cast<MessageItem*>(request.items[0]);
  ASSERT_NE(m, nullptr);
  ASSERT_EQ(m->content.size(), 2u);

  // The image part is added first, then the text sentinel.
  EXPECT_EQ(m->content[0].view->type, FOUNDRY_LOCAL_ITEM_IMAGE);
  EXPECT_EQ(m->content[1].view->type, FOUNDRY_LOCAL_ITEM_TEXT);
  const auto* img = static_cast<const ImageItem*>(m->content[0].view);
  EXPECT_EQ(img->format, "image/jpeg");
}
