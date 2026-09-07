// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "inferencing/generative/chat/chat_transcript.h"

#include "exception.h"
#include "items/message_item.h"
#include "items/text_item.h"
#include "items/tool_call_item.h"
#include "items/tool_result_item.h"

#include <optional>
#include <utility>

namespace fl {

namespace {

/// Concatenate the text of every entry of `kind`. Parts are literal fragments — the producer owns any separators.
std::string JoinEntries(const std::vector<TranscriptEntry>& entries, TranscriptEntry::Kind kind) {
  std::string text;
  for (const auto& entry : entries) {
    if (entry.kind == kind) {
      text += entry.text;
    }
  }

  return text;
}

}  // namespace

// ---------------------------------------------------------------------------
// Tool call arguments
// ---------------------------------------------------------------------------

std::optional<nlohmann::ordered_json> ParseToolCallArguments(const std::string& arguments) {
  if (arguments.empty()) {
    return nlohmann::ordered_json::object();
  }

  auto parsed = nlohmann::ordered_json::parse(arguments, nullptr, /*allow_exceptions=*/false);
  if (!parsed.is_object()) {
    return std::nullopt;
  }

  return parsed;
}

TranscriptToolCall MakeSuppliedToolCall(std::string call_id, std::string name, std::string arguments) {
  auto normalized = ParseToolCallArguments(arguments);
  if (!normalized.has_value()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT,
             "tool call '" + name + "' has arguments that are not a JSON object: " + arguments);
  }

  return {std::move(call_id), std::move(name), std::move(arguments), std::move(*normalized),
          ToolCallKind::kFunction};
}

GeneratedToolCall MakeGeneratedToolCall(std::string call_id, std::string name, std::string arguments) {
  auto normalized = ParseToolCallArguments(arguments);
  const bool usable = normalized.has_value();

  return {{std::move(call_id), std::move(name), std::move(arguments),
           usable ? std::move(*normalized) : nlohmann::ordered_json::object(), ToolCallKind::kFunction},
          usable};
}

// ---------------------------------------------------------------------------
// TranscriptMessage
// ---------------------------------------------------------------------------

TranscriptMessage::TranscriptMessage(flMessageRole role_in, std::string text, std::string name_in)
    : role(role_in), name(std::move(name_in)) {
  AppendText(std::move(text));
}

TranscriptMessage TranscriptMessage::ToolResult(std::string call_id, std::string result) {
  TranscriptMessage message;
  message.role = FOUNDRY_LOCAL_ROLE_TOOL;
  message.tool_call_id = std::move(call_id);
  message.AppendText(std::move(result));
  return message;
}

void TranscriptMessage::AppendText(std::string value) {
  if (value.empty()) {
    return;
  }

  if (!entries.empty() && entries.back().kind == TranscriptEntry::Kind::kText) {
    entries.back().text += value;
    return;
  }

  entries.push_back(TranscriptEntry::Text(std::move(value)));
}

void TranscriptMessage::AppendReasoning(std::string value) {
  if (value.empty()) {
    return;
  }

  if (!entries.empty() && entries.back().kind == TranscriptEntry::Kind::kReasoning) {
    entries.back().text += value;
    return;
  }

  entries.push_back(TranscriptEntry::Reasoning(std::move(value)));
}

void TranscriptMessage::AppendToolCall(TranscriptToolCall call) {
  entries.push_back(TranscriptEntry::ToolCall(std::move(call)));
}

bool TranscriptMessage::HasToolCalls() const {
  for (const auto& entry : entries) {
    if (entry.kind == TranscriptEntry::Kind::kToolCall) {
      return true;
    }
  }

  return false;
}

std::string TranscriptMessage::VisibleText() const {
  return JoinEntries(entries, TranscriptEntry::Kind::kText);
}

std::string TranscriptMessage::ReasoningText() const {
  return JoinEntries(entries, TranscriptEntry::Kind::kReasoning);
}

