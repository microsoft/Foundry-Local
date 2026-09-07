// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Tests for Session (base) and ChatSession.
// Unit tests validate state management without a model.
// Integration tests run actual inference against the shared test model.

#include "inferencing/generative/chat/chat_session.h"
#include "exception.h"
#include "inferencing/generative/toolcalling/raw_envelope_encoding.h"
#include "inferencing/model_load_manager.h"
#include "inferencing/generative/chat/search_options.h"
#include "inferencing/session/request.h"
#include "items/audio_item.h"
#include "items/image_item.h"
#include "items/text_item.h"
#include "items/tool_call_item.h"
#include "items/tool_result_item.h"
#include "ep_detection/ep_detector.h"
#include "logger.h"
#include "model.h"
#include "internal_api/null_session_manager.h"
#include "internal_api/test_helpers.h"
#include "internal_api/test_model_cache.h"
#include "utils/string_utils.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace fl;

// ===========================================================================
// Integration test fixture: loads the shared test model once per suite
// ===========================================================================

class ChatSessionTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    auto model_path = fl::test::GetTestModelPath(fl::test::kTestChatModelAlias);
    logger_ = std::make_unique<StderrLogger>();
    ep_detector_ = std::make_unique<test::CpuOnlyEpDetector>();
    load_manager_ = std::make_unique<ModelLoadManager>(*ep_detector_, *logger_);

    auto result = load_manager_->LoadModel(
        model_path.string(),
        fl::test::kTestChatModelAlias);

    ASSERT_EQ(result.status, ModelLoadManager::LoadStatus::kSuccess)
        << "Failed to load test model from: " << model_path;

    model_ = result.model;
  }

  static void TearDownTestSuite() {
    if (load_manager_) {
      load_manager_->UnloadModel(fl::test::kTestChatModelAlias);
    }

    load_manager_.reset();
    ep_detector_.reset();
    model_ = nullptr;
  }

  GenAIModelInstance& GetModel() { return *model_; }
  const Model& GetCatalogModel() { return catalog_model_; }

  Model MakeCatalogModelWithToolOutputEncoding(std::string encoding) {
    ModelInfo info;
    info.task = "chat-completion";
    info.SetPropertyStr(kToolOutputEncodingKey, std::move(encoding));
    return Model::FromModelInfo(std::move(info), "", svc_.download_manager, svc_.model_load_manager);
  }

  static inline std::unique_ptr<StderrLogger> logger_;
  static inline std::unique_ptr<test::CpuOnlyEpDetector> ep_detector_;
  static inline std::unique_ptr<ModelLoadManager> load_manager_;
  static inline GenAIModelInstance* model_ = nullptr;
  static inline fl::test::FakeServiceBindings svc_;
  static inline Model catalog_model_ = [] {
    ModelInfo info;
    info.task = "chat-completion";
    return Model::FromModelInfo(std::move(info), "", svc_.download_manager, svc_.model_load_manager);
  }();
  TelemetryLogger null_telemetry_{"test", fl::test::NullLog()};
  fl::test::NullSessionManager null_session_manager_;
};

// ===========================================================================
// Construction
// ===========================================================================

TEST_F(ChatSessionTest, ConstructWithModelOnly) {
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);
  EXPECT_EQ(session.MessageCount(), 0u);
  EXPECT_TRUE(session.Transcript().Empty());
  EXPECT_EQ(session.TurnCount(), 0u);
}

TEST_F(ChatSessionTest, InvalidModelRawEnvelopeDescriptorIsRejected) {
  auto catalog_model = MakeCatalogModelWithToolOutputEncoding("not-json");
  ChatSession session(catalog_model, GetModel(), *logger_, null_telemetry_);

  Request request;
  request.AddOwnedItem(std::make_unique<MessageItem>(FOUNDRY_LOCAL_ROLE_USER, "hello"));

  try {
    Response response;
    session.ProcessRequest(request, response);
    FAIL() << "Expected malformed model tool_output_encoding to be rejected";
  } catch (const fl::Exception& e) {
    EXPECT_EQ(e.code(), FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT);
    EXPECT_NE(std::string(e.what()).find("model property"), std::string::npos);
  }
}

