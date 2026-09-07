// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Model-free tests for the authoritative chat transcript: item ingestion, tool-call correlation, turn commit /
// undo semantics, and the JSON projection handed to the chat template.

#include "inferencing/generative/chat/chat_transcript.h"

#include "exception.h"
#include "inferencing/generative/chat/chat_template.h"
#include "inferencing/session/request.h"
#include "items/message_item.h"
#include "items/text_item.h"
#include "items/tool_call_item.h"
#include "items/tool_result_item.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace fl;

namespace {

TranscriptToolCall MakeCall(std::string call_id, std::string name, std::string arguments) {
  return MakeSuppliedToolCall(std::move(call_id), std::move(name), std::move(arguments));
}

TranscriptMessage MakeAssistant(std::string text, std::vector<TranscriptToolCall> calls) {
  TranscriptMessage assistant;
  assistant.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;
  assistant.AppendText(std::move(text));
  for (auto& call : calls) {
    assistant.AppendToolCall(std::move(call));
  }

  return assistant;
}

TranscriptMessage UserMessage(std::string text) {
  return {FOUNDRY_LOCAL_ROLE_USER, std::move(text)};
}

/// Commit a plain "user asks, assistant answers" turn.
void CommitTextTurn(ChatTranscript& transcript, const std::string& question, const std::string& answer) {
  transcript.CommitTurn({UserMessage(question)}, MakeAssistant(answer, {}), {});
}

}  // namespace

// ===========================================================================
// Template projection — BuildChatMessagesJson
// ===========================================================================

TEST(ChatTemplateProjectionTest, PlainConversationProjectsRoleAndContent) {
  std::vector<TranscriptMessage> messages = {
      {FOUNDRY_LOCAL_ROLE_SYSTEM, "You are helpful."},
      {FOUNDRY_LOCAL_ROLE_USER, "Hello"},
      {FOUNDRY_LOCAL_ROLE_ASSISTANT, "Hi"}};

  EXPECT_EQ(BuildChatMessagesJson(messages),
            R"([{"role":"system","content":"You are helpful."},)"
            R"({"role":"user","content":"Hello"},)"
            R"({"role":"assistant","content":"Hi"}])");
}

TEST(ChatTemplateProjectionTest, ToolExchangeProjectsCallIdsArgumentsAndResultIds) {
  std::vector<TranscriptMessage> messages = {
      {FOUNDRY_LOCAL_ROLE_SYSTEM, "You are helpful."},
      {FOUNDRY_LOCAL_ROLE_USER, "Weather in Seattle?"},
      MakeAssistant("Let me check.", {MakeCall("call_1", "get_weather", R"({"city":"Seattle"})")}),
      TranscriptMessage::ToolResult("call_1", "sunny"),
      {FOUNDRY_LOCAL_ROLE_ASSISTANT, "It is sunny."}};

  EXPECT_EQ(BuildChatMessagesJson(messages),
            R"([{"role":"system","content":"You are helpful."},)"
            R"({"role":"user","content":"Weather in Seattle?"},)"
            R"({"role":"assistant","content":"Let me check.","tool_calls":[{"id":"call_1","type":"function",)"
            R"("function":{"name":"get_weather","arguments":{"city":"Seattle"}}}]},)"
            R"({"role":"tool","content":"sunny","tool_call_id":"call_1"},)"
            R"({"role":"assistant","content":"It is sunny."}])");
}

TEST(ChatTemplateProjectionTest, MultipleToolCallsKeepEmissionOrder) {
  std::vector<TranscriptMessage> messages = {
      MakeAssistant("", {MakeCall("call_1", "first", R"({"a":1})"), MakeCall("call_2", "second", R"({"b":2})")})};

  EXPECT_EQ(BuildChatMessagesJson(messages),
            R"([{"role":"assistant","content":"","tool_calls":[)"
            R"({"id":"call_1","type":"function","function":{"name":"first","arguments":{"a":1}}},)"
            R"({"id":"call_2","type":"function","function":{"name":"second","arguments":{"b":2}}}]}])");
}

