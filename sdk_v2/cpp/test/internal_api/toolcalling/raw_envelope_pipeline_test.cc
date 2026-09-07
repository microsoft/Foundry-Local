// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// End-to-end path of a raw `*** Begin Patch` envelope: from the bytes the arbiter reads out of generated output,
// through the transcript that records the turn, and out onto both wire surfaces.
//
// The arbiter tests cover the grammar. These cover what happens to a committed call afterwards: that the ID minted
// when the envelope closed is the same one the stream, the final response, the stored conversation and the client's
// result all use, and that the payload stays byte-identical the whole way.
//
#include "inferencing/generative/toolcalling/generated_output_arbiter.h"

#include "contracts/chat_completions.h"
#include "contracts/chat_completions_converter.h"
#include "contracts/responses.h"
#include "inferencing/generative/chat/chat_template.h"
#include "inferencing/generative/chat/chat_transcript.h"
#include "inferencing/generative/openresponses/response_converter.h"
#include "inferencing/generative/openresponses/response_store.h"
#include "items/message_item.h"
#include "items/text_item.h"
#include "items/tool_call_item.h"
#include "items/tool_result_item.h"
#include "util/sha256.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace fl;
using json = nlohmann::json;

namespace {

const std::string kEnvelope = "*** Begin Patch\n*** Update File: src/a.txt\n@@\n-old\t\n+new  \n*** End Patch";

/// The dialect the pipeline suite runs against. The production path carries no literal for it — it is configured
/// per turn, and RawEnvelopePipelineTest.ArbitraryDialectTravelsTheWholePath runs the same path under another one.
constexpr const char* kRawTool = "apply_patch";

RawEnvelopeEncoding Dialect() {
  return RawEnvelopeEncoding{kRawTool, "*** Begin Patch", "*** End Patch"};
}

/// A turn whose caller forced the custom tool the dialect names. The forced choice is what makes a bare envelope
/// readable as a call; the kinds are what make the committed call report as `custom` rather than `function`.
ToolCallContext RawEnvelopeTurn() {
  ToolCallContext context;
  context.tool_call_start = "<tool_call>";
  context.tool_call_end = "</tool_call>";
  context.tool_output = true;
  context.tool_kinds = {{kRawTool, ToolKind::kCustom}, {"bash", ToolKind::kFunction}};
  context.tool_output_encoding = Dialect();
  context.forced_tool = ForcedToolChoice{kRawTool, ToolKind::kCustom};
  return context;
}

/// Read one turn's generated output the way both chat paths do — one byte at a time, then drained.
std::vector<ParsedToolCall> ReadGeneratedCalls(const std::string& generated, std::string* visible = nullptr,
                                               const ToolCallContext& context = RawEnvelopeTurn()) {
  GeneratedOutputArbiter arbiter(context);
  std::vector<ParsedToolCall> calls;

  const auto absorb = [&](GeneratedOutputArbiter::Output out) {
    for (auto& event : out.events) {
      if (auto* text = std::get_if<std::string>(&event)) {
        if (visible != nullptr) {
          *visible += *text;
        }
        continue;
      }

      calls.push_back(std::move(std::get<ParsedToolCall>(event)));
    }
  };

  for (char c : generated) {
    absorb(arbiter.Push(std::string(1, c)));
  }

  absorb(arbiter.Flush());
  return calls;
}

fl::Response ResponseWithItems(std::vector<std::unique_ptr<Item>> items) {
  fl::Response response;
  response.items = std::move(items);
  return response;
}

}  // namespace

// ==========================================================================
// Transcript: what a committed raw call looks like in the authoritative record.
// ==========================================================================

TEST(RawEnvelopePipelineTest, CommittedCallKeepsRawBytesAndRendersAsCustomInput) {
  const auto calls = ReadGeneratedCalls("Applying:\n" + kEnvelope + "\n");
  ASSERT_EQ(calls.size(), 1u);

  const auto generated = MakeGeneratedToolCall(calls[0].id, calls[0].name, calls[0].arguments,
                                               RawEnvelopeTurn().KindOf(calls[0].name), calls[0].raw_encoding);

  EXPECT_TRUE(generated.arguments_usable) << "a custom payload is text and can never be malformed";
  EXPECT_EQ(generated.call.kind, ToolKind::kCustom);
  EXPECT_EQ(generated.call.arguments, kEnvelope) << "the authoritative record keeps the bytes that were generated";
  EXPECT_EQ(generated.call.normalized_arguments, json::object({{"input", kEnvelope}}))
      << "template projection re-wraps the payload in the shape the model was prompted with";
}

TEST(RawEnvelopePipelineTest, TurnCommitsTheCallUnderTheIdTheEnvelopeMinted) {
  const auto calls = ReadGeneratedCalls(kEnvelope);
  ASSERT_EQ(calls.size(), 1u);

  TranscriptMessage assistant;
  assistant.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;
  assistant.AppendToolCall(
      MakeGeneratedToolCall(calls[0].id, calls[0].name, calls[0].arguments, ToolKind::kCustom, calls[0].raw_encoding)
          .call);

  ChatTranscript transcript;
  transcript.ValidateGeneratedOutput(assistant);
  transcript.CommitTurn({TranscriptMessage(FOUNDRY_LOCAL_ROLE_USER, "fix it")}, assistant, {});

  EXPECT_TRUE(transcript.IsOutstanding(calls[0].id));
  EXPECT_EQ(transcript.OutstandingCallCount(), 1u);

  const auto committed = transcript.Messages().back().ToolCalls();
  ASSERT_EQ(committed.size(), 1u);
  EXPECT_EQ(committed[0]->call_id, calls[0].id);
  EXPECT_EQ(committed[0]->arguments, kEnvelope);
}

TEST(RawEnvelopePipelineTest, SequentialEnvelopesCommitAsDistinctOutstandingCalls) {
  const std::string first = "*** Begin Patch\n*** Add File: a.txt\n+a\n*** End Patch";
  const std::string second = "*** Begin Patch\n*** Delete File: b.txt\n*** End Patch";

  const auto calls = ReadGeneratedCalls(first + "\nand then\n" + second + "\n");
  ASSERT_EQ(calls.size(), 2u);
  ASSERT_NE(calls[0].id, calls[1].id);

  TranscriptMessage assistant;
  assistant.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;
  for (const auto& call : calls) {
    assistant.AppendToolCall(
        MakeGeneratedToolCall(call.id, call.name, call.arguments, ToolKind::kCustom, call.raw_encoding).call);
  }

  ChatTranscript transcript;
  transcript.ValidateGeneratedOutput(assistant);
  transcript.CommitTurn({TranscriptMessage(FOUNDRY_LOCAL_ROLE_USER, "do both")}, assistant, {});

  EXPECT_EQ(transcript.OutstandingCallCount(), 2u);

  const auto committed = transcript.Messages().back().ToolCalls();
  ASSERT_EQ(committed.size(), 2u);
  EXPECT_EQ(committed[0]->arguments, first);
  EXPECT_EQ(committed[1]->arguments, second);
}