TEST_F(ChatSessionTest, BlankRequestRawEnvelopeDescriptorDoesNotFallBackToModel) {
  auto catalog_model = MakeCatalogModelWithToolOutputEncoding(
      R"({"type":"raw_envelope","tool_name":"submit_change","start_marker":"<<<CHANGE","end_marker":"CHANGE>>>"})");
  ChatSession session(catalog_model, GetModel(), *logger_, null_telemetry_);

  Request request;
  request.AddOwnedItem(std::make_unique<MessageItem>(FOUNDRY_LOCAL_ROLE_USER, "hello"));
  request.options.Add(kToolOutputEncodingKey, "   ");

  try {
    Response response;
    session.ProcessRequest(request, response);
    FAIL() << "Expected blank request descriptor to be rejected";
  } catch (const fl::Exception& e) {
    EXPECT_EQ(e.code(), FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT);
    EXPECT_NE(std::string(e.what()).find("request option"), std::string::npos);
  }
}

TEST_F(ChatSessionTest, ValidRequestRawEnvelopeDescriptorOverridesInvalidModelProperty) {
  auto catalog_model = MakeCatalogModelWithToolOutputEncoding("not-json");
  ChatSession session(catalog_model, GetModel(), *logger_, null_telemetry_);

  Request request;
  request.AddOwnedItem(std::make_unique<MessageItem>(FOUNDRY_LOCAL_ROLE_USER, "Reply with one word."));
  request.options.Add(
      kToolOutputEncodingKey,
      R"({"type":"raw_envelope","tool_name":"submit_change","start_marker":"<<<CHANGE","end_marker":"CHANGE>>>"})");
  request.options.Add("max_output_tokens", "1");

  Response response;
  EXPECT_NO_THROW(session.ProcessRequest(request, response));
}

TEST_F(ChatSessionTest, InvalidRequestRawEnvelopeDescriptorDoesNotFallBackToValidModelProperty) {
  auto catalog_model = MakeCatalogModelWithToolOutputEncoding(
      R"({"type":"raw_envelope","tool_name":"submit_change","start_marker":"<<<CHANGE","end_marker":"CHANGE>>>"})");
  ChatSession session(catalog_model, GetModel(), *logger_, null_telemetry_);

  Request request;
  request.AddOwnedItem(std::make_unique<MessageItem>(FOUNDRY_LOCAL_ROLE_USER, "hello"));
  request.options.Add(kToolOutputEncodingKey, "not-json");

  try {
    Response response;
    session.ProcessRequest(request, response);
    FAIL() << "Expected malformed request tool_output_encoding to be rejected";
  } catch (const fl::Exception& e) {
    EXPECT_EQ(e.code(), FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT);
    EXPECT_NE(std::string(e.what()).find("request option"), std::string::npos);
  }
}

TEST_F(ChatSessionTest, ForcedRawEnvelopeRejectsExplicitResponseGuidance) {
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);
  session.AddToolDefinition({"submit_change", "Submit a change.", {}, ToolKind::kCustom});

  Request request;
  request.AddOwnedItem(std::make_unique<MessageItem>(FOUNDRY_LOCAL_ROLE_USER, "make a change"));
  request.options.Add(FOUNDRY_LOCAL_PARAM_TOOL_CHOICE, "required");
  request.options.Add(
      kToolOutputEncodingKey,
      R"({"type":"raw_envelope","tool_name":"submit_change","start_marker":"<<<CHANGE","end_marker":"CHANGE>>>"})");
  request.options.Add("guidance_type", "json_schema");
  request.options.Add("guidance_data", R"({"type":"object"})");
  request.forced_tool_choice = ForcedToolChoice{"submit_change", ToolKind::kCustom};

  try {
    Response response;
    session.ProcessRequest(request, response);
    FAIL() << "Expected explicit response guidance with forced raw output to be rejected";
  } catch (const fl::Exception& e) {
    EXPECT_EQ(e.code(), FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT);
    EXPECT_NE(std::string(e.what()).find("guidance"), std::string::npos);
    EXPECT_NE(std::string(e.what()).find("raw-envelope"), std::string::npos);
  }
}

// ===========================================================================
// Session::Run helpers
// ===========================================================================