TEST(ChatTemplateProjectionTest, ReasoningIsProjectedAlongsideToolCalls) {
  TranscriptMessage assistant;
  assistant.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;
  assistant.AppendReasoning("The user wants weather.");
  assistant.AppendText("Checking.");
  assistant.AppendToolCall(MakeCall("call_1", "get_weather", R"({"city":"Seattle"})"));

  EXPECT_EQ(BuildChatMessagesJson({assistant}),
            R"([{"role":"assistant","content":"Checking.","reasoning_content":"The user wants weather.",)"
            R"("tool_calls":[{"id":"call_1","type":"function",)"
            R"("function":{"name":"get_weather","arguments":{"city":"Seattle"}}}]}])");
}

TEST(ChatTemplateProjectionTest, ReasoningWithoutToolCallsIsNotReplayed) {
  TranscriptMessage assistant;
  assistant.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;
  assistant.AppendReasoning("Thinking out loud.");
  assistant.AppendText("The answer is 4.");

  EXPECT_EQ(BuildChatMessagesJson({assistant}), R"([{"role":"assistant","content":"The answer is 4."}])");
}

TEST(ChatTemplateProjectionTest, EmptyArgumentsBecomeEmptyObject) {
  std::vector<TranscriptMessage> messages = {MakeAssistant("", {MakeCall("call_1", "now", "")})};

  EXPECT_EQ(BuildChatMessagesJson(messages),
            R"([{"role":"assistant","content":"","tool_calls":)"
            R"([{"id":"call_1","type":"function","function":{"name":"now","arguments":{}}}]}])");
}

TEST(ChatTemplateProjectionTest, GeneratedUnusableArgumentsProjectAsAnEmptyObject) {
  // The model emitted bytes that are not a JSON object. Projection must still render — the turn was already streamed
  // to the caller — using the normalized form while the raw bytes stay on the transcript.
  auto generated = MakeGeneratedToolCall("call_1", "get_weather", R"({"city":)");
  ASSERT_FALSE(generated.arguments_usable);

  TranscriptMessage assistant;
  assistant.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;
  assistant.AppendToolCall(generated.call);

  EXPECT_EQ(BuildChatMessagesJson({assistant}),
            R"([{"role":"assistant","content":"","tool_calls":)"
            R"([{"id":"call_1","type":"function","function":{"name":"get_weather","arguments":{}}}]}])");
  EXPECT_EQ(assistant.ToolCalls()[0]->arguments, R"({"city":)");
}

TEST(ChatTemplateProjectionTest, EmptyToolResultKeepsCallIdAndEmptyContent) {
  std::vector<TranscriptMessage> messages = {TranscriptMessage::ToolResult("call_1", "")};

  EXPECT_EQ(BuildChatMessagesJson(messages), R"([{"role":"tool","content":"","tool_call_id":"call_1"}])");
}

TEST(ChatTemplateProjectionTest, ParticipantNameIsProjected) {
  std::vector<TranscriptMessage> messages = {{FOUNDRY_LOCAL_ROLE_USER, "Hello", "alice"}};

  EXPECT_EQ(BuildChatMessagesJson(messages), R"([{"role":"user","content":"Hello","name":"alice"}])");
}

// ===========================================================================
// Item ingestion — BuildTranscriptMessages
// ===========================================================================

TEST(TranscriptIngestTest, ToolCallItemsFoldIntoAdjacentAssistantMessage) {
  Request request;
  request.AddOwnedItem(std::make_unique<MessageItem>(FOUNDRY_LOCAL_ROLE_USER, "Weather?"));
  request.AddOwnedItem(std::make_unique<MessageItem>(FOUNDRY_LOCAL_ROLE_ASSISTANT, "Let me check."));
  request.AddOwnedItem(std::make_unique<ToolCallItem>("call_1", "get_weather", R"({"city":"Seattle"})"));
  request.AddOwnedItem(std::make_unique<ToolCallItem>("call_2", "get_time", R"({})"));

  auto messages = BuildTranscriptMessages(request.items);

  ASSERT_EQ(messages.size(), 2u);
  EXPECT_EQ(messages[1].role, FOUNDRY_LOCAL_ROLE_ASSISTANT);
  EXPECT_EQ(messages[1].VisibleText(), "Let me check.");

  auto calls = messages[1].ToolCalls();
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_EQ(calls[0]->call_id, "call_1");
  EXPECT_EQ(calls[0]->name, "get_weather");
  EXPECT_EQ(calls[0]->arguments, R"({"city":"Seattle"})");
  EXPECT_EQ(calls[1]->call_id, "call_2");
  EXPECT_EQ(calls[1]->kind, ToolCallKind::kFunction);
}