TEST(RawEnvelopePipelineTest, AdjacentRawEnvelopesKeepTheirSeparatorThroughTheGenerationGuard) {
  const std::string second = "*** Begin Patch\n*** Delete File: src/b.txt\n*** End Patch";
  GeneratedOutputArbiter arbiter(RawEnvelopeTurn());
  auto output = arbiter.Push(kEnvelope + "\n" + second);
  auto flushed = arbiter.Flush();
  output.events.insert(output.events.end(), std::make_move_iterator(flushed.events.begin()),
                       std::make_move_iterator(flushed.events.end()));

  AssistantTurnGuard guard;
  TranscriptMessage assistant;
  assistant.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;

  for (auto& event : output.events) {
    if (auto* text = std::get_if<std::string>(&event)) {
      if (guard.OfferVisibleText(*text) == TextDisposition::kEmit) {
        assistant.AppendText(*text);
      }
      continue;
    }

    auto call = std::move(std::get<ParsedToolCall>(event));
    ASSERT_TRUE(guard.OfferToolCall(call.raw_encoding.has_value()));
    assistant.AppendToolCall(
        MakeGeneratedToolCall(call.id, call.name, call.arguments, ToolKind::kCustom, call.raw_encoding).call);
  }

  EXPECT_FALSE(guard.TurnEnded());
  const auto projected = json::parse(BuildChatMessagesJson({assistant}));
  EXPECT_EQ(projected.at(0).at("content"), kEnvelope + "\n" + second);
  EXPECT_FALSE(projected.at(0).contains("tool_calls"));
}

TEST(RawEnvelopePipelineTest, RawCallAfterStructuredCallEndsBeforeItCanBeReported) {
  AssistantTurnGuard guard;
  EXPECT_TRUE(guard.OfferToolCall(/*raw_envelope=*/false));
  EXPECT_FALSE(guard.OfferToolCall(/*raw_envelope=*/true));
  EXPECT_TRUE(guard.TurnEnded());

  TranscriptMessage unrenderable;
  unrenderable.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;
  unrenderable.AppendToolCall(MakeGeneratedToolCall("call_structured", "bash", R"({"command":"pwd"})",
                                                    ToolKind::kFunction)
                                  .call);
  unrenderable.AppendToolCall(
      MakeGeneratedToolCall("call_raw", kRawTool, kEnvelope, ToolKind::kCustom, Dialect()).call);
  EXPECT_THROW(ValidateRenderableTurn(unrenderable), fl::Exception);
}

TEST(RawEnvelopePipelineTest, FailedResultAndRetryCorrelateOldAndNewCallIds) {
  const auto first_attempt = ReadGeneratedCalls(kEnvelope);
  ASSERT_EQ(first_attempt.size(), 1u);

  ChatTranscript transcript;

  TranscriptMessage assistant;
  assistant.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;
  assistant.AppendToolCall(
      MakeGeneratedToolCall(first_attempt[0].id, first_attempt[0].name, first_attempt[0].arguments,
                            ToolKind::kCustom, first_attempt[0].raw_encoding)
          .call);
  transcript.CommitTurn({TranscriptMessage(FOUNDRY_LOCAL_ROLE_USER, "fix it")}, assistant, {});

  // The client applies the patch, it fails, and it answers the call it was given with the failure text. Answering
  // is what clears the correlation — a failed result is still a result.
  const std::string failure = "error: context line 3 did not match";
  std::vector<TranscriptMessage> retry_inputs{TranscriptMessage::ToolResult(first_attempt[0].id, failure)};
  transcript.ValidateInputs(retry_inputs);

  // The model tries again with a corrected patch. That is a new envelope, so it commits under a new ID; the old ID
  // stays in the record as the one the failure answered.
  const std::string corrected = "*** Begin Patch\n*** Update File: src/a.txt\n@@\n-older\t\n+new  \n*** End Patch";
  const auto retry = ReadGeneratedCalls(corrected);
  ASSERT_EQ(retry.size(), 1u);
  EXPECT_NE(retry[0].id, first_attempt[0].id) << "a retry is a distinct call, not a resend of the failed one";

  TranscriptMessage retry_assistant;
  retry_assistant.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;
  retry_assistant.AppendToolCall(
      MakeGeneratedToolCall(retry[0].id, retry[0].name, retry[0].arguments, ToolKind::kCustom, retry[0].raw_encoding)
          .call);

  transcript.ValidateGeneratedOutput(retry_assistant);
  transcript.CommitTurn(std::move(retry_inputs), retry_assistant, {});

  EXPECT_FALSE(transcript.IsOutstanding(first_attempt[0].id)) << "the failed call was answered";
  EXPECT_TRUE(transcript.IsOutstanding(retry[0].id)) << "the retry is the call now awaiting a result";
  EXPECT_EQ(transcript.OutstandingCallCount(), 1u);

  // Both attempts, and the failure between them, survive in order so the next prompt can show the model why the
  // first patch was rejected.
  const auto& messages = transcript.Messages();
  ASSERT_EQ(messages.size(), 4u);
  EXPECT_EQ(messages[1].ToolCalls().at(0)->arguments, kEnvelope);
  EXPECT_EQ(messages[2].role, FOUNDRY_LOCAL_ROLE_TOOL);
  EXPECT_EQ(messages[2].tool_call_id, first_attempt[0].id);
  EXPECT_EQ(messages[2].VisibleText(), failure);
  EXPECT_EQ(messages[3].ToolCalls().at(0)->arguments, corrected);

  const auto projected = json::parse(BuildChatMessagesJson(messages));
  ASSERT_EQ(projected.size(), 4u);
  EXPECT_EQ(projected[1].at("content"), kEnvelope);
  EXPECT_FALSE(projected[1].contains("tool_calls"));
  EXPECT_EQ(projected[2].at("role"), "tool");
  EXPECT_EQ(projected[2].at("tool_call_id"), first_attempt[0].id);
  EXPECT_EQ(projected[2].at("content"), failure);
  EXPECT_EQ(projected[3].at("content"), corrected);
  EXPECT_FALSE(projected[3].contains("tool_calls"))
      << "cold replay must preserve the model's raw envelope dialect around the tool result and retry";
}