namespace {
std::unique_ptr<Item> MakeMessage(flMessageRole role, const std::string& content) {
  return std::make_unique<MessageItem>(role, content);
}

// Returns the content of the first MESSAGE item with role "assistant",
// or empty string if none found.
std::string GetAssistantText(const Response& response) {
  for (const auto& item : response.items) {
    if (item->type == FOUNDRY_LOCAL_ITEM_MESSAGE) {
      const MessageItem& msg = static_cast<const MessageItem&>(*item);
      if (msg.role == FOUNDRY_LOCAL_ROLE_ASSISTANT) {
        return msg.GetSimpleText();
      }
    }
  }

  return {};
}
}  // namespace

// ===========================================================================
// Session::Run (integration — requires loaded model)
// ===========================================================================

TEST_F(ChatSessionTest, RunBasic) {
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);

  Request request;
  request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, "What is 2+2? Answer with just the number."));
  request.options.Add("max_output_tokens", "32");
  request.options.Add("temperature", "0");

  Response response;
  session.ProcessRequest(request, response);
  auto text = GetAssistantText(response);

  EXPECT_FALSE(text.empty());
  EXPECT_NE(text.find("4"), std::string::npos)
      << "Expected '4' in response. Got: " << text;
  EXPECT_EQ(response.finish_reason, FOUNDRY_LOCAL_FINISH_STOP);
  EXPECT_GT(response.usage.prompt_tokens, 0);
  EXPECT_GT(response.usage.completion_tokens, 0);
  EXPECT_EQ(response.usage.total_tokens,
            response.usage.prompt_tokens + response.usage.completion_tokens);

  // History should contain user + assistant
  EXPECT_EQ(session.MessageCount(), 2u);
  const auto& messages = session.Transcript().Messages();
  EXPECT_EQ(messages[0].role, FOUNDRY_LOCAL_ROLE_USER);
  EXPECT_EQ(messages[1].role, FOUNDRY_LOCAL_ROLE_ASSISTANT);
  EXPECT_EQ(messages[1].VisibleText(), text);
}

TEST_F(ChatSessionTest, ChatCompletionRejectsAudioInput) {
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);

  std::vector<std::unique_ptr<Item>> parts;
  parts.push_back(std::make_unique<AudioItem>(std::vector<std::uint8_t>(32000), "pcm"));

  Request request;
  request.AddOwnedItem(std::make_unique<MessageItem>(FOUNDRY_LOCAL_ROLE_USER, std::move(parts)));

  Response response;
  try {
    session.ProcessRequest(request, response);
    FAIL() << "Expected unsupported audio input to be rejected";
  } catch (const fl::Exception& e) {
    EXPECT_EQ(e.code(), FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT);
    EXPECT_NE(std::string(e.what()).find("AUDIO input is not supported"), std::string::npos);
    EXPECT_NE(std::string(e.what()).find("chat-completion"), std::string::npos);
  }
}

TEST_F(ChatSessionTest, ChatCompletionRejectsImageInput) {
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);

  std::vector<std::unique_ptr<Item>> parts;
  parts.push_back(std::make_unique<ImageItem>(std::vector<std::uint8_t>{1}, "png"));

  Request request;
  request.AddOwnedItem(std::make_unique<MessageItem>(FOUNDRY_LOCAL_ROLE_USER, std::move(parts)));

  Response response;
  try {
    session.ProcessRequest(request, response);
    FAIL() << "Expected image input to be rejected by a chat-completion model";
  } catch (const fl::Exception& e) {
    EXPECT_EQ(e.code(), FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT);
    EXPECT_NE(std::string(e.what()).find("IMAGE input is not supported"), std::string::npos);
    EXPECT_NE(std::string(e.what()).find("chat-completion"), std::string::npos);
  }
}