TEST(TranscriptIngestTest, ToolCallItemWithoutAdjacentAssistantStartsNewMessage) {
  Request request;
  request.AddOwnedItem(std::make_unique<MessageItem>(FOUNDRY_LOCAL_ROLE_USER, "Weather?"));
  request.AddOwnedItem(std::make_unique<ToolCallItem>("call_1", "get_weather", R"({})"));

  auto messages = BuildTranscriptMessages(request.items);

  ASSERT_EQ(messages.size(), 2u);
  EXPECT_EQ(messages[1].role, FOUNDRY_LOCAL_ROLE_ASSISTANT);
  EXPECT_EQ(messages[1].VisibleText(), "");
  ASSERT_EQ(messages[1].ToolCalls().size(), 1u);
  EXPECT_EQ(messages[1].ToolCalls()[0]->call_id, "call_1");
}

TEST(TranscriptIngestTest, ToolResultKeepsCallIdAndEmptyResult) {
  Request request;
  request.AddOwnedItem(std::make_unique<ToolResultItem>("call_1", ""));

  auto messages = BuildTranscriptMessages(request.items);

  ASSERT_EQ(messages.size(), 1u);
  EXPECT_EQ(messages[0].role, FOUNDRY_LOCAL_ROLE_TOOL);
  EXPECT_EQ(messages[0].tool_call_id, "call_1");
  EXPECT_EQ(messages[0].VisibleText(), "");
}

TEST(TranscriptIngestTest, TypedTextPartsKeepVisibleAndReasoningApart) {
  std::vector<std::unique_ptr<Item>> parts;
  parts.push_back(std::make_unique<TextItem>("thinking", FOUNDRY_LOCAL_TEXT_ITEM_TYPE_REASONING));
  parts.push_back(std::make_unique<TextItem>("answer", FOUNDRY_LOCAL_TEXT_ITEM_TYPE_DEFAULT));

  Request request;
  request.AddOwnedItem(std::make_unique<MessageItem>(FOUNDRY_LOCAL_ROLE_ASSISTANT, std::move(parts)));

  auto messages = BuildTranscriptMessages(request.items);

  ASSERT_EQ(messages.size(), 1u);
  EXPECT_EQ(messages[0].ReasoningText(), "thinking");
  EXPECT_EQ(messages[0].VisibleText(), "answer");
}

// ===========================================================================
// Outstanding-call correlation
// ===========================================================================