TEST(RawEnvelopePipelineTest, ReplayedCallFromAPriorTurnNormalizesLikeTheGeneratedOne) {
  const auto calls = ReadGeneratedCalls(kEnvelope);
  ASSERT_EQ(calls.size(), 1u);

  ToolCallItem replayed(calls[0].id, "apply_patch", kEnvelope);
  std::vector<Item*> items{&replayed};

  const auto messages = BuildTranscriptMessages(items, RawEnvelopeTurn().tool_kinds);
  ASSERT_EQ(messages.size(), 1u);

  const auto replayed_calls = messages[0].ToolCalls();
  ASSERT_EQ(replayed_calls.size(), 1u);
  EXPECT_EQ(replayed_calls[0]->call_id, calls[0].id);
  EXPECT_EQ(replayed_calls[0]->arguments, kEnvelope);
  EXPECT_EQ(replayed_calls[0]->normalized_arguments, json::object({{"input", kEnvelope}}));
}

// ==========================================================================
// Chat Completions surface.
// ==========================================================================

TEST(RawEnvelopePipelineTest, ChatCompletionsReportsTheEnvelopeAsACustomCall) {
  const auto calls = ReadGeneratedCalls(kEnvelope + "\n");
  ASSERT_EQ(calls.size(), 1u);

  const auto call = chat_completions::MakeToolCall(calls[0].id, calls[0].name, calls[0].arguments,
                                                   RawEnvelopeTurn().KindOf(calls[0].name));
  const json serialized = call;

  ASSERT_TRUE(call.custom.has_value());
  EXPECT_EQ(serialized.at("type"), "custom");
  EXPECT_EQ(serialized.at("custom").at("name"), "apply_patch");
  EXPECT_EQ(serialized.at("custom").at("input"), kEnvelope);
  EXPECT_FALSE(serialized.contains("function")) << "a custom payload is never reported as JSON arguments";
}

TEST(RawEnvelopePipelineTest, ChatCompletionsStreamedAndFinalCallsShareOneId) {
  const auto calls = ReadGeneratedCalls(kEnvelope + "\n");
  ASSERT_EQ(calls.size(), 1u);

  auto streamed = chat_completions::MakeToolCall(calls[0].id, calls[0].name, calls[0].arguments, ToolKind::kCustom);
  streamed.index = 0;
  const json chunk = json::parse(
      chat_completions::FormatToolCallStreamingChunk({streamed}, "chatcmpl-1", 42, "test-model"));

  std::vector<std::unique_ptr<Item>> items;
  items.push_back(std::make_unique<ToolCallItem>(calls[0].id, calls[0].name, calls[0].arguments));
  auto response = ResponseWithItems(std::move(items));
  response.finish_reason = FOUNDRY_LOCAL_FINISH_TOOL_CALLS;

  const json final_response =
      chat_completions::BuildResponse(response, "chatcmpl-1", 42, "test-model", RawEnvelopeTurn().tool_kinds);
  const auto& final_call = final_response.at("choices").at(0).at("message").at("tool_calls").at(0);
  const auto& streamed_call = chunk.at("choices").at(0).at("delta").at("tool_calls").at(0);

  EXPECT_EQ(streamed_call.at("id"), final_call.at("id"));
  EXPECT_EQ(streamed_call.at("custom").at("input"), kEnvelope);
  EXPECT_EQ(final_call.at("custom").at("input"), kEnvelope);
  EXPECT_EQ(final_response.at("choices").at(0).at("finish_reason"), "tool_calls");
}

// ==========================================================================
// Responses surface.
// ==========================================================================

TEST(RawEnvelopePipelineTest, ResponsesStreamsTheEnvelopeAsCustomToolCallInput) {
  const auto calls = ReadGeneratedCalls(kEnvelope + "\n");
  ASSERT_EQ(calls.size(), 1u);

  ToolCallItem call(calls[0].id, calls[0].name, calls[0].arguments);
  int sequence_number = 0;
  const auto streamed = ResponseConverter::BuildToolCallStreamOutput(call, ToolKind::kCustom, 0, sequence_number);

  ASSERT_EQ(streamed.events.size(), 4u);

  const auto& completed = std::get<responses::CustomToolCallOutputItem>(streamed.completed_item);
  EXPECT_EQ(completed.call_id, calls[0].id);
  EXPECT_EQ(completed.name, "apply_patch");
  EXPECT_EQ(completed.input, kEnvelope);

  const json delta = streamed.events[1];
  EXPECT_EQ(delta.at("type"), "response.custom_tool_call_input.delta");
  EXPECT_EQ(delta.at("delta"), kEnvelope);
  EXPECT_EQ(delta.at("call_id"), calls[0].id);

  const json done = streamed.events[2];
  EXPECT_EQ(done.at("input"), kEnvelope);
  EXPECT_FALSE(done.contains("arguments"));
}

TEST(RawEnvelopePipelineTest, ResponsesStreamedAndFinalItemsAgreeOnIdAndPayload) {
  const auto calls = ReadGeneratedCalls(kEnvelope + "\n");
  ASSERT_EQ(calls.size(), 1u);

  ToolCallItem call(calls[0].id, calls[0].name, calls[0].arguments);
  int sequence_number = 0;
  const auto streamed = ResponseConverter::BuildToolCallStreamOutput(call, ToolKind::kCustom, 0, sequence_number);

  std::vector<std::unique_ptr<Item>> items;
  items.push_back(std::make_unique<ToolCallItem>(calls[0].id, calls[0].name, calls[0].arguments));
  const auto [output, output_text] =
      ResponseConverter::FromSessionResponse(ResponseWithItems(std::move(items)), RawEnvelopeTurn().tool_kinds);

  ASSERT_EQ(output.size(), 1u);
  const auto& streamed_item = std::get<responses::CustomToolCallOutputItem>(streamed.completed_item);
  const auto& final_item = std::get<responses::CustomToolCallOutputItem>(output.at(0));

  EXPECT_EQ(streamed_item.call_id, calls[0].id);
  EXPECT_EQ(streamed_item.call_id, final_item.call_id);
  EXPECT_EQ(streamed_item.input, final_item.input);
  EXPECT_EQ(final_item.input, kEnvelope);
  EXPECT_TRUE(output_text.empty()) << "a call is not assistant text";
}

// ==========================================================================
// Byte fidelity across the whole path.
// ==========================================================================