std::vector<const TranscriptToolCall*> TranscriptMessage::ToolCalls() const {
  std::vector<const TranscriptToolCall*> calls;
  for (const auto& entry : entries) {
    if (entry.kind == TranscriptEntry::Kind::kToolCall) {
      calls.push_back(&entry.tool_call);
    }
  }

  return calls;
}

// ---------------------------------------------------------------------------
// Item ingestion
// ---------------------------------------------------------------------------

std::vector<TranscriptMessage> BuildTranscriptMessages(const std::vector<Item*>& items) {
  std::vector<TranscriptMessage> messages;

  for (const auto* item : items) {
    if (item == nullptr) {
      continue;
    }

    if (item->type == FOUNDRY_LOCAL_ITEM_MESSAGE) {
      const auto& message_item = static_cast<const MessageItem&>(*item);

      // A message with no content parts carries nothing to say — unless it names the participant, in which case it
      // exists to attribute the tool calls that follow it.
      if (message_item.content.empty() && message_item.name.empty()) {
        continue;
      }

      TranscriptMessage message;
      message.role = message_item.role;
      message.name = message_item.name;

      for (const auto& part : message_item.content) {
        if (!part.view || part.view->type != FOUNDRY_LOCAL_ITEM_TEXT) {
          continue;
        }

        const auto& text_item = static_cast<const TextItem&>(*part.view);
        if (text_item.text_type == FOUNDRY_LOCAL_TEXT_ITEM_TYPE_REASONING) {
          message.AppendReasoning(text_item.text);
        } else {
          message.AppendText(text_item.text);
        }
      }

      messages.push_back(std::move(message));
      continue;
    }

    if (item->type == FOUNDRY_LOCAL_ITEM_TOOL_CALL) {
      const auto& call_item = static_cast<const ToolCallItem&>(*item);
      auto call = MakeSuppliedToolCall(call_item.call_id, call_item.name, call_item.arguments);

      // Fold into a directly adjacent assistant message so replayed content and its calls stay in one message.
      if (!messages.empty() && messages.back().role == FOUNDRY_LOCAL_ROLE_ASSISTANT) {
        messages.back().AppendToolCall(std::move(call));
      } else {
        TranscriptMessage message;
        message.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;
        message.AppendToolCall(std::move(call));
        messages.push_back(std::move(message));
      }
      continue;
    }

    if (item->type == FOUNDRY_LOCAL_ITEM_TOOL_RESULT) {
      const auto& result_item = static_cast<const ToolResultItem&>(*item);
      messages.push_back(TranscriptMessage::ToolResult(result_item.call_id, result_item.result));
    }
  }

  return messages;
}

// ---------------------------------------------------------------------------
// ChatTranscript
// ---------------------------------------------------------------------------

namespace {

/// Validate the tool calls of an assistant message against the call IDs issued so far, then record them as
/// outstanding. `issued` and `outstanding` may be the transcript's own sets or scratch copies used for a dry run.
void StageAssistantCalls(const TranscriptMessage& message,
                         std::unordered_set<std::string>& issued,
                         std::unordered_set<std::string>& outstanding) {
  for (const auto* call : message.ToolCalls()) {
    if (call->call_id.empty()) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, "tool call requires a non-empty call id");
    }

    if (!issued.insert(call->call_id).second) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT,
               "tool call id '" + call->call_id + "' is already used by another tool call");
    }

    outstanding.insert(call->call_id);
  }
}

/// Validate a role="tool" message against the outstanding calls and consume the call it answers.
void StageToolResult(const TranscriptMessage& message,
                     const std::unordered_set<std::string>& issued,
                     std::unordered_set<std::string>& outstanding) {
  if (message.tool_call_id.empty()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT, "tool result requires a non-empty call id");
  }

  if (outstanding.erase(message.tool_call_id) == 1) {
    return;
  }

  if (issued.count(message.tool_call_id) != 0) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT,
             "tool call id '" + message.tool_call_id + "' already has a result");
  }

  FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT,
           "tool result references unknown tool call id '" + message.tool_call_id + "'");
}

