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
#include "inferencing/generative/chat/chat_transcript.h"
#include "inferencing/generative/openresponses/response_converter.h"
#include "items/message_item.h"
#include "items/text_item.h"
#include "items/tool_call_item.h"
#include "items/tool_result_item.h"
#include "util/sha256.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace fl;
using json = nlohmann::json;

namespace {

const std::string kPatch = "*** Begin Patch\n*** Update File: src/a.txt\n@@\n-old\t\n+new  \n*** End Patch";

/// A turn that offered `apply_patch` as a custom tool. The kinds are what make a bare envelope readable as a call
/// and what make the committed call report as `custom` rather than `function` on the wire.
ToolCallContext PatchTurn() {
  ToolCallContext context;
  context.tool_call_start = "<tool_call>";
  context.tool_call_end = "</tool_call>";
  context.tool_output = true;
  context.tool_kinds = {{"apply_patch", ToolKind::kCustom}, {"bash", ToolKind::kFunction}};
  return context;
}

/// Read one turn's generated output the way both chat paths do — one byte at a time, then drained.
std::vector<ParsedToolCall> ReadGeneratedCalls(const std::string& generated, std::string* visible = nullptr) {
  GeneratedOutputArbiter arbiter(PatchTurn());
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

TEST(RawApplyPatchPipelineTest, CommittedCallKeepsRawBytesAndRendersAsCustomInput) {
  const auto calls = ReadGeneratedCalls("Applying:\n" + kPatch + "\n");
  ASSERT_EQ(calls.size(), 1u);

  const auto generated = MakeGeneratedToolCall(calls[0].id, calls[0].name, calls[0].arguments,
                                               PatchTurn().KindOf(calls[0].name));

  EXPECT_TRUE(generated.arguments_usable) << "a custom payload is text and can never be malformed";
  EXPECT_EQ(generated.call.kind, ToolKind::kCustom);
  EXPECT_EQ(generated.call.arguments, kPatch) << "the authoritative record keeps the bytes that were generated";
  EXPECT_EQ(generated.call.normalized_arguments, json::object({{"input", kPatch}}))
      << "template projection re-wraps the payload in the shape the model was prompted with";
}

TEST(RawApplyPatchPipelineTest, TurnCommitsTheCallUnderTheIdTheEnvelopeMinted) {
  const auto calls = ReadGeneratedCalls(kPatch);
  ASSERT_EQ(calls.size(), 1u);

  TranscriptMessage assistant;
  assistant.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;
  assistant.AppendToolCall(
      MakeGeneratedToolCall(calls[0].id, calls[0].name, calls[0].arguments, ToolKind::kCustom).call);

  ChatTranscript transcript;
  transcript.ValidateGeneratedOutput(assistant);
  transcript.CommitTurn({TranscriptMessage(FOUNDRY_LOCAL_ROLE_USER, "fix it")}, assistant, {});

  EXPECT_TRUE(transcript.IsOutstanding(calls[0].id));
  EXPECT_EQ(transcript.OutstandingCallCount(), 1u);

  const auto committed = transcript.Messages().back().ToolCalls();
  ASSERT_EQ(committed.size(), 1u);
  EXPECT_EQ(committed[0]->call_id, calls[0].id);
  EXPECT_EQ(committed[0]->arguments, kPatch);
}

TEST(RawApplyPatchPipelineTest, SequentialEnvelopesCommitAsDistinctOutstandingCalls) {
  const std::string first = "*** Begin Patch\n*** Add File: a.txt\n+a\n*** End Patch";
  const std::string second = "*** Begin Patch\n*** Delete File: b.txt\n*** End Patch";

  const auto calls = ReadGeneratedCalls(first + "\nand then\n" + second + "\n");
  ASSERT_EQ(calls.size(), 2u);
  ASSERT_NE(calls[0].id, calls[1].id);

  TranscriptMessage assistant;
  assistant.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;
  for (const auto& call : calls) {
    assistant.AppendToolCall(MakeGeneratedToolCall(call.id, call.name, call.arguments, ToolKind::kCustom).call);
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

TEST(RawApplyPatchPipelineTest, FailedResultAndRetryCorrelateOldAndNewCallIds) {
  const auto first_attempt = ReadGeneratedCalls(kPatch);
  ASSERT_EQ(first_attempt.size(), 1u);

  ChatTranscript transcript;

  TranscriptMessage assistant;
  assistant.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;
  assistant.AppendToolCall(
      MakeGeneratedToolCall(first_attempt[0].id, first_attempt[0].name, first_attempt[0].arguments,
                            ToolKind::kCustom)
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
      MakeGeneratedToolCall(retry[0].id, retry[0].name, retry[0].arguments, ToolKind::kCustom).call);

  transcript.ValidateGeneratedOutput(retry_assistant);
  transcript.CommitTurn(std::move(retry_inputs), retry_assistant, {});

  EXPECT_FALSE(transcript.IsOutstanding(first_attempt[0].id)) << "the failed call was answered";
  EXPECT_TRUE(transcript.IsOutstanding(retry[0].id)) << "the retry is the call now awaiting a result";
  EXPECT_EQ(transcript.OutstandingCallCount(), 1u);

  // Both attempts, and the failure between them, survive in order so the next prompt can show the model why the
  // first patch was rejected.
  const auto& messages = transcript.Messages();
  ASSERT_EQ(messages.size(), 4u);
  EXPECT_EQ(messages[1].ToolCalls().at(0)->arguments, kPatch);
  EXPECT_EQ(messages[2].role, FOUNDRY_LOCAL_ROLE_TOOL);
  EXPECT_EQ(messages[2].tool_call_id, first_attempt[0].id);
  EXPECT_EQ(messages[2].VisibleText(), failure);
  EXPECT_EQ(messages[3].ToolCalls().at(0)->arguments, corrected);
}

TEST(RawApplyPatchPipelineTest, ReplayedCallFromAPriorTurnNormalizesLikeTheGeneratedOne) {
  const auto calls = ReadGeneratedCalls(kPatch);
  ASSERT_EQ(calls.size(), 1u);

  ToolCallItem replayed(calls[0].id, "apply_patch", kPatch);
  std::vector<Item*> items{&replayed};

  const auto messages = BuildTranscriptMessages(items, PatchTurn().tool_kinds);
  ASSERT_EQ(messages.size(), 1u);

  const auto replayed_calls = messages[0].ToolCalls();
  ASSERT_EQ(replayed_calls.size(), 1u);
  EXPECT_EQ(replayed_calls[0]->call_id, calls[0].id);
  EXPECT_EQ(replayed_calls[0]->arguments, kPatch);
  EXPECT_EQ(replayed_calls[0]->normalized_arguments, json::object({{"input", kPatch}}));
}

// ==========================================================================
// Chat Completions surface.
// ==========================================================================

TEST(RawApplyPatchPipelineTest, ChatCompletionsReportsTheEnvelopeAsACustomCall) {
  const auto calls = ReadGeneratedCalls(kPatch + "\n");
  ASSERT_EQ(calls.size(), 1u);

  const auto call = chat_completions::MakeToolCall(calls[0].id, calls[0].name, calls[0].arguments,
                                                   PatchTurn().KindOf(calls[0].name));
  const json serialized = call;

  ASSERT_TRUE(call.custom.has_value());
  EXPECT_EQ(serialized.at("type"), "custom");
  EXPECT_EQ(serialized.at("custom").at("name"), "apply_patch");
  EXPECT_EQ(serialized.at("custom").at("input"), kPatch);
  EXPECT_FALSE(serialized.contains("function")) << "a custom payload is never reported as JSON arguments";
}

TEST(RawApplyPatchPipelineTest, ChatCompletionsStreamedAndFinalCallsShareOneId) {
  const auto calls = ReadGeneratedCalls(kPatch + "\n");
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
      chat_completions::BuildResponse(response, "chatcmpl-1", 42, "test-model", PatchTurn().tool_kinds);
  const auto& final_call = final_response.at("choices").at(0).at("message").at("tool_calls").at(0);
  const auto& streamed_call = chunk.at("choices").at(0).at("delta").at("tool_calls").at(0);

  EXPECT_EQ(streamed_call.at("id"), final_call.at("id"));
  EXPECT_EQ(streamed_call.at("custom").at("input"), kPatch);
  EXPECT_EQ(final_call.at("custom").at("input"), kPatch);
  EXPECT_EQ(final_response.at("choices").at(0).at("finish_reason"), "tool_calls");
}

// ==========================================================================
// Responses surface.
// ==========================================================================

TEST(RawApplyPatchPipelineTest, ResponsesStreamsTheEnvelopeAsCustomToolCallInput) {
  const auto calls = ReadGeneratedCalls(kPatch + "\n");
  ASSERT_EQ(calls.size(), 1u);

  ToolCallItem call(calls[0].id, calls[0].name, calls[0].arguments);
  int sequence_number = 0;
  const auto streamed = ResponseConverter::BuildToolCallStreamOutput(call, ToolKind::kCustom, 0, sequence_number);

  ASSERT_EQ(streamed.events.size(), 4u);

  const auto& completed = std::get<responses::CustomToolCallOutputItem>(streamed.completed_item);
  EXPECT_EQ(completed.call_id, calls[0].id);
  EXPECT_EQ(completed.name, "apply_patch");
  EXPECT_EQ(completed.input, kPatch);

  const json delta = streamed.events[1];
  EXPECT_EQ(delta.at("type"), "response.custom_tool_call_input.delta");
  EXPECT_EQ(delta.at("delta"), kPatch);
  EXPECT_EQ(delta.at("call_id"), calls[0].id);

  const json done = streamed.events[2];
  EXPECT_EQ(done.at("input"), kPatch);
  EXPECT_FALSE(done.contains("arguments"));
}

TEST(RawApplyPatchPipelineTest, ResponsesStreamedAndFinalItemsAgreeOnIdAndPayload) {
  const auto calls = ReadGeneratedCalls(kPatch + "\n");
  ASSERT_EQ(calls.size(), 1u);

  ToolCallItem call(calls[0].id, calls[0].name, calls[0].arguments);
  int sequence_number = 0;
  const auto streamed = ResponseConverter::BuildToolCallStreamOutput(call, ToolKind::kCustom, 0, sequence_number);

  std::vector<std::unique_ptr<Item>> items;
  items.push_back(std::make_unique<ToolCallItem>(calls[0].id, calls[0].name, calls[0].arguments));
  const auto [output, output_text] =
      ResponseConverter::FromSessionResponse(ResponseWithItems(std::move(items)), PatchTurn().tool_kinds);

  ASSERT_EQ(output.size(), 1u);
  const auto& streamed_item = std::get<responses::CustomToolCallOutputItem>(streamed.completed_item);
  const auto& final_item = std::get<responses::CustomToolCallOutputItem>(output.at(0));

  EXPECT_EQ(streamed_item.call_id, calls[0].id);
  EXPECT_EQ(streamed_item.call_id, final_item.call_id);
  EXPECT_EQ(streamed_item.input, final_item.input);
  EXPECT_EQ(final_item.input, kPatch);
  EXPECT_TRUE(output_text.empty()) << "a call is not assistant text";
}

// ==========================================================================
// Byte fidelity across the whole path.
// ==========================================================================

TEST(RawApplyPatchPipelineTest, PayloadHashIsIdenticalAtEveryStageOfThePath) {
  const std::string expected_hash = Sha256String(kPatch);

  const auto calls = ReadGeneratedCalls("prefix\n" + kPatch + "\nsuffix");
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(Sha256String(calls[0].arguments), expected_hash);

  const auto committed = MakeGeneratedToolCall(calls[0].id, calls[0].name, calls[0].arguments, ToolKind::kCustom);
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

TEST(RawApplyPatchPipelineTest, VisibleTextAroundAnEnvelopeIsPreservedInOrder) {
  std::string visible;
  const auto calls = ReadGeneratedCalls("Here is the fix:\n" + kPatch + "\nLet me know if it works.", &visible);

  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(visible, "Here is the fix:\n\nLet me know if it works.");
  EXPECT_EQ(calls[0].arguments, kPatch);
}