TEST(RawEnvelopePipelineTest, PayloadHashIsIdenticalAtEveryStageOfThePath) {
  const std::string expected_hash = Sha256String(kEnvelope);

  const auto calls = ReadGeneratedCalls("prefix\n" + kEnvelope + "\nsuffix");
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(Sha256String(calls[0].arguments), expected_hash);

  const auto committed = MakeGeneratedToolCall(calls[0].id, calls[0].name, calls[0].arguments, ToolKind::kCustom,
                                               calls[0].raw_encoding);
  EXPECT_EQ(Sha256String(committed.call.arguments), expected_hash);
  EXPECT_EQ(Sha256String(committed.call.normalized_arguments.at("input").get<std::string>()), expected_hash);

  const auto chat_call =
      chat_completions::MakeToolCall(calls[0].id, calls[0].name, calls[0].arguments, ToolKind::kCustom);
  ASSERT_TRUE(chat_call.custom.has_value());
  EXPECT_EQ(Sha256String(chat_call.custom->input), expected_hash);

  ToolCallItem item(calls[0].id, calls[0].name, calls[0].arguments);
  int sequence_number = 0;
  const auto streamed = ResponseConverter::BuildToolCallStreamOutput(item, ToolKind::kCustom, 0, sequence_number);
  EXPECT_EQ(Sha256String(std::get<responses::CustomToolCallOutputItem>(streamed.completed_item).input),
            expected_hash);
}

TEST(RawEnvelopePipelineTest, VisibleTextAroundAnEnvelopeIsPreservedInOrder) {
  std::string visible;
  const auto calls = ReadGeneratedCalls("Here is the fix:\n" + kEnvelope + "\nLet me know if it works.", &visible);

  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(visible, "Here is the fix:\n\nLet me know if it works.");
  EXPECT_EQ(calls[0].arguments, kEnvelope);
}

// ==========================================================================
// Provenance and prompt projection — replaying a turn in the dialect the model wrote it in.
//
// A structured call is projected into a later prompt as a `tool_calls` entry, because that is the only shape a chat
// template can render one in. A raw call was never written that way: the model produced envelope bytes as ordinary
// assistant content. Replaying it as a `tool_calls` entry would show the model a transcript of a turn it never had,
// in a dialect it does not use.
// ==========================================================================

namespace {

/// The transcript message a turn commits, built the way ChatSession builds it.
TranscriptMessage AssistantTurn(const std::string& text, const ParsedToolCall& call, ToolKind kind) {
  TranscriptMessage message;
  message.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;
  message.AppendText(text);
  message.AppendToolCall(MakeGeneratedToolCall(call.id, call.name, call.arguments, kind, call.raw_encoding).call);
  return message;
}

/// The single projected assistant message for a one-message transcript.
nlohmann::json ProjectOne(const TranscriptMessage& message) {
  return nlohmann::json::parse(BuildChatMessagesJson({message})).at(0);
}

}  // namespace

TEST(RawEnvelopePipelineTest, CommittedRawCallCarriesProvenanceAndTheExactDialect) {
  const auto calls = ReadGeneratedCalls(kEnvelope + "\n");
  ASSERT_EQ(calls.size(), 1u);

  const auto generated = MakeGeneratedToolCall(calls[0].id, calls[0].name, calls[0].arguments, ToolKind::kCustom,
                                               calls[0].raw_encoding);

  EXPECT_TRUE(generated.call.IsRawEnvelope());
  ASSERT_TRUE(generated.call.raw_encoding.has_value());
  EXPECT_EQ(*generated.call.raw_encoding, Dialect());
  EXPECT_EQ(generated.call.RawEnvelopeText(), kEnvelope) << "the model's own bytes, byte for byte";
}

TEST(RawEnvelopePipelineTest, StructuredCallIsProjectedAsAToolCallsEntry) {
  // The unchanged behaviour, stated so the raw case below is a contrast rather than an assertion in isolation.
  const auto calls = ReadGeneratedCalls(R"(<tool_call>{"name":"bash","arguments":{"cmd":"ls"}}</tool_call>)");
  ASSERT_EQ(calls.size(), 1u);
  ASSERT_FALSE(calls[0].raw_encoding.has_value());

  const auto entry = ProjectOne(AssistantTurn("Running it.", calls[0], ToolKind::kFunction));

  EXPECT_EQ(entry.at("content"), "Running it.");
  ASSERT_TRUE(entry.contains("tool_calls"));
  ASSERT_EQ(entry.at("tool_calls").size(), 1u);
  EXPECT_EQ(entry.at("tool_calls")[0].at("function").at("name"), "bash");
}

TEST(RawEnvelopePipelineTest, RawCallIsProjectedAsAssistantContentBytes) {
  const auto calls = ReadGeneratedCalls("Here is the fix:\n" + kEnvelope + "\n");
  ASSERT_EQ(calls.size(), 1u);

  const auto entry = ProjectOne(AssistantTurn("Here is the fix:\n", calls[0], ToolKind::kCustom));

  // The prompt shows exactly what the model produced: its prose, then its envelope, in that order.
  EXPECT_EQ(entry.at("content"), "Here is the fix:\n" + kEnvelope);
}

TEST(RawEnvelopePipelineTest, RawCallIsNotAlsoRenderedAsAToolCallsEntry) {
  // Double rendering would show the call twice — once as the bytes the model wrote, once as a structured entry it
  // never wrote. An empty `tool_calls` array is omitted too: some templates render it as an empty call block.
  const auto calls = ReadGeneratedCalls(kEnvelope + "\n");
  ASSERT_EQ(calls.size(), 1u);

  const auto entry = ProjectOne(AssistantTurn("", calls[0], ToolKind::kCustom));

  EXPECT_FALSE(entry.contains("tool_calls"));
  EXPECT_EQ(entry.at("content"), kEnvelope);
}

TEST(RawEnvelopePipelineTest, AMixedTurnProjectsEachCallInItsOwnShape) {
  // One raw call and one structured call in a single turn: the raw one belongs to content, the structured one to
  // `tool_calls`, and neither appears twice.
  const auto raw = ReadGeneratedCalls(kEnvelope + "\n");
  const auto structured = ReadGeneratedCalls(R"(<tool_call>{"name":"bash","arguments":{"cmd":"ls"}}</tool_call>)");
  ASSERT_EQ(raw.size(), 1u);
  ASSERT_EQ(structured.size(), 1u);

  TranscriptMessage message;
  message.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;
  message.AppendText("First:\n");
  message.AppendToolCall(
      MakeGeneratedToolCall(raw[0].id, raw[0].name, raw[0].arguments, ToolKind::kCustom, raw[0].raw_encoding).call);
  message.AppendToolCall(MakeGeneratedToolCall(structured[0].id, structured[0].name, structured[0].arguments,
                                               ToolKind::kFunction, std::nullopt)
                             .call);

  const auto entry = ProjectOne(message);

  EXPECT_EQ(entry.at("content"), "First:\n" + kEnvelope);
  ASSERT_TRUE(entry.contains("tool_calls"));
  ASSERT_EQ(entry.at("tool_calls").size(), 1u) << "only the structured call is declared";
  EXPECT_EQ(entry.at("tool_calls")[0].at("function").at("name"), "bash");
}