TEST(ChatTranscriptTest, CommittedCallBecomesOutstanding) {
  ChatTranscript transcript;
  transcript.CommitTurn({UserMessage("Weather?")}, MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")}), {});

  EXPECT_EQ(transcript.MessageCount(), 2u);
  EXPECT_EQ(transcript.TurnCount(), 1u);
  EXPECT_TRUE(transcript.IsOutstanding("call_1"));
  EXPECT_EQ(transcript.OutstandingCallCount(), 1u);
}

TEST(ChatTranscriptTest, ResultConsumesOutstandingCall) {
  ChatTranscript transcript;
  transcript.CommitTurn({UserMessage("Weather?")}, MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")}), {});
  transcript.CommitTurn({TranscriptMessage::ToolResult("call_1", "sunny")}, MakeAssistant("It is sunny.", {}), {});

  EXPECT_FALSE(transcript.HasOutstandingCalls());
  EXPECT_EQ(transcript.MessageCount(), 4u);
}

TEST(ChatTranscriptTest, ResultsMayArriveOutOfOrder) {
  ChatTranscript transcript;
  transcript.CommitTurn({UserMessage("Both please")},
                        MakeAssistant("", {MakeCall("call_1", "first", "{}"), MakeCall("call_2", "second", "{}")}),
                        {});

  transcript.CommitTurn({TranscriptMessage::ToolResult("call_2", "second done"),
                         TranscriptMessage::ToolResult("call_1", "first done")},
                        MakeAssistant("Done.", {}), {});

  EXPECT_EQ(transcript.OutstandingCallCount(), 0u);
}

TEST(ChatTranscriptTest, EmptyResultStringIsAccepted) {
  ChatTranscript transcript;
  transcript.CommitTurn({UserMessage("Weather?")}, MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")}), {});
  transcript.CommitTurn({TranscriptMessage::ToolResult("call_1", "")}, MakeAssistant("No data.", {}), {});

  EXPECT_FALSE(transcript.IsOutstanding("call_1"));
  EXPECT_EQ(transcript.Messages()[2].VisibleText(), "");
}

TEST(ChatTranscriptTest, ResultWithoutCallIdIsRejected) {
  ChatTranscript transcript;
  transcript.CommitTurn({UserMessage("Weather?")}, MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")}), {});

  try {
    transcript.ValidateInputs({TranscriptMessage::ToolResult("", "sunny")});
    FAIL() << "expected a missing call id to be rejected";
  } catch (const fl::Exception& ex) {
    EXPECT_EQ(ex.code(), FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT);
  }
}

TEST(ChatTranscriptTest, ResultForUnknownCallIsRejected) {
  ChatTranscript transcript;
  transcript.CommitTurn({UserMessage("Weather?")}, MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")}), {});

  try {
    transcript.ValidateInputs({TranscriptMessage::ToolResult("call_missing", "sunny")});
    FAIL() << "expected an unknown call id to be rejected";
  } catch (const fl::Exception& ex) {
    EXPECT_EQ(ex.code(), FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT);
  }
}

TEST(ChatTranscriptTest, SecondResultForTheSameCallIsRejected) {
  ChatTranscript transcript;
  transcript.CommitTurn({UserMessage("Weather?")}, MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")}), {});
  transcript.CommitTurn({TranscriptMessage::ToolResult("call_1", "sunny")}, MakeAssistant("It is sunny.", {}), {});

  try {
    transcript.ValidateInputs({TranscriptMessage::ToolResult("call_1", "sunny again")});
    FAIL() << "expected an already answered call id to be rejected";
  } catch (const fl::Exception& ex) {
    EXPECT_EQ(ex.code(), FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT);
  }
}

TEST(ChatTranscriptTest, TwoResultsForTheSameCallInOneBatchAreRejected) {
  ChatTranscript transcript;
  transcript.CommitTurn({UserMessage("Weather?")}, MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")}), {});

  EXPECT_THROW(transcript.ValidateInputs({TranscriptMessage::ToolResult("call_1", "sunny"),
                                          TranscriptMessage::ToolResult("call_1", "sunny")}),
               fl::Exception);
}

TEST(ChatTranscriptTest, ReplayedCallIdMustBeNonEmptyAndUnique) {
  ChatTranscript transcript;
  transcript.CommitTurn({UserMessage("Weather?")}, MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")}), {});

  EXPECT_THROW(transcript.ValidateInputs({MakeAssistant("", {MakeCall("", "get_weather", "{}")})}), fl::Exception);
  EXPECT_THROW(transcript.ValidateInputs({MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")})}),
               fl::Exception);
  EXPECT_THROW(transcript.ValidateInputs({MakeAssistant("", {MakeCall("call_9", "a", "{}"),
                                                             MakeCall("call_9", "b", "{}")})}),
               fl::Exception);
}

TEST(ChatTranscriptTest, ReplayedCallCanBeAnsweredWithinTheSameBatch) {
  ChatTranscript transcript;
  std::vector<TranscriptMessage> inputs = {UserMessage("Weather?"),
                                           MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")}),
                                           TranscriptMessage::ToolResult("call_1", "sunny")};

  EXPECT_NO_THROW(transcript.ValidateInputs(inputs));

  transcript.CommitTurn(std::move(inputs), MakeAssistant("It is sunny.", {}), {});
  EXPECT_EQ(transcript.MessageCount(), 4u);
  EXPECT_FALSE(transcript.HasOutstandingCalls());
}

TEST(ChatTranscriptTest, ResultBeforeItsCallInTheSameBatchIsRejected) {
  ChatTranscript transcript;
  std::vector<TranscriptMessage> inputs = {TranscriptMessage::ToolResult("call_1", "sunny"),
                                           MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")})};

  EXPECT_THROW(transcript.ValidateInputs(inputs), fl::Exception);
}

// ===========================================================================
// Atomicity — a rejected turn must leave nothing behind
// ===========================================================================

TEST(ChatTranscriptTest, RejectedTurnLeavesTranscriptUnchanged) {
  ChatTranscript transcript;
  CommitTextTurn(transcript, "Hello", "Hi");

  EXPECT_THROW(transcript.CommitTurn({TranscriptMessage::ToolResult("call_missing", "sunny")},
                                     MakeAssistant("ignored", {}), {}),
               fl::Exception);

  EXPECT_EQ(transcript.MessageCount(), 2u);
  EXPECT_EQ(transcript.TurnCount(), 1u);
  EXPECT_FALSE(transcript.HasOutstandingCalls());
}

TEST(ChatTranscriptTest, FailedGenerationCommitsNoOutstandingCall) {
  ChatTranscript transcript;
  transcript.CommitTurn({UserMessage("Weather?")}, MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")}), {});

  // A generation that re-uses an already issued call ID is rejected, and the rejection must not register the call.
  auto duplicate = MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")});
  EXPECT_THROW(transcript.ValidateGeneratedOutput(duplicate), fl::Exception);
  EXPECT_THROW(transcript.CommitTurn({UserMessage("Again?")}, duplicate, {}), fl::Exception);

  EXPECT_EQ(transcript.MessageCount(), 2u);
  EXPECT_EQ(transcript.TurnCount(), 1u);
  EXPECT_EQ(transcript.OutstandingCallCount(), 1u);
}

// Generation has already been streamed to the caller by the time a turn is committed, so unusable model arguments
// must never fail the request. Each shape the model can get wrong is checked end to end: the turn commits, the raw
// bytes survive, and the conversation still projects on every later turn.
class GeneratedArgumentsTest : public ::testing::TestWithParam<std::string> {};

TEST_P(GeneratedArgumentsTest, UnusableGeneratedArgumentsCommitAndStayRenderable) {
  const std::string& raw = GetParam();

  auto generated = MakeGeneratedToolCall("call_1", "get_weather", raw);
  EXPECT_FALSE(generated.arguments_usable) << "raw: " << raw;
  EXPECT_EQ(generated.call.arguments, raw);
  EXPECT_TRUE(generated.call.normalized_arguments.is_object());
  EXPECT_TRUE(generated.call.normalized_arguments.empty());

  TranscriptMessage assistant;
  assistant.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;
  assistant.AppendToolCall(std::move(generated.call));

  ChatTranscript transcript;
  EXPECT_NO_THROW(transcript.ValidateGeneratedOutput(assistant));
  EXPECT_NO_THROW(transcript.CommitTurn({UserMessage("Weather?")}, assistant, {}));

  EXPECT_EQ(transcript.MessageCount(), 2u);
  EXPECT_TRUE(transcript.IsOutstanding("call_1"));

  // Raw bytes are what the authoritative record reports.
  ASSERT_EQ(transcript.Messages()[1].ToolCalls().size(), 1u);
  EXPECT_EQ(transcript.Messages()[1].ToolCalls()[0]->arguments, raw);

  // The next turn rebuilds from this transcript, so projection must succeed.
  EXPECT_NO_THROW(BuildChatMessagesJson(transcript.Messages()));

  // And answering the call still works, so the conversation can continue.
  EXPECT_NO_THROW(transcript.ValidateInputs({TranscriptMessage::ToolResult("call_1", "sunny")}));
}

INSTANTIATE_TEST_SUITE_P(UnusableShapes, GeneratedArgumentsTest,
                         ::testing::Values(std::string("null"),
                                           std::string(R"("Seattle")"),
                                           std::string("[1,2]"),
                                           std::string("42"),
                                           std::string(R"({"city":)")));

TEST(ChatTranscriptTest, UsableGeneratedArgumentsKeepRawAndNormalizedInSync) {
  auto generated = MakeGeneratedToolCall("call_1", "get_weather", R"({"city":"Seattle"})");

  EXPECT_TRUE(generated.arguments_usable);
  EXPECT_EQ(generated.call.arguments, R"({"city":"Seattle"})");
  EXPECT_EQ(generated.call.normalized_arguments.dump(), R"({"city":"Seattle"})");
}

TEST(ChatTranscriptTest, GeneratedCallWithoutArgumentsNormalizesToAnEmptyObject) {
  auto generated = MakeGeneratedToolCall("call_1", "now", "");

  EXPECT_TRUE(generated.arguments_usable);
  EXPECT_EQ(generated.call.arguments, "");
  EXPECT_TRUE(generated.call.normalized_arguments.is_object());
}

TEST(ChatTranscriptTest, SuppliedCallWithUnusableArgumentsIsRejected) {
  // Caller-supplied replay stays strict: a client sending arguments that are not a JSON object gets an explicit
  // error instead of a silently altered conversation.
  for (const std::string& raw : {std::string("null"), std::string(R"("Seattle")"), std::string("[1,2]"),
                                 std::string(R"({"city":)")}) {
    try {
      MakeSuppliedToolCall("call_1", "get_weather", raw);
      FAIL() << "expected supplied arguments to be rejected: " << raw;
    } catch (const fl::Exception& ex) {
      EXPECT_EQ(ex.code(), FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT) << "raw: " << raw;
    }
  }
}

TEST(TranscriptIngestTest, SuppliedToolCallItemWithUnusableArgumentsIsRejected) {
  Request request;
  request.AddOwnedItem(std::make_unique<ToolCallItem>("call_1", "get_weather", "[1,2]"));

  EXPECT_THROW(BuildTranscriptMessages(request.items), fl::Exception);
}

TEST(ChatTranscriptTest, CommittedTranscriptAlwaysProjects) {
  ChatTranscript transcript;
  transcript.CommitTurn({UserMessage("Weather?")}, MakeAssistant("", {MakeCall("call_1", "get_weather", "")}), {});
  transcript.CommitTurn({TranscriptMessage::ToolResult("call_1", "sunny")}, MakeAssistant("It is sunny.", {}), {});

  EXPECT_NO_THROW(BuildChatMessagesJson(transcript.Messages()));
}

// ===========================================================================
// Undo
// ===========================================================================

TEST(ChatTranscriptTest, UndoRemovesTurnAndItsOutstandingCalls) {
  ChatTranscript transcript;
  CommitTextTurn(transcript, "Hello", "Hi");
  transcript.CommitTurn({UserMessage("Weather?")}, MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")}),
                        {10, 24});

  auto tokens = transcript.UndoTurns(1);

  ASSERT_TRUE(tokens.pre_turn.has_value());
  EXPECT_EQ(*tokens.pre_turn, 10);
  EXPECT_EQ(tokens.post_turn, 24);
  EXPECT_EQ(transcript.MessageCount(), 2u);
  EXPECT_EQ(transcript.TurnCount(), 1u);
  EXPECT_FALSE(transcript.HasOutstandingCalls());
}

TEST(ChatTranscriptTest, UndoneCallIdIsNoLongerAnswerable) {
  ChatTranscript transcript;
  transcript.CommitTurn({UserMessage("Weather?")}, MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")}), {});
  transcript.UndoTurns(1);

  EXPECT_THROW(transcript.ValidateInputs({TranscriptMessage::ToolResult("call_1", "sunny")}), fl::Exception);
}

TEST(ChatTranscriptTest, UndoRestoresAConsumedCallToOutstanding) {
  ChatTranscript transcript;
  transcript.CommitTurn({UserMessage("Weather?")}, MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")}), {});
  transcript.CommitTurn({TranscriptMessage::ToolResult("call_1", "sunny")}, MakeAssistant("It is sunny.", {}), {});
  ASSERT_FALSE(transcript.HasOutstandingCalls());

  transcript.UndoTurns(1);

  EXPECT_TRUE(transcript.IsOutstanding("call_1"));
  EXPECT_EQ(transcript.MessageCount(), 2u);
}

TEST(ChatTranscriptTest, UndoOfARebuiltTurnReportsNoRewindPoint) {
  ChatTranscript transcript;
  CommitTextTurn(transcript, "Hello", "Hi");
  // A turn that rebuilt its generator has no pre-turn boundary — its input is part of the generator's prompt.
  transcript.CommitTurn({UserMessage("Weather?")}, MakeAssistant("Sunny.", {}), {std::nullopt, 24});

  auto tokens = transcript.UndoTurns(1);

  EXPECT_FALSE(tokens.pre_turn.has_value());
  EXPECT_EQ(transcript.TurnCount(), 1u);
}

TEST(ChatTranscriptTest, UndoAcrossARebuiltTurnReportsNoRewindPoint) {
  ChatTranscript transcript;
  // Turn 1 appended to an existing generator, turn 2 rebuilt it, turn 3 appended to the rebuilt generator.
  transcript.CommitTurn({UserMessage("one")}, MakeAssistant("1", {}), {5, 10});
  transcript.CommitTurn({UserMessage("two")}, MakeAssistant("2", {}), {std::nullopt, 30});
  transcript.CommitTurn({UserMessage("three")}, MakeAssistant("3", {}), {30, 40});

  // Undoing only the last turn lands on a boundary that belongs to the current generator.
  auto one_turn = transcript.UndoTurns(1);
  ASSERT_TRUE(one_turn.pre_turn.has_value());
  EXPECT_EQ(*one_turn.pre_turn, 30);
  EXPECT_EQ(transcript.TurnCount(), 2u);

  // Undoing back past the rebuild must not hand back turn 1's offset: the rebuild reset the generator's token scale,
  // so rewinding there would splice the KV cache at a stale position.
  transcript.CommitTurn({UserMessage("three again")}, MakeAssistant("3", {}), {30, 40});
  auto across_rebuild = transcript.UndoTurns(2);
  EXPECT_FALSE(across_rebuild.pre_turn.has_value());
  EXPECT_EQ(transcript.TurnCount(), 1u);
  EXPECT_EQ(transcript.MessageCount(), 2u);
}

TEST(ChatTranscriptTest, UndoOfAppendedTurnsOnlyKeepsTheEarliestBoundary) {
  ChatTranscript transcript;
  transcript.CommitTurn({UserMessage("one")}, MakeAssistant("1", {}), {5, 10});
  transcript.CommitTurn({UserMessage("two")}, MakeAssistant("2", {}), {10, 20});
  transcript.CommitTurn({UserMessage("three")}, MakeAssistant("3", {}), {20, 30});

  auto tokens = transcript.UndoTurns(2);

  ASSERT_TRUE(tokens.pre_turn.has_value());
  EXPECT_EQ(*tokens.pre_turn, 10);
  EXPECT_EQ(transcript.TurnCount(), 1u);
}

TEST(ChatTranscriptTest, UndoingMoreTurnsThanExistIsRejected) {
  ChatTranscript transcript;
  CommitTextTurn(transcript, "Hello", "Hi");

  try {
    transcript.UndoTurns(2);
    FAIL() << "expected undo beyond the turn count to be rejected";
  } catch (const fl::Exception& ex) {
    EXPECT_EQ(ex.code(), FOUNDRY_LOCAL_ERROR_INVALID_USAGE);
  }

  EXPECT_EQ(transcript.TurnCount(), 1u);
}

// ===========================================================================
// Value semantics and full-history continuation
// ===========================================================================

TEST(ChatTranscriptTest, CopyAndMoveKeepIndependentState) {
  ChatTranscript original;
  original.CommitTurn({UserMessage("Weather?")}, MakeAssistant("", {MakeCall("call_1", "get_weather", "{}")}), {});

  ChatTranscript copy = original;
  copy.CommitTurn({TranscriptMessage::ToolResult("call_1", "sunny")}, MakeAssistant("It is sunny.", {}), {});

  EXPECT_EQ(copy.MessageCount(), 4u);
  EXPECT_FALSE(copy.HasOutstandingCalls());

  // The copy's extra turn must not have touched the original's messages or call state.
  EXPECT_EQ(original.MessageCount(), 2u);
  EXPECT_TRUE(original.IsOutstanding("call_1"));

  ChatTranscript moved = std::move(original);
  EXPECT_EQ(moved.MessageCount(), 2u);
  EXPECT_TRUE(moved.IsOutstanding("call_1"));
  EXPECT_NO_THROW(moved.ValidateInputs({TranscriptMessage::ToolResult("call_1", "sunny")}));
}

TEST(ChatTranscriptTest, FullHistoryContinuationProjectsTheWholeConversation) {
  ChatTranscript transcript;
  transcript.CommitTurn({UserMessage("Weather in Seattle?")},
                        MakeAssistant("", {MakeCall("call_1", "get_weather", R"({"city":"Seattle"})")}), {});
  transcript.CommitTurn({TranscriptMessage::ToolResult("call_1", "sunny")}, MakeAssistant("It is sunny.", {}), {});

  // What a rebuilt generator would see: the committed transcript plus the new turn's input.
  std::vector<TranscriptMessage> all_messages = transcript.Messages();
  all_messages.push_back(UserMessage("And tomorrow?"));

  EXPECT_EQ(BuildChatMessagesJson(all_messages),
            R"([{"role":"user","content":"Weather in Seattle?"},)"
            R"({"role":"assistant","content":"","tool_calls":[{"id":"call_1","type":"function",)"
            R"("function":{"name":"get_weather","arguments":{"city":"Seattle"}}}]},)"
            R"({"role":"tool","content":"sunny","tool_call_id":"call_1"},)"
            R"({"role":"assistant","content":"It is sunny."},)"
            R"({"role":"user","content":"And tomorrow?"}])");
}