TEST_F(ChatSessionTest, RunWithStreaming) {
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);

  // Use a multi-token prompt with deterministic substrings so we can validate:
  //   1. Streaming actually delivers multiple deltas (callback_count >= 2),
  //      not a single coalesced item.
  //   2. The streamed content matches expectations (at least 2 of the 4 UK
  //      constituent country names appear). A 0.5B model may abbreviate or
  //      reorder; requiring a subset stays robust.
  Request request;
  request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, "Name the countries in the United Kingdom."));
  request.options.Add("max_output_tokens", "128");
  request.options.Add("temperature", "0");

  std::string streamed_text;
  int callback_count = 0;
  int local_user_data = 123;  // example user data to pass to callback
  fl::Session::StreamingCallbackFn callback_fn = [&](flStreamingCallbackData event, void* user_data) -> int {
    EXPECT_EQ(user_data, &local_user_data) << "User data pointer mismatch in callback";

    fl::ItemQueue* queue = reinterpret_cast<fl::ItemQueue*>(event.item_queue);
    auto item = queue->TryPop();
    if (!item) {
      // should never happen
      return 0;
    }

    EXPECT_EQ(item->type, FOUNDRY_LOCAL_ITEM_TEXT);
    auto& text_item = static_cast<fl::TextItem&>(*item);
    std::string delta_text = text_item.text;
    streamed_text += delta_text;
    ++callback_count;

    return 0;
  };

  session.SetStreamingCallback(callback_fn, &local_user_data);

  Response response;
  session.ProcessRequest(request, response);
  auto text = GetAssistantText(response);

  EXPECT_FALSE(text.empty());

  // Lowercase the final text for case-insensitive substring matches.
  std::string lower = fl::test::ToLower(text);

  const std::vector<std::string> uk_countries = {"england", "scotland", "wales", "ireland"};
  int found = 0;
  for (const auto& name : uk_countries) {
    if (lower.find(name) != std::string::npos) {
      ++found;
    }
  }

  EXPECT_GE(found, 2)
      << "Expected at least 2 UK country names in response. Got: " << text;

  // Streamed text should match the final result.
  EXPECT_EQ(streamed_text, text);

  // Real streaming must deliver more than a single coalesced delta.
  EXPECT_GE(callback_count, 2)
      << "Expected multiple streaming callbacks (real token-by-token streaming), "
      << "got " << callback_count << ". Final text: " << text;

  // Turn 2 — a context-dependent follow-up. Asking for the capital of each
  // exercises history-aware generation and gives a second deterministic
  // content check. Streaming state is reused on the same session.
  streamed_text.clear();
  callback_count = 0;

  Request request2;
  request2.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, "What is the capital of each?"));
  request2.options.Add("max_output_tokens", "128");
  request2.options.Add("temperature", "0");

  Response response2;
  session.ProcessRequest(request2, response2);
  auto text2 = GetAssistantText(response2);

  EXPECT_FALSE(text2.empty());

  std::string lower2 = fl::test::ToLower(text2);

  const std::vector<std::string> uk_capitals = {"london", "edinburgh", "cardiff", "belfast"};
  int found2 = 0;
  for (const auto& name : uk_capitals) {
    if (lower2.find(name) != std::string::npos) {
      ++found2;
    }
  }

  EXPECT_GE(found2, 2)
      << "Turn 2: expected at least 2 UK capital names in response. Got: " << text2;
  EXPECT_EQ(streamed_text, text2);
  EXPECT_GE(callback_count, 2)
      << "Turn 2: expected multiple streaming callbacks, got " << callback_count
      << ". Final text: " << text2;
}

TEST_F(ChatSessionTest, RunMultiTurn) {
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);

  // Turn 1
  Request req1;
  req1.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, "What is 2+2? Answer with just the number."));
  req1.options.Add("max_output_tokens", "32");
  req1.options.Add("temperature", "0");

  Response r1;
  session.ProcessRequest(req1, r1);
  auto t1 = GetAssistantText(r1);
  EXPECT_NE(t1.find("4"), std::string::npos)
      << "Turn 1: expected '4'. Got: " << t1;
  EXPECT_EQ(session.MessageCount(), 2u);
  EXPECT_EQ(session.TurnCount(), 1u);

  // Turn 2 — Run adds to existing history
  Request req2;
  req2.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, "Now add 1 to that. Answer with just the number."));
  req2.options.Add("max_output_tokens", "32");
  req2.options.Add("temperature", "0");

  Response r2;
  session.ProcessRequest(req2, r2);
  auto t2 = GetAssistantText(r2);
  EXPECT_NE(t2.find("5"), std::string::npos)
      << "Turn 2: expected '5'. Got: " << t2;
  EXPECT_EQ(session.MessageCount(), 4u);
}