TEST(RawEnvelopePipelineTest, ClientSuppliedCustomCallReplaysAsStructured) {
  // Same tool, same payload bytes — but the client wrote it as a structured call, so that is how it replays. Only
  // the arbiter can mint raw provenance.
  std::vector<std::unique_ptr<Item>> items;
  items.push_back(std::make_unique<ToolCallItem>("call_1", kRawTool, kEnvelope, ToolKind::kCustom));

  std::vector<Item*> views;
  for (const auto& item : items) {
    views.push_back(item.get());
  }

  const auto messages = BuildTranscriptMessages(views, {{kRawTool, ToolKind::kCustom}});
  ASSERT_EQ(messages.size(), 1u);

  const auto calls = messages[0].ToolCalls();
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_FALSE(calls[0]->IsRawEnvelope());

  const auto entry = ProjectOne(messages[0]);
  ASSERT_TRUE(entry.contains("tool_calls")) << "a client-supplied call keeps the structured shape it arrived in";
  EXPECT_EQ(entry.at("content"), "");
}

TEST(RawEnvelopePipelineTest, ReplayedRawCallPreservesTheModelsDialect) {
  // The internal replay path — and only it — carries provenance on the item, so a rebuilt conversation projects the
  // model's own bytes again.
  std::vector<std::unique_ptr<Item>> items;
  items.push_back(std::make_unique<ToolCallItem>("call_1", kRawTool, kEnvelope, ToolKind::kCustom, Dialect()));

  std::vector<Item*> views;
  for (const auto& item : items) {
    views.push_back(item.get());
  }

  const auto messages = BuildTranscriptMessages(views, {{kRawTool, ToolKind::kCustom}});
  ASSERT_EQ(messages.size(), 1u);

  const auto calls = messages[0].ToolCalls();
  ASSERT_EQ(calls.size(), 1u);
  ASSERT_TRUE(calls[0]->IsRawEnvelope());
  EXPECT_EQ(*calls[0]->raw_encoding, Dialect());

  const auto entry = ProjectOne(messages[0]);
  EXPECT_EQ(entry.at("content"), kEnvelope);
  EXPECT_FALSE(entry.contains("tool_calls"));
}

// ==========================================================================
// Responses storage — provenance survives a cold rebuild, and never reaches the wire.
// ==========================================================================

namespace {

/// The typed Responses response a turn that produced one raw call would publish.
responses::ResponseObject ResponseWithRawCall(const ParsedToolCall& call) {
  auto items = std::vector<std::unique_ptr<Item>>{};
  items.push_back(std::make_unique<ToolCallItem>(call.id, call.name, call.arguments, std::nullopt,
                                                 call.raw_encoding));

  auto session_response = ResponseWithItems(std::move(items));
  auto [output, output_text] = ResponseConverter::FromSessionResponse(session_response,
                                                                      {{call.name, ToolKind::kCustom}});

  responses::ResponseCreateParams params =
      nlohmann::json::parse(R"({"model": "m", "input": "hi"})").get<responses::ResponseCreateParams>();

  return ResponseConverter::BuildResponseObject("resp_1", 0, "m", params, std::move(output), output_text, {});
}

}  // namespace

TEST(RawEnvelopePipelineTest, StoredResponseCarriesTheDialectAndTheWireCopyDoesNot) {
  const auto calls = ReadGeneratedCalls(kEnvelope + "\n");
  ASSERT_EQ(calls.size(), 1u);

  const auto response = ResponseWithRawCall(calls[0]);

  const nlohmann::json wire = response;
  ASSERT_EQ(wire.at("output").size(), 1u);
  EXPECT_EQ(wire.at("output")[0].at("type"), "custom_tool_call");
  EXPECT_FALSE(wire.at("output")[0].contains(responses::kRawEnvelopeReplayKey))
      << "the published Responses schema must be unchanged";

  const auto stored = ResponseConverter::ToStoredJson(response);
  ASSERT_TRUE(stored.at("output")[0].contains(responses::kRawEnvelopeReplayKey));
  EXPECT_EQ(stored.at("output")[0].at(responses::kRawEnvelopeReplayKey).get<RawEnvelopeEncoding>(), Dialect());

  // Everything else is identical — the annotation is additive and store-only.
  EXPECT_EQ(stored.at("output")[0].at("input"), wire.at("output")[0].at("input"));
  EXPECT_EQ(stored.at("output")[0].at("call_id"), wire.at("output")[0].at("call_id"));
}

TEST(RawEnvelopePipelineTest, StructuredCustomCallIsStoredWithoutADialect) {
  auto items = std::vector<std::unique_ptr<Item>>{};
  items.push_back(std::make_unique<ToolCallItem>("call_1", kRawTool, kEnvelope, std::nullopt, std::nullopt));

  auto session_response = ResponseWithItems(std::move(items));
  auto [output, output_text] =
      ResponseConverter::FromSessionResponse(session_response, {{kRawTool, ToolKind::kCustom}});

  responses::ResponseCreateParams params =
      nlohmann::json::parse(R"({"model": "m", "input": "hi"})").get<responses::ResponseCreateParams>();
  const auto response =
      ResponseConverter::BuildResponseObject("resp_1", 0, "m", params, std::move(output), output_text, {});

  const auto stored = ResponseConverter::ToStoredJson(response);
  EXPECT_FALSE(stored.at("output")[0].contains(responses::kRawEnvelopeReplayKey));
}

TEST(RawEnvelopePipelineTest, StoreHandsOutAWireViewWithoutInternalMetadata) {
  const auto calls = ReadGeneratedCalls(kEnvelope + "\n");
  ASSERT_EQ(calls.size(), 1u);

  ResponseStore store;
  store.Store("resp_1", ResponseConverter::ToStoredJson(ResponseWithRawCall(calls[0])), nlohmann::json::array());

  const auto fetched = store.Get("resp_1");
  ASSERT_TRUE(fetched.has_value());
  EXPECT_FALSE(fetched->at("output")[0].contains(responses::kRawEnvelopeReplayKey))
      << "GET /responses/{id} must not expose internal replay bookkeeping";

  const auto page = store.List(10, "");
  ASSERT_EQ(page.data.size(), 1u);
  EXPECT_FALSE(page.data[0].at("output")[0].contains(responses::kRawEnvelopeReplayKey))
      << "the list page must not expose it either";
}

