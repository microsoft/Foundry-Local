// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "items/item.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace fl {

/// Kind of tool invocation recorded in the transcript. Function calls are the only kind produced today; the enum
/// keeps the internal representation extensible without widening the public ABI.
enum class ToolCallKind {
  kFunction,
};

/// A single tool invocation, carried through the transcript exactly as it was generated or supplied.
struct TranscriptToolCall {
  std::string call_id;
  std::string name;
  /// Raw argument bytes exactly as generated or supplied. Never rewritten, so the authoritative record and the
  /// response always report what actually happened.
  std::string arguments;
  /// Object form used for template projection. Always a JSON object, so projecting a committed transcript can never
  /// fail. For caller-supplied calls this is the parsed `arguments`; for model output that failed to produce an
  /// object it is an empty object (the raw bytes are still preserved above).
  nlohmann::ordered_json normalized_arguments = nlohmann::ordered_json::object();
  ToolCallKind kind = ToolCallKind::kFunction;
};

/// One event within a message, stored in the order it occurred.
struct TranscriptEntry {
  enum class Kind {
    kText,       // visible assistant/user text
    kReasoning,  // chain-of-thought text
    kToolCall,   // a tool invocation
  };

  Kind kind = Kind::kText;
  std::string text;
  TranscriptToolCall tool_call;

  static TranscriptEntry Text(std::string value) {
    return {Kind::kText, std::move(value), {}};
  }

  static TranscriptEntry Reasoning(std::string value) {
    return {Kind::kReasoning, std::move(value), {}};
  }

  static TranscriptEntry ToolCall(TranscriptToolCall call) {
    return {Kind::kToolCall, {}, std::move(call)};
  }
};

/// A message in the authoritative transcript.
///
/// Unlike MessageItem (the API-facing type) a transcript message keeps visible text, reasoning, and tool calls as
/// separate ordered events rather than folding everything into modality content parts. Media parts are intentionally
/// absent: media input is single-shot and is fed to the generator directly from the request items.
struct TranscriptMessage {
  flMessageRole role = FOUNDRY_LOCAL_ROLE_NONE;
  std::vector<TranscriptEntry> entries;
  std::string name;  // optional participant name
  /// Set only for role == FOUNDRY_LOCAL_ROLE_TOOL: the assistant call this message answers.
  std::string tool_call_id;

  TranscriptMessage() = default;

  /// Convenience for the common single-text message. An empty `text` produces a message with no entries, which
  /// projects to empty template content — used for assistant turns whose output was entirely hidden reasoning.
  TranscriptMessage(flMessageRole role_in, std::string text, std::string name_in = {});

  /// Build a role="tool" message. An empty `result` is valid and is preserved as empty content.
  static TranscriptMessage ToolResult(std::string call_id, std::string result);

  /// Append text, merging into a trailing entry of the same kind. Empty strings are ignored.
  void AppendText(std::string value);
  void AppendReasoning(std::string value);
  void AppendToolCall(TranscriptToolCall call);

  bool HasToolCalls() const;

  /// Concatenated visible text across all kText entries.
  std::string VisibleText() const;

  /// Concatenated reasoning text across all kReasoning entries.
  std::string ReasoningText() const;

  /// Tool calls in event order.
  std::vector<const TranscriptToolCall*> ToolCalls() const;
};

/// Parse raw function-call argument bytes into the object form used for template projection.
/// Absent arguments mean "no arguments" and yield an empty object. Returns nullopt when bytes exist but are not a
/// JSON object — the caller decides whether that is a client error or a model defect.
std::optional<nlohmann::ordered_json> ParseToolCallArguments(const std::string& arguments);

/// Build a function tool call supplied by the caller. Arguments are validated strictly: a client that replays a call
/// with bytes that are not a JSON object gets an explicit error rather than a silently altered conversation.
///
/// @throws fl::Exception FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT when the arguments are not a JSON object.
TranscriptToolCall MakeSuppliedToolCall(std::string call_id, std::string name, std::string arguments);

/// Result of admitting a model-generated tool call into the transcript.
struct GeneratedToolCall {
  TranscriptToolCall call;
  /// False when the model's raw argument bytes were not a JSON object. The raw bytes are preserved on `call` and the
  /// normalized form falls back to an empty object; the caller is expected to report the model defect.
  bool arguments_usable = true;
};