TEST_F(ChatSessionTest, RunStreamingCancellation) {
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);

  Request request;
  request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, "Count from 1 to 100."));
  request.options.Add("max_output_tokens", "256");
  request.options.Add("temperature", "0");

  int tokens_received = 0;

  fl::Session::StreamingCallbackFn callback_fn = [&](flStreamingCallbackData event, void* /*user_data*/) -> int {
    fl::ItemQueue* queue = reinterpret_cast<fl::ItemQueue*>(event.item_queue);
    auto item = queue->TryPop();
    if (!item) {
      // should never happen
      return 0;
    }

    ++tokens_received;
    bool cancel = tokens_received >= 3;  // example cancellation condition: after receiving 3 tokens
    return cancel ? 1 : 0;               // cancel after 3 tokens
  };

  session.SetStreamingCallback(callback_fn);

  Response response;
  session.ProcessRequest(request, response);

  // Should have stopped early
  EXPECT_EQ(response.finish_reason, FOUNDRY_LOCAL_FINISH_NONE);
  // we check cancellation at the start of each loop and we don't use std::atomic to make processing cheaper
  // so allow for a couple of extra tokens to come through after the cancellation condition is met
  EXPECT_LE(tokens_received, 6);

  // Cancelled requests should not commit to history (delayed commit)
  EXPECT_EQ(session.MessageCount(), 0u);
  EXPECT_EQ(session.TurnCount(), 0u);
}

TEST_F(ChatSessionTest, CancellationFromTheLastQueuedCallbackPreventsCommit) {
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);

  Request request;
  request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, "Reply with one word."));
  request.options.Add("max_output_tokens", "1");
  request.options.Add("temperature", "0");

  session.SetStreamingCallback([](flStreamingCallbackData event, void*) {
    auto* queue = reinterpret_cast<fl::ItemQueue*>(event.item_queue);
    auto item = queue->TryPop();
    if (!item) {
      return 0;
    }

    // Generation has only one token to produce, so this callback remains outstanding after the generator finishes.
    // ProcessRequest must drain it and observe cancellation before committing the turn.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return 1;
  });

  Response response;
  session.ProcessRequest(request, response);

  EXPECT_EQ(response.finish_reason, FOUNDRY_LOCAL_FINISH_NONE);
  EXPECT_EQ(session.MessageCount(), 0u);
  EXPECT_EQ(session.TurnCount(), 0u);
}

// ===========================================================================
// SearchOptions::FromParameters
// ===========================================================================

TEST_F(ChatSessionTest, SearchOptionsFromParameters) {
  fl::KeyValuePairs params;
  params.Add(FOUNDRY_LOCAL_PARAM_TEMPERATURE, "0.7");
  params.Add(FOUNDRY_LOCAL_PARAM_TOP_P, "0.9");
  params.Add(FOUNDRY_LOCAL_PARAM_MAX_OUTPUT_TOKENS, "128");
  params.Add(FOUNDRY_LOCAL_PARAM_SEED, "42");

  auto opts = SearchOptions::FromParameters(params);

  EXPECT_FLOAT_EQ(*opts.temperature, 0.7f);
  EXPECT_FLOAT_EQ(*opts.top_p, 0.9f);
  EXPECT_EQ(*opts.max_output_tokens, 128);
  EXPECT_EQ(*opts.seed, 42);
  EXPECT_FALSE(opts.top_k.has_value());
  EXPECT_FALSE(opts.frequency_penalty.has_value());
}

TEST_F(ChatSessionTest, SearchOptionsFromEmptyParameters) {
  fl::KeyValuePairs params;
  auto opts = SearchOptions::FromParameters(params);

  EXPECT_FALSE(opts.temperature.has_value());
  EXPECT_FALSE(opts.max_output_tokens.has_value());
}