TEST(RawEnvelopePipelineTest, ColdReplayFromStorageRebuildsTheRawCall) {
  // The whole point of storing the dialect: after the warm session is gone, the conversation is rebuilt from stored
  // JSON, and the model must still see the envelope it wrote.
  const auto calls = ReadGeneratedCalls(kEnvelope + "\n");
  ASSERT_EQ(calls.size(), 1u);
  const auto expected_hash = Sha256String(kEnvelope);

  const auto response = ResponseWithRawCall(calls[0]);
  const auto& final_call = std::get<responses::CustomToolCallOutputItem>(response.output.at(0));
  EXPECT_EQ(final_call.call_id, calls[0].id);
  EXPECT_EQ(Sha256String(final_call.input), expected_hash);

  const auto stored = ResponseConverter::ToStoredJson(response);
  EXPECT_EQ(stored.at("output")[0].at("call_id"), calls[0].id);
  EXPECT_EQ(Sha256String(stored.at("output")[0].at("input").get<std::string>()), expected_hash);

  ResponseChainContext chain;
  chain.push_back(ResponseChainHop{nlohmann::json::array(), stored.at("output")});

  responses::ResponseCreateParams params =
      nlohmann::json::parse(R"({"model": "m", "input": "and now?"})").get<responses::ResponseCreateParams>();
  const auto request = ResponseConverter::ToSessionRequest(params, &chain);

  const ToolCallItem* replayed = nullptr;
  for (const auto* item : request.items) {
    if (item->type == FOUNDRY_LOCAL_ITEM_TOOL_CALL) {
      replayed = static_cast<const ToolCallItem*>(item);
    }
  }

  ASSERT_NE(replayed, nullptr);
  EXPECT_EQ(replayed->call_id, calls[0].id);
  EXPECT_EQ(Sha256String(replayed->arguments), expected_hash);
  ASSERT_TRUE(replayed->raw_encoding.has_value());
  EXPECT_EQ(*replayed->raw_encoding, Dialect());

  const auto messages = BuildTranscriptMessages(request.items, {{kRawTool, ToolKind::kCustom}});
  bool found = false;
  for (const auto& message : messages) {
    for (const auto* call : message.ToolCalls()) {
      found = true;
      EXPECT_TRUE(call->IsRawEnvelope());
      EXPECT_EQ(ProjectOne(message).at("content").get<std::string>().find(kEnvelope), 0u);
    }
  }
  EXPECT_TRUE(found);
}

TEST(RawEnvelopePipelineTest, ColdReplayKeepsTheSeparatorBetweenAdjacentRawCalls) {
  const std::string second = "*** Begin Patch\n*** Delete File: src/b.txt\n*** End Patch";
  const auto calls = ReadGeneratedCalls(kEnvelope + "\n" + second);
  ASSERT_EQ(calls.size(), 2u);

  std::vector<std::unique_ptr<Item>> items;
  items.push_back(std::make_unique<ToolCallItem>(calls[0].id, calls[0].name, calls[0].arguments, std::nullopt,
                                                 calls[0].raw_encoding));
  items.push_back(std::make_unique<MessageItem>(FOUNDRY_LOCAL_ROLE_ASSISTANT, "\n"));
  items.push_back(std::make_unique<ToolCallItem>(calls[1].id, calls[1].name, calls[1].arguments, std::nullopt,
                                                 calls[1].raw_encoding));

  auto session_response = ResponseWithItems(std::move(items));
  auto [output, output_text] =
      ResponseConverter::FromSessionResponse(session_response, {{kRawTool, ToolKind::kCustom}});
  responses::ResponseCreateParams hop_params =
      nlohmann::json::parse(R"({"model": "m", "input": "fix both"})").get<responses::ResponseCreateParams>();
  const auto stored = ResponseConverter::ToStoredJson(
      ResponseConverter::BuildResponseObject("resp_1", 0, "m", hop_params, std::move(output), output_text, {}));

  ResponseChainContext chain;
  chain.push_back(ResponseChainHop{nlohmann::json::array(), stored.at("output")});
  responses::ResponseCreateParams next_params =
      nlohmann::json::parse(R"({"model": "m", "input": "continue"})").get<responses::ResponseCreateParams>();
  const auto request = ResponseConverter::ToSessionRequest(next_params, &chain);
  const auto messages = BuildTranscriptMessages(request.items, {{kRawTool, ToolKind::kCustom}});

  ASSERT_FALSE(messages.empty());
  EXPECT_EQ(ProjectOne(messages.front()).at("content"), kEnvelope + "\n" + second);
}

TEST(RawEnvelopePipelineTest, ColdReplayOfAWireCopyStaysStructured) {
  // A client that fetched a response and echoed its items back has no dialect to offer — the wire copy never carried
  // one — so the call replays as the structured call it looks like. Provenance cannot be forged from outside.
  const auto calls = ReadGeneratedCalls(kEnvelope + "\n");
  ASSERT_EQ(calls.size(), 1u);

  const nlohmann::json wire = ResponseWithRawCall(calls[0]);

  ResponseChainContext chain;
  chain.push_back(ResponseChainHop{nlohmann::json::array(), wire.at("output")});

  responses::ResponseCreateParams params =
      nlohmann::json::parse(R"({"model": "m", "input": "and now?"})").get<responses::ResponseCreateParams>();
  const auto request = ResponseConverter::ToSessionRequest(params, &chain);

  for (const auto* item : request.items) {
    if (item->type == FOUNDRY_LOCAL_ITEM_TOOL_CALL) {
      EXPECT_FALSE(static_cast<const ToolCallItem*>(item)->raw_encoding.has_value());
    }
  }
}

// ==========================================================================
// The dialect is configuration — the same path runs under any of them.
// ==========================================================================

