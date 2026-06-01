// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Unit tests for OnnxChatGenerator's vision-input message rewriting.
//
// Covers `TransformMessagesForVision` — the only static helper exposed for
// testability without loading a model. Image-byte extraction is covered by
// ImageItemTest (item_test.cc); model-bound vision behaviour is exercised by
// the integration tests.

#include "inferencing/generative/chat/onnx_chat_generator.h"

#include "items/image_item.h"
#include "items/message_item.h"
#include "items/text_item.h"

#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace fl;

namespace {

// Build a single-text MessageItem with the given role/text.
MessageItem MakeTextMessage(flMessageRole role, std::string text) {
  return MessageItem(role, std::move(text));
}

// Build a multi-part MessageItem from a list of (text + image) factory parts.
MessageItem MakeImageMessage(flMessageRole role, std::string text) {
  std::vector<std::unique_ptr<Item>> parts;
  // One byte of image payload — content shape doesn't matter, just needs to
  // exist and be non-empty so OgaImages would (in production) accept it.
  static const std::uint8_t kFakeImage[] = {0x89};
  parts.emplace_back(std::make_unique<ImageItem>(kFakeImage, sizeof(kFakeImage), "image/png"));
  parts.emplace_back(std::make_unique<TextItem>(std::move(text)));
  return MessageItem(role, std::move(parts));
}

}  // namespace

// ---------------------------------------------------------------------------
// TransformMessagesForVision
// ---------------------------------------------------------------------------

TEST(OnnxChatGeneratorVision, TransformRewritesLastUserAsStructuredContent) {
  std::vector<MessageItem> messages;
  messages.push_back(MakeTextMessage(FOUNDRY_LOCAL_ROLE_SYSTEM, "you are helpful"));
  messages.push_back(MakeImageMessage(FOUNDRY_LOCAL_ROLE_USER, "describe the image"));

  std::string json_str = OnnxChatGenerator::TransformMessagesForVision(messages);
  auto json = nlohmann::json::parse(json_str);

  ASSERT_TRUE(json.is_array());
  ASSERT_EQ(json.size(), 2u);

  // System message: plain string content.
  EXPECT_EQ(json[0]["role"], "system");
  EXPECT_EQ(json[0]["content"], "you are helpful");

  // Last user message: structured array with image sentinel + text.
  EXPECT_EQ(json[1]["role"], "user");
  ASSERT_TRUE(json[1]["content"].is_array());
  ASSERT_EQ(json[1]["content"].size(), 2u);
  EXPECT_EQ(json[1]["content"][0]["type"], "image");
  EXPECT_EQ(json[1]["content"][1]["type"], "text");
  EXPECT_EQ(json[1]["content"][1]["text"], "describe the image");
}

TEST(OnnxChatGeneratorVision, TransformPreservesPriorTextOnlyMessages) {
  std::vector<MessageItem> messages;
  messages.push_back(MakeTextMessage(FOUNDRY_LOCAL_ROLE_USER, "hi"));
  messages.push_back(MakeTextMessage(FOUNDRY_LOCAL_ROLE_ASSISTANT, "hello"));
  messages.push_back(MakeImageMessage(FOUNDRY_LOCAL_ROLE_USER, "what is this"));

  auto json = nlohmann::json::parse(OnnxChatGenerator::TransformMessagesForVision(messages));

  ASSERT_EQ(json.size(), 3u);
  EXPECT_EQ(json[0]["content"], "hi");
  EXPECT_EQ(json[1]["content"], "hello");
  ASSERT_TRUE(json[2]["content"].is_array());
  EXPECT_EQ(json[2]["content"][1]["text"], "what is this");
}

TEST(OnnxChatGeneratorVision, TransformOnlyRewritesFinalUserMessage) {
  // Two user messages; only the latter should be rewritten.
  std::vector<MessageItem> messages;
  messages.push_back(MakeTextMessage(FOUNDRY_LOCAL_ROLE_USER, "first"));
  messages.push_back(MakeTextMessage(FOUNDRY_LOCAL_ROLE_ASSISTANT, "ok"));
  messages.push_back(MakeImageMessage(FOUNDRY_LOCAL_ROLE_USER, "second"));

  auto json = nlohmann::json::parse(OnnxChatGenerator::TransformMessagesForVision(messages));

  ASSERT_EQ(json.size(), 3u);
  EXPECT_TRUE(json[0]["content"].is_string());
  EXPECT_EQ(json[0]["content"], "first");
  EXPECT_TRUE(json[2]["content"].is_array());
}
