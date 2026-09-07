// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "inferencing/generative/chat/chat_transcript.h"
#include "inferencing/generative/chat/reasoning_stream_splitter.h"
#include "inferencing/generative/chat/search_options.h"
#include "inferencing/generative/toolcalling/tool_call_context.h"
#include "inferencing/generative/toolcalling/tool_call_utils.h"
#include "inferencing/session/session.h"
#include "items/message_item.h"
#include "logger.h"

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace fl {

class GenAIModelInstance;
class OnnxChatGenerator;

using GeneratedOutputEvent = std::variant<ReasoningStreamSplitter::Segment, ParsedToolCall>;

/// A chat session that maintains an authoritative conversation transcript across turns.
/// Designed for multi-turn conversations where visible text, reasoning, and tool calls accumulate in event order
/// and are sent with each generation request (for use with the OpenAI Responses API pattern).
///
/// Generator caching: after the first non-JSON request, the ORT GenAI generator is cached.
/// Subsequent turns append only new messages to the cached generator, reusing the KV cache. Turns that carry tool
/// calls or tool results rebuild from the full committed transcript instead, because a tool-only suffix is not
/// something chat templates can render on its own.
/// OpenAI chat completions JSON requests (TextItem with text_type == OPENAI_JSON) always create a fresh
/// generator and never use the cache.
class ChatSession : public Session {
 public:
  ChatSession(const fl::Model& catalog_model, GenAIModelInstance& model, ILogger& logger, ITelemetry& telemetry);
  ~ChatSession();

  // Movable: transfers session refcount ownership to the moved-to instance.
  ChatSession(ChatSession&& other) noexcept;
  ChatSession& operator=(ChatSession&&) = delete;

  SessionType Type() const override;

  /// Get the authoritative conversation transcript. Not safe to inspect while a request is in flight.
  const ChatTranscript& Transcript() const;

  /// Get the number of messages in the transcript. Not safe to inspect while a request is in flight.
  size_t MessageCount() const;

  /// Get the number of completed turns. Not safe to inspect while a request is in flight.
  size_t TurnCount() const override;

  /// Undo the last `count` completed turns: rewinds the cached generator and removes
  /// each turn's input messages and assistant reply from the transcript.
  /// If all turns are undone, the cached generator is destroyed.
  ///
  /// Vision turns: image input is only allowed while the conversation has no
  /// history. UndoTurns rolls back messages, so undoing every turn does make
  /// the session accept media again — but the media bytes of an undone turn
  /// are gone either way, because they never entered the transcript.
  ///
  /// Blocks until any in-flight request on this session completes.
  ///
  /// @param count  Number of turns to undo. Must be <= TurnCount().
  void UndoTurns(size_t count) override;

 private:
  // populate session_options_
  void SetSessionOptionsImpl(const KeyValuePairs& options) override;

  /// Process a request: extracts items and parameters from the generic request, generates a response, and on
  /// success commits the turn to the transcript.
  void ProcessRequestImpl(const Request& request, Response& response) override;

  /// Build tool calling context from request parameters and a snapshot of the session's tool definitions.
  ///
  /// The snapshot is supplied by the caller rather than read here so that one turn resolves its replayed calls, its
  /// prompt, and its produced calls against the same tool set.
  ToolCallContext BuildToolCallContext(const Request& request, const std::vector<ToolDefinition>& definitions) const;

  /// Update per-turn fields (tool_choice, forced tool, output encoding, guidance) on an existing tool context.
  /// Called on the cached-generator path so each turn gets fresh per-request settings
  /// while keeping session-level tool definitions and marker tokens stable.
  void UpdateToolContextForTurn(const Request& request, ToolCallContext& tool_ctx) const;

  /// Resolve the two request-scoped inputs that decide how this turn's generated output is *read*: the tool the
  /// caller explicitly forced, and the raw-envelope dialect in effect.
  ///
  /// The descriptor is resolved from the request's own options first and from the model's published properties
  /// second. The request value is an atomic override, never a merge, and an invalid one is an error rather than a
  /// reason to use the model's.
  ///
  /// @throws fl::Exception FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT when a configured descriptor is not valid.
  void ResolveToolOutputPolicy(const Request& request, ToolCallContext& tool_ctx) const;

  /// Build final response items from the typed segments and tool calls produced during generation.
  void ProcessGeneratedOutput(std::vector<GeneratedOutputEvent> events,
                              const SearchOptions& effective_options, bool canceled,
                              Response& response, int prompt_tokens, int total_tokens,
                              int reasoning_tokens);

  /// Process a request whose first item is a TextItem tagged OPENAI_JSON containing an OpenAI chat completions
  /// request. Parses the JSON, converts to internal items, runs generation, and produces an OPENAI_JSON-tagged
  /// TextItem response with the OpenAI ChatCompletionResponse.
  /// Does not use or update the transcript or the cached generator.
  void ProcessChatCompletionsJson(const std::string& request_json, const Request& original_request,
                                  Response& response);

  /// Drop the cached generator and its tool context. Called whenever the generator's KV cache can no longer be
  /// trusted to match the committed transcript — the next turn then rebuilds from full committed history.
  /// noexcept because it also runs from a scope guard during exception unwinding.
  void InvalidateCachedGenerator() noexcept;

  GenAIModelInstance& Model() { return model_; }
  const GenAIModelInstance& Model() const { return model_; }

  ILogger& logger_;
  GenAIModelInstance& model_;
  // Tracks who is responsible for calling model_.ReleaseSession(). Set to false on the
  // moved-from instance so the refcount transfers cleanly across moves.
  bool owns_session_ = true;
  ChatTranscript transcript_;
  SearchOptions session_options_;

  // Cached generator for continuous decoding (non-JSON path only).
  // Null until first non-JSON ProcessRequestImpl call.
  std::unique_ptr<OnnxChatGenerator> cached_generator_;

  // Tool context used when creating the cached generator.
  // Reused for subsequent turns to maintain tool definition consistency.
  ToolCallContext cached_tool_ctx_;

  // The system prefix baked into cached_generator_'s prompt (the kSystemPromptOption value of the turn that built
  // it). Deliberately not part of the transcript: it is request state, so it can never accumulate a copy per turn
  // and is never replayed from a stored conversation. A turn that asks for a different prefix rebuilds; a turn that
  // asks for the same one keeps the KV cache.
  std::string system_prompt_;
};

}  // namespace fl