// ===========================================================================
// Request-scoped system prefix (`instructions`) against a warm session.
//
// The prefix is baked into a generator's prompt, so a turn that changes it must rebuild while a turn that repeats it
// keeps the KV cache. Public usage always reports the complete logical prompt, so warm results are compared with an
// equivalent freshly rebuilt session rather than exposing the internal suffix-token optimization.
// ===========================================================================

namespace {

/// Run one turn. `instructions` is the request-scoped system prefix; empty means none.
Response RunTurnResponse(ChatSession& session, const std::string& user_text, const std::string& instructions,
                         bool disable_tools = false, int max_output_tokens = 8) {
  Request request;
  if (!user_text.empty()) {
    request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, user_text));
  }

  if (!instructions.empty()) {
    request.options.Add(kSystemPromptOption, instructions);
  }
  if (disable_tools) {
    request.options.Add(FOUNDRY_LOCAL_PARAM_TOOL_CHOICE, "none");
  }

  request.options.Add("max_output_tokens", std::to_string(max_output_tokens));
  request.options.Add("temperature", "0");

  Response response;
  session.ProcessRequest(request, response);
  return response;
}

fl::TokenUsage RunTurn(ChatSession& session, const std::string& user_text, const std::string& instructions) {
  return RunTurnResponse(session, user_text, instructions).usage;
}

}  // namespace

TEST_F(ChatSessionTest, UnchangedInstructionsKeepTheCachedGeneratorAndDoNotRepeatThePrefix) {
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);

  const std::string instructions = "You are a terse assistant that answers in one word.";
  const std::string first_user = "Say ok.";
  const std::string second_user = "Say ok again.";

  auto first_response = RunTurnResponse(session, first_user, instructions);
  const auto second = RunTurn(session, second_user, instructions);

  EXPECT_GT(first_response.usage.prompt_tokens, 0);
  EXPECT_GT(second.prompt_tokens, 0);
  EXPECT_GT(second.prompt_tokens, first_response.usage.prompt_tokens)
      << "the second turn's input sequence includes the conversation already held by the generator";
  EXPECT_LE(second.completion_tokens, 8);
  ASSERT_EQ(session.Transcript().Turns().size(), 2u);
  EXPECT_TRUE(session.Transcript().Turns()[1].tokens.pre_turn.has_value())
      << "unchanged instructions should append to the existing generator rather than rebuild it";

  // The prefix is request state and never enters the conversation record: two turns, four messages, no system
  // message among them.
  ASSERT_EQ(session.MessageCount(), 4u);
  for (const auto& message : session.Transcript().Messages()) {
    EXPECT_NE(message.role, FOUNDRY_LOCAL_ROLE_SYSTEM) << "the system prefix must not be committed to history";
  }
}

TEST_F(ChatSessionTest, WarmTurnThatStopsBeforeItsLimitReportsStop) {
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);

  const std::string instructions = "Answer each request with only the word ok.";
  RunTurnResponse(session, "Say ok.", instructions, /*disable_tools=*/false, /*max_output_tokens=*/64);
  const auto second =
      RunTurnResponse(session, "Say ok again.", instructions, /*disable_tools=*/false, /*max_output_tokens=*/64);

  EXPECT_EQ(second.finish_reason, FOUNDRY_LOCAL_FINISH_STOP);
  EXPECT_GT(second.usage.completion_tokens, 0);
  EXPECT_LT(second.usage.completion_tokens, 64);
  EXPECT_EQ(second.usage.total_tokens, second.usage.prompt_tokens + second.usage.completion_tokens);
  EXPECT_TRUE(session.Transcript().Turns()[1].tokens.pre_turn.has_value());
}