/// Build a function tool call from model output.
///
/// Generation has already been streamed to the caller by the time a turn is committed, so a model that emits
/// unusable argument bytes must not fail the request. The raw bytes are preserved verbatim and the normalized form
/// degrades to an empty object, which keeps the committed transcript renderable on every later turn.
GeneratedToolCall MakeGeneratedToolCall(std::string call_id, std::string name, std::string arguments);

/// Convert request items into transcript messages, preserving the order in which they were supplied.
///
/// - MESSAGE items become role-tagged messages; typed TextItem parts keep their visible / reasoning distinction and
///   non-text parts are skipped (media is handled separately by the caller). A message with neither content nor a
///   participant name carries nothing and is dropped; a content-free named message survives to attribute the tool
///   calls that follow it.
/// - TOOL_CALL items are folded into a directly preceding assistant message so replayed assistant content and its
///   calls stay together, matching how the model produced them.
/// - TOOL_RESULT items become role="tool" messages carrying the call ID. An empty result string is preserved.
std::vector<TranscriptMessage> BuildTranscriptMessages(const std::vector<Item*>& items);

/// Authoritative ordered record of a conversation.
///
/// The transcript owns two things ChatSession must not duplicate: the committed message order (including tool calls
/// and results) and the outstanding-call bookkeeping used to validate correlation. Turns are committed atomically,
/// so a failed, cancelled, or rejected turn leaves no partial state behind, and undo restores the exact prior state.
class ChatTranscript {
 public:
  /// Generator sequence lengths bracketing a turn. Used to rewind the cached generator on undo.
  struct TurnTokens {
    /// Sequence length before this turn's input was appended. Empty when the turn built a fresh generator: the
    /// turn's input is baked into that generator's prompt, so there is no boundary to rewind back to and the
    /// caller must drop the generator instead.
    std::optional<int> pre_turn;
    int post_turn = 0;
  };

  struct Turn {
    size_t message_start = 0;  // index of this turn's first input message
    TurnTokens tokens;
  };

  const std::vector<TranscriptMessage>& Messages() const { return messages_; }
  const std::vector<Turn>& Turns() const { return turns_; }

  size_t MessageCount() const { return messages_.size(); }
  size_t TurnCount() const { return turns_.size(); }
  bool Empty() const { return messages_.empty(); }

  bool IsOutstanding(const std::string& call_id) const { return outstanding_.count(call_id) != 0; }
  size_t OutstandingCallCount() const { return outstanding_.size(); }
  bool HasOutstandingCalls() const { return !outstanding_.empty(); }

  /// Validate a turn's input batch without mutating anything. The batch is walked in order so a replayed assistant
  /// tool call can be answered by a result later in the same batch.
  ///
  /// @throws fl::Exception FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT for a missing, unknown, duplicate, or already
  ///         answered call ID.
  void ValidateInputs(const std::vector<TranscriptMessage>& inputs) const;

  /// Validate a generated assistant message: every tool call needs a non-empty ID that is unique within the message,
  /// does not collide with an already-issued call, and carries usable arguments.
  ///
  /// @throws fl::Exception FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT
  void ValidateGeneratedOutput(const TranscriptMessage& output) const;

  /// Commit one turn atomically: the input messages followed by the assistant reply. Both are validated first, so a
  /// rejected turn leaves the transcript untouched.
  void CommitTurn(std::vector<TranscriptMessage> inputs, TranscriptMessage output, TurnTokens tokens);

  /// Remove the last `count` turns and restore outstanding-call state to exactly what it was before them.
  ///
  /// Returns the token bracket the caller should rewind its generator to, with no `pre_turn` when the removed range
  /// crosses a turn that rebuilt the generator: the current KV cache's token offsets were established by that
  /// rebuild, so every earlier boundary is stale and the caller must drop the generator instead.
  ///
  /// @throws fl::Exception FOUNDRY_LOCAL_ERROR_INVALID_USAGE if `count` exceeds TurnCount().
  TurnTokens UndoTurns(size_t count);

 private:
  /// Recompute issued / outstanding call IDs from the committed messages. Called after truncation so undo never has
  /// to reason about incremental bookkeeping.
  void RebuildCallState();

  void RecordMessageCalls(const TranscriptMessage& message);

  std::vector<TranscriptMessage> messages_;
  std::vector<Turn> turns_;

  // Every call ID ever issued by a committed assistant message, and the subset still awaiting a result. Both are
  // derived state: they exist so validation is O(1) and are rebuilt wholesale on undo.
  std::unordered_set<std::string> issued_;
  std::unordered_set<std::string> outstanding_;
};

}  // namespace fl