TEST(RawEnvelopePipelineTest, ArbitraryDialectTravelsTheWholePath) {
  ToolCallContext context;
  context.tool_call_start = "<tool_call>";
  context.tool_call_end = "</tool_call>";
  context.tool_output = true;
  context.tool_kinds = {{"edit_file", ToolKind::kCustom}};
  context.tool_output_encoding = RawEnvelopeEncoding{"edit_file", "<<<<<< EDIT", ">>>>>> DONE"};
  context.forced_tool = ForcedToolChoice{"edit_file", ToolKind::kCustom};

  const std::string envelope = "<<<<<< EDIT\nsrc/a.txt\n-old\n+new\n>>>>>> DONE";
  std::string visible;
  const auto calls = ReadGeneratedCalls("Doing it:\n" + envelope + "\nDone.", &visible, context);

  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].name, "edit_file") << "the configured name is what is emitted";
  EXPECT_EQ(calls[0].arguments, envelope);
  EXPECT_EQ(visible, "Doing it:\n\nDone.");

  // …and it survives commit, storage and projection exactly as the built-in-looking dialect does.
  const auto response = [&] {
    auto items = std::vector<std::unique_ptr<Item>>{};
    items.push_back(std::make_unique<ToolCallItem>(calls[0].id, calls[0].name, calls[0].arguments, std::nullopt,
                                                   calls[0].raw_encoding));
    auto session_response = ResponseWithItems(std::move(items));
    auto [output, output_text] =
        ResponseConverter::FromSessionResponse(session_response, {{"edit_file", ToolKind::kCustom}});
    responses::ResponseCreateParams params =
        nlohmann::json::parse(R"({"model": "m", "input": "hi"})").get<responses::ResponseCreateParams>();
    return ResponseConverter::BuildResponseObject("resp_1", 0, "m", params, std::move(output), output_text, {});
  }();

  const auto stored = ResponseConverter::ToStoredJson(response);
  ASSERT_TRUE(stored.at("output")[0].contains(responses::kRawEnvelopeReplayKey));
  EXPECT_EQ(stored.at("output")[0].at(responses::kRawEnvelopeReplayKey).at("tool_name"), "edit_file");

  const auto projected = ProjectOne(AssistantTurn("Doing it:\n", calls[0], ToolKind::kCustom));
  EXPECT_EQ(projected.at("content"), "Doing it:\n" + envelope);
}

TEST(RawEnvelopePipelineTest, ChatCompletionsRecordsTheForcedChoiceBeforeNarrowing) {
  // The Chat Completions surface must carry the caller's explicit choice too, or the same request would enable the
  // dialect on Responses and not here.
  auto request_json = nlohmann::json::parse(R"({
    "model": "m",
    "messages": [{"role": "user", "content": "fix it"}],
    "tools": [{"type": "custom", "custom": {"name": "apply_patch"}},
              {"type": "function", "function": {"name": "bash", "parameters": {"type": "object"}}}],
    "tool_choice": {"type": "custom", "custom": {"name": "apply_patch"}}
  })");

  const auto request = request_json.get<ChatCompletionRequest>();

  Request session_request;
  const auto definitions = chat_completions::ExtractToolDefinitions(request, session_request);

  const auto forced = tools::ReadForcedToolChoice(session_request);
  ASSERT_TRUE(forced.has_value());
  EXPECT_EQ(forced->name, "apply_patch");
  EXPECT_EQ(forced->kind, ToolKind::kCustom);

  ASSERT_EQ(definitions.size(), 1u) << "narrowing still happens, and the choice survived it";
  EXPECT_EQ(definitions[0].name, "apply_patch");

  ToolCallContext context;
  context.tool_output = true;
  context.tool_kinds = tools::KindsByName(definitions);
  context.tool_output_encoding = Dialect();
  context.forced_tool = forced;
  EXPECT_NE(context.ActiveRawEnvelope(), nullptr);
}

TEST(RawEnvelopePipelineTest, ChatCompletionsAutoRecordsNoForcedChoice) {
  auto request_json = nlohmann::json::parse(R"({
    "model": "m",
    "messages": [{"role": "user", "content": "fix it"}],
    "tools": [{"type": "custom", "custom": {"name": "apply_patch"}}],
    "tool_choice": "auto"
  })");

  const auto request = request_json.get<ChatCompletionRequest>();

  Request session_request;
  const auto definitions = chat_completions::ExtractToolDefinitions(request, session_request);

  EXPECT_FALSE(tools::ReadForcedToolChoice(session_request).has_value());

  ToolCallContext context;
  context.tool_output = true;
  context.tool_kinds = tools::KindsByName(definitions);
  context.tool_output_encoding = Dialect();
  context.forced_tool = tools::ReadForcedToolChoice(session_request);
  EXPECT_EQ(context.ActiveRawEnvelope(), nullptr) << "offering the tool is not instructing the model to call it";
}

TEST(RawEnvelopePipelineTest, LongColdChainReplaysEveryRawCallInItsOwnDialect) {
  ToolCallContext other_dialect;
  other_dialect.tool_output = true;
  other_dialect.tool_kinds = {{"edit_file", ToolKind::kCustom}};
  other_dialect.tool_output_encoding = RawEnvelopeEncoding{"edit_file", "@@BEGIN@@", "@@END@@"};
  other_dialect.forced_tool = ForcedToolChoice{"edit_file", ToolKind::kCustom};

  ResponseChainContext chain;
  std::vector<ParsedToolCall> expected_calls;
  std::vector<std::string> expected_envelopes;

  for (int index = 0; index < 8; ++index) {
    const bool alternate = index % 2 != 0;
    const auto envelope = alternate ? "@@BEGIN@@\nchange " + std::to_string(index) + "\n@@END@@"
                                    : "*** Begin Patch\nchange " + std::to_string(index) + "\n*** End Patch";
    const auto parsed =
        ReadGeneratedCalls(envelope + "\n", nullptr, alternate ? other_dialect : RawEnvelopeTurn());
    ASSERT_EQ(parsed.size(), 1u);

    nlohmann::json input = nlohmann::json::array();
    if (!expected_calls.empty()) {
      input.push_back({{"type", "custom_tool_call_output"},
                       {"call_id", expected_calls.back().id},
                       {"output", "applied " + std::to_string(index - 1)}});
    }

    chain.push_back(ResponseChainHop{std::move(input),
                                     ResponseConverter::ToStoredJson(ResponseWithRawCall(parsed[0])).at("output")});
    expected_calls.push_back(parsed[0]);
    expected_envelopes.push_back(envelope);
  }

  responses::ResponseCreateParams params =
      nlohmann::json::parse(R"({"model": "m", "input": "and now?"})").get<responses::ResponseCreateParams>();
  const auto request = ResponseConverter::ToSessionRequest(params, &chain);

  std::vector<const ToolCallItem*> replayed;
  for (const auto* item : request.items) {
    if (item->type == FOUNDRY_LOCAL_ITEM_TOOL_CALL) {
      replayed.push_back(static_cast<const ToolCallItem*>(item));
    }
  }

  ASSERT_EQ(replayed.size(), expected_calls.size());
  for (size_t index = 0; index < replayed.size(); ++index) {
    ASSERT_TRUE(replayed[index]->raw_encoding.has_value());
    EXPECT_EQ(replayed[index]->call_id, expected_calls[index].id);
    EXPECT_EQ(replayed[index]->arguments, expected_envelopes[index]);
    EXPECT_EQ(replayed[index]->raw_encoding->tool_name, index % 2 == 0 ? kRawTool : "edit_file");
  }

  const auto messages =
      BuildTranscriptMessages(request.items, {{kRawTool, ToolKind::kCustom}, {"edit_file", ToolKind::kCustom}});
  size_t assistant_index = 0;
  size_t result_index = 0;
  for (const auto& message : messages) {
    if (message.role == FOUNDRY_LOCAL_ROLE_ASSISTANT) {
      ASSERT_LT(assistant_index, expected_envelopes.size());
      EXPECT_EQ(ProjectOne(message).at("content"), expected_envelopes[assistant_index++]);
    } else if (message.role == FOUNDRY_LOCAL_ROLE_TOOL) {
      ASSERT_LT(result_index, expected_calls.size() - 1);
      EXPECT_EQ(message.tool_call_id, expected_calls[result_index].id);
      EXPECT_EQ(message.VisibleText(), "applied " + std::to_string(result_index++));
    }
  }

  EXPECT_EQ(assistant_index, expected_envelopes.size());
  EXPECT_EQ(result_index, expected_calls.size() - 1);
}