TEST_F(ChatSessionTest, ChangedInstructionsRebuildTheGeneratorWithTheNewPrefix) {
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);

  const std::string first_instructions = "You are a terse assistant that answers in one word.";
  // Deliberately much longer: a rebuilt prompt has to carry it, so the token count cannot be mistaken for an append.
  const std::string second_instructions =
      "You are an extremely careful assistant. Answer in one word. Do not explain yourself. Do not apologise. "
      "Do not add pleasantries. Do not restate the question. Keep every answer as short as it can possibly be.";

  const auto first = RunTurnResponse(session, "Say ok.", first_instructions);
  const auto second = RunTurnResponse(session, "Say ok again.", first_instructions);
  const auto rebuilt = RunTurnResponse(session, "Say ok once more.", second_instructions);

  ChatSession fresh_session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);
  Request fresh_request;
  fresh_request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, "Say ok."));
  fresh_request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_ASSISTANT, GetAssistantText(first)));
  fresh_request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, "Say ok again."));
  fresh_request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_ASSISTANT, GetAssistantText(second)));
  fresh_request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, "Say ok once more."));
  fresh_request.options.Add(kSystemPromptOption, second_instructions);
  fresh_request.options.Add("max_output_tokens", "8");
  fresh_request.options.Add("temperature", "0");

  Response fresh;
  fresh_session.ProcessRequest(fresh_request, fresh);

  EXPECT_EQ(rebuilt.usage.prompt_tokens, fresh.usage.prompt_tokens)
      << "changing instructions must rebuild the same prompt as a fresh session";
  EXPECT_EQ(rebuilt.usage.total_tokens, rebuilt.usage.prompt_tokens + rebuilt.usage.completion_tokens);

  // Still nothing in the record: a changed prefix replaces the old one rather than stacking another system message.
  ASSERT_EQ(session.MessageCount(), 6u);
  for (const auto& message : session.Transcript().Messages()) {
    EXPECT_NE(message.role, FOUNDRY_LOCAL_ROLE_SYSTEM) << "the system prefix must not be committed to history";
  }
}

TEST_F(ChatSessionTest, RemovingToolDefinitionsRebuildsWithTheCurrentToolSet) {
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);
  session.AddToolDefinition({"lookup", "Look up a value",
                             R"({"type":"object","properties":{"key":{"type":"string"}}})"});

  const std::string first_user = "Reply with the single word ok.";
  const std::string second_user = "Reply with the single word done.";
  auto first_response = RunTurnResponse(session, first_user, "", /*disable_tools=*/true);
  ASSERT_TRUE(session.RemoveToolDefinition("lookup"));

  const auto after_removal = RunTurn(session, second_user, "");

  ChatSession rebuilt_session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);
  Request rebuilt_request;
  rebuilt_request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, first_user));
  rebuilt_request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_ASSISTANT, GetAssistantText(first_response)));
  rebuilt_request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, second_user));
  rebuilt_request.options.Add("max_output_tokens", "8");
  rebuilt_request.options.Add("temperature", "0");

  Response rebuilt_response;
  rebuilt_session.ProcessRequest(rebuilt_request, rebuilt_response);

  EXPECT_EQ(after_removal.prompt_tokens, rebuilt_response.usage.prompt_tokens)
      << "removing tools must rebuild with the same prompt a fresh session sees";
}

TEST_F(ChatSessionTest, AutoModeGeneratedToolCallInvalidatesTheCachedGenerator) {
  const ToolDefinition tool{"lookup", "Look up a value",
                            R"({"type":"object","properties":{"key":{"type":"string"}},"required":["key"]})"};
  const std::string first_user = "Call lookup with key alpha. Do not answer without calling the tool.";
  const std::string second_user = "Reply with the single word done without calling a tool.";

  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);
  session.AddToolDefinition(tool);

  Request first_request;
  first_request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, first_user));
  first_request.options.Add(FOUNDRY_LOCAL_PARAM_TOOL_CHOICE, "auto");
  first_request.options.Add("max_output_tokens", "64");
  first_request.options.Add("temperature", "0");

  Response first_response;
  session.ProcessRequest(first_request, first_response);

  const auto generated_call = std::find_if(first_response.items.begin(), first_response.items.end(),
                                           [](const auto& item) {
                                             return item->type == FOUNDRY_LOCAL_ITEM_TOOL_CALL;
                                           });
  ASSERT_NE(generated_call, first_response.items.end()) << "the deterministic prompt must produce a tool call";

  Request second_request;
  second_request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, second_user));
  second_request.options.Add(FOUNDRY_LOCAL_PARAM_TOOL_CHOICE, "auto");
  second_request.options.Add("max_output_tokens", "8");
  second_request.options.Add("temperature", "0");

  Response warm_response;
  session.ProcessRequest(second_request, warm_response);

  ChatSession rebuilt_session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);
  rebuilt_session.AddToolDefinition(tool);

  Request rebuilt_request;
  rebuilt_request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, first_user));
  for (const auto& item : first_response.items) {
    if (item->type == FOUNDRY_LOCAL_ITEM_MESSAGE) {
      rebuilt_request.AddOwnedItem(std::make_unique<MessageItem>(static_cast<const MessageItem&>(*item)));
    } else if (item->type == FOUNDRY_LOCAL_ITEM_TOOL_CALL) {
      rebuilt_request.AddOwnedItem(std::make_unique<ToolCallItem>(static_cast<const ToolCallItem&>(*item)));
    }
  }
  rebuilt_request.AddOwnedItem(MakeMessage(FOUNDRY_LOCAL_ROLE_USER, second_user));
  rebuilt_request.options.Add(FOUNDRY_LOCAL_PARAM_TOOL_CHOICE, "auto");
  rebuilt_request.options.Add("max_output_tokens", "8");
  rebuilt_request.options.Add("temperature", "0");

  Response rebuilt_response;
  rebuilt_session.ProcessRequest(rebuilt_request, rebuilt_response);

  EXPECT_EQ(warm_response.usage.prompt_tokens, rebuilt_response.usage.prompt_tokens)
      << "a generated auto-mode call must force the next turn to rebuild from structured history";
}