/// Replay a turn against call-ID state, validating as it goes. The batch is walked in order because a single turn may
/// both replay an assistant call and answer it. `output` is null when only the input batch is being checked.
void StageTurn(const std::vector<TranscriptMessage>& inputs,
               const TranscriptMessage* output,
               std::unordered_set<std::string>& issued,
               std::unordered_set<std::string>& outstanding) {
  for (const auto& message : inputs) {
    if (message.role == FOUNDRY_LOCAL_ROLE_TOOL) {
      StageToolResult(message, issued, outstanding);
    } else if (message.role == FOUNDRY_LOCAL_ROLE_ASSISTANT) {
      StageAssistantCalls(message, issued, outstanding);
    }
  }

  if (output != nullptr) {
    StageAssistantCalls(*output, issued, outstanding);
  }
}

}  // namespace

void ChatTranscript::ValidateInputs(const std::vector<TranscriptMessage>& inputs) const {
  auto issued = issued_;
  auto outstanding = outstanding_;
  StageTurn(inputs, nullptr, issued, outstanding);
}

void ChatTranscript::ValidateGeneratedOutput(const TranscriptMessage& output) const {
  auto issued = issued_;
  auto outstanding = outstanding_;
  StageTurn({}, &output, issued, outstanding);
}

void ChatTranscript::CommitTurn(std::vector<TranscriptMessage> inputs, TranscriptMessage output, TurnTokens tokens) {
  // Stage the whole turn against scratch copies first. Everything below this point is unconditional, so a rejected
  // turn leaves the committed messages and call state exactly as they were.
  auto issued = issued_;
  auto outstanding = outstanding_;
  StageTurn(inputs, &output, issued, outstanding);

  Turn turn;
  turn.message_start = messages_.size();
  turn.tokens = tokens;

  messages_.reserve(messages_.size() + inputs.size() + 1);
  for (auto& message : inputs) {
    messages_.push_back(std::move(message));
  }

  messages_.push_back(std::move(output));
  turns_.push_back(turn);
  issued_ = std::move(issued);
  outstanding_ = std::move(outstanding);
}

ChatTranscript::TurnTokens ChatTranscript::UndoTurns(size_t count) {
  if (count > turns_.size()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_USAGE,
             "Cannot undo " + std::to_string(count) + " turns; only " + std::to_string(turns_.size()) +
                 " turns exist");
  }

  if (count == 0) {
    return {};
  }

  const size_t first_removed = turns_.size() - count;
  TurnTokens tokens = turns_[first_removed].tokens;

  // Any rebuilt turn inside the removed range reset the generator's token scale, so the target turn's boundary no
  // longer refers to the current KV cache. Report no rewind point and let the caller invalidate.
  for (size_t i = first_removed; i < turns_.size(); ++i) {
    if (!turns_[i].tokens.pre_turn.has_value()) {
      tokens.pre_turn.reset();
      break;
    }
  }

  messages_.resize(turns_[first_removed].message_start);
  turns_.resize(first_removed);

  // Rebuild rather than unwind: undone tool calls must stop being answerable, and a full recompute is the only
  // representation that cannot drift from the committed messages.
  RebuildCallState();

  return tokens;
}

void ChatTranscript::RecordMessageCalls(const TranscriptMessage& message) {
  if (message.role == FOUNDRY_LOCAL_ROLE_TOOL) {
    outstanding_.erase(message.tool_call_id);
    return;
  }

  for (const auto* call : message.ToolCalls()) {
    issued_.insert(call->call_id);
    outstanding_.insert(call->call_id);
  }
}

void ChatTranscript::RebuildCallState() {
  issued_.clear();
  outstanding_.clear();

  for (const auto& message : messages_) {
    RecordMessageCalls(message);
  }
}

}  // namespace fl