TEST(RawEnvelopePipelineTest, StreamedItemCarriesTheDialectSoAStreamedTurnStoresIt) {
  // The Responses streaming path never sees the session's final items: it builds the completed response from the
  // items pushed to the streaming callback. A streamed item that dropped its dialect would store a response that
  // cold-replays as a structured call the model never wrote — invisible on the non-streaming path.
  const auto calls = ReadGeneratedCalls(kEnvelope + "\n");
  ASSERT_EQ(calls.size(), 1u);

  // The item ChatSession pushes to the streaming callback.
  ToolCallItem streamed(calls[0].id, calls[0].name, calls[0].arguments, std::nullopt, calls[0].raw_encoding);
  ASSERT_TRUE(streamed.raw_encoding.has_value());

  // …becomes the completed item the handler collects into closed_items.
  int sequence_number = 0;
  auto output = ResponseConverter::BuildToolCallStreamOutput(streamed, ToolKind::kCustom, 0, sequence_number);

  responses::ResponseCreateParams params =
      nlohmann::json::parse(R"({"model": "m", "input": "hi"})").get<responses::ResponseCreateParams>();
  std::vector<responses::ResponseOutputItem> closed_items;
  closed_items.push_back(std::move(output.completed_item));

  const auto response =
      ResponseConverter::BuildResponseObject("resp_1", 0, "m", params, std::move(closed_items), "", {});

  const auto stored = ResponseConverter::ToStoredJson(response);
  ASSERT_EQ(stored.at("output").size(), 1u);
  ASSERT_TRUE(stored.at("output")[0].contains(responses::kRawEnvelopeReplayKey))
      << "a streamed turn must store the dialect just as a non-streamed one does";
  EXPECT_EQ(stored.at("output")[0].at(responses::kRawEnvelopeReplayKey).get<RawEnvelopeEncoding>(), Dialect());

  // And the streamed events themselves stay on the standard schema.
  for (const auto& event : output.events) {
    const nlohmann::json event_json = event;
    EXPECT_EQ(event_json.dump().find(responses::kRawEnvelopeReplayKey), std::string::npos)
        << "streamed events must not carry internal replay metadata";
  }
}

TEST(RawEnvelopePipelineTest, StoredAnnotationTracksTheRightItemInAMixedOutput) {
  // ToStoredJson matches typed items to serialized ones positionally, so a response whose raw call is not the first
  // output item would expose any drift between the two orderings.
  const auto raw = ReadGeneratedCalls("Here goes:\n" + kEnvelope + "\n");
  ASSERT_EQ(raw.size(), 1u);

  auto items = std::vector<std::unique_ptr<Item>>{};
  items.push_back(std::make_unique<MessageItem>(FOUNDRY_LOCAL_ROLE_ASSISTANT, "Here goes:\n"));
  items.push_back(std::make_unique<ToolCallItem>(raw[0].id, raw[0].name, raw[0].arguments, std::nullopt,
                                                 raw[0].raw_encoding));
  items.push_back(std::make_unique<ToolCallItem>("call_fn", "bash", R"({"cmd":"ls"})", std::nullopt, std::nullopt));

  auto session_response = ResponseWithItems(std::move(items));
  auto [output, output_text] = ResponseConverter::FromSessionResponse(
      session_response, {{kRawTool, ToolKind::kCustom}, {"bash", ToolKind::kFunction}});

  responses::ResponseCreateParams params =
      nlohmann::json::parse(R"({"model": "m", "input": "hi"})").get<responses::ResponseCreateParams>();
  const auto response =
      ResponseConverter::BuildResponseObject("resp_1", 0, "m", params, std::move(output), output_text, {});

  const nlohmann::json wire = response;
  const auto stored = ResponseConverter::ToStoredJson(response);

  // `output` serializes 1:1 with the typed vector — the assumption ToStoredJson relies on.
  ASSERT_EQ(wire.at("output").size(), response.output.size());
  ASSERT_EQ(stored.at("output").size(), wire.at("output").size());
  ASSERT_EQ(stored.at("output").size(), 3u);

  EXPECT_EQ(stored.at("output")[0].at("type"), "message");
  EXPECT_FALSE(stored.at("output")[0].contains(responses::kRawEnvelopeReplayKey));

  ASSERT_EQ(stored.at("output")[1].at("type"), "custom_tool_call");
  ASSERT_TRUE(stored.at("output")[1].contains(responses::kRawEnvelopeReplayKey))
      << "the annotation must land on the raw call, not on its neighbour";
  EXPECT_EQ(stored.at("output")[1].at("input"), kEnvelope);

  EXPECT_EQ(stored.at("output")[2].at("type"), "function_call");
  EXPECT_FALSE(stored.at("output")[2].contains(responses::kRawEnvelopeReplayKey));
}

TEST(RawEnvelopePipelineTest, ToolCallsToItemsKeepsTheDialect) {
  // The shared arbiter-output-to-items helper is used by non-streaming callers; it must not drop provenance either.
  const auto calls = ReadGeneratedCalls(kEnvelope + "\n");
  ASSERT_EQ(calls.size(), 1u);

  const auto items = ToolCallsToItems(calls);
  ASSERT_EQ(items.size(), 1u);

  const auto& item = static_cast<const ToolCallItem&>(*items[0]);
  ASSERT_TRUE(item.raw_encoding.has_value());
  EXPECT_EQ(*item.raw_encoding, Dialect());
}
