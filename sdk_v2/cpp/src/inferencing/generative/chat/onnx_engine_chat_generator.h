// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "inferencing/generative/chat/chat_generator.h"
#include "inferencing/generative/chat/onnx_chat_engine.h"
#include "inferencing/generative/chat/search_options.h"
#include "inferencing/generative/toolcalling/tool_call_context.h"

#include <atomic>
#include <memory>
#include <optional>

struct OgaTokenizerStream;

namespace fl {

class GenAIModelInstance;

namespace engine_generator_internal {

/// Admit a retained-conversation turn, then hand the turn a fresh token decoder.
///
/// ORT GenAI builds each turn's decoder (a `StopStringController` owning one `TokenizerStream`) before the admission
/// attempt and installs it only in `Request::CommitTurnAdmission()`; `Request::RollbackTurnAdmission()` discards it
/// and leaves whatever decoder the Request already had completely untouched. Foundry mirrors that ordering: a
/// rejected `BeginTurn` (capacity eviction, token budget, an option this backend cannot honor) must leave the
/// generator able to keep decoding the state it still has, while every admitted turn starts from a stream carrying
/// no partial UTF-8/BPE bytes from the previous turn. Prompt and continuation tokens are never pushed through the
/// decoder on either side — upstream observes generated tokens only.
template <typename AdmitTurnFn, typename ResetDecoderFn>
void AdmitTurnThenResetDecoder(AdmitTurnFn&& admit_turn, ResetDecoderFn&& reset_decoder) {
  admit_turn();
  reset_decoder();
}

}  // namespace engine_generator_internal

/// ChatGenerator adapter for a conversation scheduled by a model-owned ORT GenAI Engine.
class OnnxEngineChatGenerator final : public ChatGenerator {
 public:
  ~OnnxEngineChatGenerator() override;

  bool IsDone() const override;
  void GenerateNextToken() override;
  std::string Decode() override;
  int TokenCount() const override;
  int PromptTokenCount() const override;
  void Cancel() override;
  int AppendMessages(const std::vector<MessageItem>& new_messages,
                     GenAIModelInstance& model,
                     const ToolCallContext& tool_ctx,
                     const SearchOptions& options) override;
  bool CanRewind() const override { return false; }
  void RewindTo(int token_count) override;
  std::optional<ChatTurnUsage> GetTurnUsage() const override;

  static std::unique_ptr<OnnxEngineChatGenerator> Create(
      const std::vector<MessageItem>& messages,
      const SearchOptions& options,
      GenAIModelInstance& model,
      const ToolCallContext& tool_ctx);

 private:
  OnnxEngineChatGenerator(OnnxChatEngine& engine,
                          std::shared_ptr<OnnxChatEngine::Conversation> conversation,
                          std::unique_ptr<OgaTokenizerStream> stream,
                          GenAIModelInstance& model,
                          int prompt_token_count);

  /// Replace this turn's token decoder. Called only once the Engine has admitted a turn.
  void ResetTurnDecoder();

  OnnxChatEngine& engine_;
  std::shared_ptr<OnnxChatEngine::Conversation> conversation_;
  std::unique_ptr<OgaTokenizerStream> stream_;
  GenAIModelInstance& model_;
  int prompt_token_count_ = 0;
  std::optional<int32_t> current_token_;
  std::atomic<bool> cancelled_{false};
};

}  // namespace fl