// ===========================================================================
// Turns that carry no message of their own.
// ===========================================================================

TEST_F(ChatSessionTest, InstructionsAloneOnANewConversationGenerate) {
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);

  const auto usage = RunTurn(session, "", "Reply with the single word: ok");

  EXPECT_GT(usage.prompt_tokens, 0) << "the system prefix is the prompt";
  ASSERT_EQ(session.MessageCount(), 1u) << "only the assistant turn is recorded";
  EXPECT_EQ(session.Transcript().Messages()[0].role, FOUNDRY_LOCAL_ROLE_ASSISTANT);
}

TEST_F(ChatSessionTest, AnEmptyInputContinuesAWarmConversation) {
  // The warm half of the previous_response_id-with-empty-input case. The cached generator cannot append an empty
  // message list, so the turn rebuilds from committed history and produces the next assistant turn — which is what
  // the cold path does with the same request.
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);

  RunTurn(session, "Name a colour. Answer with one word.", "");
  ASSERT_EQ(session.MessageCount(), 2u);

  const auto continued = RunTurn(session, "", "");

  EXPECT_GT(continued.prompt_tokens, 0);
  // No input message was added, so the turn contributes exactly one assistant message.
  ASSERT_EQ(session.MessageCount(), 3u);
  EXPECT_EQ(session.Transcript().Messages()[2].role, FOUNDRY_LOCAL_ROLE_ASSISTANT);
  EXPECT_EQ(session.Transcript().TurnCount(), 2u);
}

TEST_F(ChatSessionTest, ARequestWithNothingAtAllIsAClientError) {
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);

  Request request;
  Response response;

  try {
    session.ProcessRequest(request, response);
    FAIL() << "expected a request with no content, no media, no instructions and no history to be rejected";
  } catch (const fl::Exception& ex) {
    // A caller mistake, so it must map to HTTP 400 — never a 500.
    EXPECT_EQ(ex.code(), FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT);
    EXPECT_NE(std::string(ex.what()).find("nothing to generate from"), std::string::npos) << ex.what();
  }
}

TEST_F(ChatSessionTest, AnEmptyToolResultForAnUnknownCallReportsTheCorrelationError) {
  // Precedence: an empty tool result carries no content, but "unknown call id" is the real problem and must not be
  // reported as an empty request.
  ChatSession session(GetCatalogModel(), GetModel(), *logger_, null_telemetry_);

  Request request;
  request.AddOwnedItem(std::make_unique<ToolResultItem>("call_missing", ""));

  Response response;
  try {
    session.ProcessRequest(request, response);
    FAIL() << "expected the unknown tool call id to be rejected";
  } catch (const fl::Exception& ex) {
    EXPECT_EQ(ex.code(), FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT);
    EXPECT_NE(std::string(ex.what()).find("unknown tool call id"), std::string::npos) << ex.what();
  }
}
