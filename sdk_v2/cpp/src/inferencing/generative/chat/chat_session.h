// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "inferencing/generative/chat/reasoning_stream_splitter.h"
#include "inferencing/generative/chat/search_options.h"
#include "inferencing/generative/chat/stop_strings.h"
#include "inferencing/generative/toolcalling/tool_call_context.h"
#include "inferencing/generative/toolcalling/tool_call_utils.h"
#include "inferencing/session/session.h"
#include "items/message_item.h"
#include "logger.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace fl {

class GenAIModelInstance;
class ChatGenerator;

namespace chat_session_internal {

template <typename SegmentProcessor>
bool PushDecodedFragment(const std::string& fragment,
                         std::optional<int32_t> token_id,
                         StopStringFilter* stop_filter,
                         ReasoningStreamSplitter& splitter,
                         SegmentProcessor&& process_segments) {
  if (stop_filter == nullptr) {
    if (token_id.has_value()) {
      process_segments(splitter.Push(*token_id, fragment));
    } else if (!fragment.empty()) {
      process_segments(splitter.Push(fragment));
    }

    return false;
  }

  if (stop_filter->matched()) {
    return true;
  }

  if (fragment.empty()) {
    if (token_id.has_value()) {
      process_segments(splitter.Push(*token_id, fragment));
    }

    return false;
  }

  auto filtered = stop_filter->PushWithTokenAlignment(fragment);
  if (!filtered.text.empty()) {
    if (filtered.token_aligned && token_id.has_value()) {
      process_segments(splitter.Push(*token_id, std::move(filtered.text)));
    } else {
      // Filtering combined or shortened decoded token fragments, so the current token ID is no longer aligned.
      process_segments(splitter.Push(filtered.text));
    }
  }

  return stop_filter->matched();
}

template <typename SegmentProcessor>
void FlushDecodedStream(StopStringFilter* stop_filter,
                        ReasoningStreamSplitter& splitter,
                        SegmentProcessor&& process_segments) {
  if (stop_filter != nullptr && !stop_filter->matched()) {
    auto tail = stop_filter->Flush();
    if (!tail.empty()) {
      process_segments(splitter.Push(tail));
    }
  }

  process_segments(splitter.Flush());
}

/// Resolve the final finish reason for one generated turn.
/// Complete tool calls win over a later host-side stop-string match because the
/// structured tool-call protocol is the durable caller-visible outcome.
flFinishReason ResolveGeneratedFinishReason(bool canceled,
                                            bool has_tool_calls,
                                            bool stop_sequence_matched,
                                            bool host_output_limit_reached,
                                            std::optional<flFinishReason> backend_finish_reason,
                                            int completion_tokens,
                                            std::optional<int> max_output_tokens);

/// Whether the host output limit stopped generation before the backend completed naturally.
bool DidHostOutputLimitTruncate(int output_tokens, int max_output_tokens, bool backend_finished);

/// Whether the selected generator path requires host-side output-limit enforcement.
bool ShouldEnforceHostOutputLimit(ChatBackendKind backend_kind, bool media_turn);

/// Whether the retained generator must be rebuilt from full history before appending this turn.
///
/// Guidance is baked into a classic generator at creation time, and ORT GenAI only allows continuous decoding on a
/// static-batching Engine while exactly one request is resident in the batch
/// (`Engine::ValidateRequestCanContinue`: "Continuous decoding requires exactly one resident request in a static
/// engine batch"). Foundry sizes a static Engine from `engine.static_batching.max_batch_size` and lets a model host
/// several conversations, so it can never prove that precondition — a static Engine always rebuilds.
bool ShouldRebuildRetainedGeneratorBeforeAppend(ChatBackendKind backend_kind,
                                                bool guidance_requirement_changed,
                                                bool guidance_payload_changed,
                                                bool retained_generation_settings_changed);

/// Whether retained backend state must be discarded after a successful turn commits.
/// Host-side stop filtering or output-limit truncation can leave retained state ahead of committed history, so
/// either condition invalidates even when the current turn still succeeds.
bool ShouldInvalidateRetainedGenerationStateAfterSuccessfulTurn(ChatBackendKind backend_kind,
                                                                bool grammar_was_active,
                                                                bool reasoning_was_active,
                                                                bool stop_sequence_matched,
                                                                bool host_output_limit_reached);

}  // namespace chat_session_internal

using GeneratedOutputEvent = std::variant<ReasoningStreamSplitter::Segment, ParsedToolCall>;

/// A chat session that maintains conversation history across turns.
/// Designed for multi-turn conversations where message history accumulates
/// and is sent with each generation request (for use with the OpenAI
/// Responses API pattern).
///
/// Generator caching: after the first non-JSON request, the ORT GenAI generator is cached.
/// Subsequent turns append only new messages to the cached generator, reusing the KV cache.
/// OpenAI chat completions JSON requests (TextItem with text_type == OPENAI_JSON) always create a fresh
/// generator and never use the cache.
class ChatSession : public Session {
 public:
  /// Tracks the token-level and history-level boundaries of a single conversation turn.
  /// Used for generator rewind and history rollback on error or undo.
  struct TurnRecord {
    size_t history_start;       // index in history_ where this turn's input messages begin
    size_t input_count;         // number of input messages (user + tool results) in this turn
    int pre_turn_token_count;   // generator sequence length before this turn's input was appended
    int post_turn_token_count;  // generator sequence length after generation completed
    bool can_rewind_to_pre_turn;
    // The assistant reply is at history_[history_start + input_count]
  };

  ChatSession(const fl::Model& catalog_model, GenAIModelInstance& model, ILogger& logger, ITelemetry& telemetry);
  ~ChatSession();

  // Movable: transfers session refcount ownership to the moved-to instance.
  ChatSession(ChatSession&& other) noexcept;
  ChatSession& operator=(ChatSession&&) = delete;

  SessionType Type() const override;

  /// Get the full conversation history.
  const std::vector<MessageItem>& GetHistory() const;

  /// Get the number of messages in the history.
  size_t MessageCount() const;

  /// Get the number of completed turns.
  size_t TurnCount() const override;

  /// Undo the last `count` completed turns: rewinds the cached generator and removes
  /// each turn's input messages and assistant reply from history.
  /// If all turns are undone, the cached generator is destroyed.
  ///
  /// Vision turns: image input is only allowed on the first turn of a
  /// session. UndoTurns rolls back history but does not undo this
  /// constraint — once a session has started, no later turn may include
  /// images. Start a new ChatSession to send images.
  ///
  /// @param count  Number of turns to undo. Must be <= TurnCount().
  void UndoTurns(size_t count) override;

 private:
  // populate session_options_
  void SetSessionOptionsImpl(const KeyValuePairs& options) override;

  /// Process a request: extracts MESSAGE items and parameters from the generic request,
  /// generates a response, and on success commits messages to conversation history.
  void ProcessRequestImpl(const Request& request, Response& response) override;

  /// Build tool calling context from request parameters and session tool definitions.
  ToolCallContext BuildToolCallContext(const Request& request) const;

  /// Update per-turn fields (tool_choice, guidance) on an existing tool context.
  /// Called on the cached-generator path so each turn gets fresh per-request settings
  /// while keeping session-level tool definitions and marker tokens stable.
  void UpdateToolContextForTurn(const Request& request, ToolCallContext& tool_ctx) const;

  /// Build final response items from the typed segments and tool calls produced during generation.
  void ProcessGeneratedOutput(std::vector<GeneratedOutputEvent> events,
                              const SearchOptions& effective_options,
                              bool canceled,
                              bool stop_sequence_matched,
                              bool host_output_limit_reached,
                              Response& response,
                              int prompt_tokens,
                              int total_tokens,
                              int reasoning_tokens,
                              std::optional<flFinishReason> backend_finish_reason);

  /// Process a request whose first item is a TextItem tagged OPENAI_JSON containing an OpenAI chat completions
  /// request. Parses the JSON, converts to internal items, runs generation, and produces an OPENAI_JSON-tagged
  /// TextItem response with the OpenAI ChatCompletionResponse.
  /// Does not use or update history_ or the cached generator.
  void ProcessChatCompletionsJson(const std::string& request_json, const Request& original_request,
                                  Response& response);

  /// Commit input messages and assistant reply to history after a successful turn.
  void CommitTurn(std::vector<MessageItem>&& new_messages,
                  std::string assistant_history,
                  int pre_turn_token_count,
                  int post_turn_token_count,
                  bool can_rewind_to_pre_turn);

  GenAIModelInstance& Model() { return model_; }
  const GenAIModelInstance& Model() const { return model_; }

  ILogger& logger_;
  GenAIModelInstance& model_;
  // Tracks who is responsible for calling model_.ReleaseSession(). Set to false on the
  // moved-from instance so the refcount transfers cleanly across moves.
  bool owns_session_ = true;
  std::vector<MessageItem> history_;
  std::vector<TurnRecord> turns_;
  SearchOptions session_options_;

  // Cached generator for continuous decoding (non-JSON path only).
  // Null until first non-JSON ProcessRequestImpl call.
  std::unique_ptr<ChatGenerator> cached_generator_;

  // Tool context used when creating the cached generator.
  // Reused for subsequent turns to maintain tool definition consistency.
  ToolCallContext cached_tool_ctx_;

  // Search settings baked into the retained generator or Engine request.
  SearchOptions cached_search_options_;
};

}  // namespace fl
