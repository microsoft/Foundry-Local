// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "items/message_item.h"

#include <memory>
#include <string>
#include <vector>

// Forward declarations
struct OgaTokenizer;
struct OgaSequences;

namespace fl {

/// Render a MessageItem's content as a plain string suitable for the chat template.
///
/// - Single-text messages return their text directly.
/// - Multi-part messages concatenate their TextItem parts. Non-text parts (images, audio) are skipped.
/// - REASONING-typed TextItem parts are always skipped: chain-of-thought content is preserved on the assistant
///   message in `history_` (useful for caller inspection) but must NOT be re-injected into the model's prompt.
///   Reasoning models (qwen3, etc.) are trained to never see prior reasoning in the conversation.
///
/// This is the single canonical "message → prompt string" entry point. Any new code path that builds prompts from
/// MessageItems should call this rather than iterating `msg.content` directly, so the REASONING-skip policy stays
/// in one place.
std::string RenderMessageForPrompt(const MessageItem& msg);

/// Build a chat prompt string from a list of messages.
/// Uses the tokenizer's built-in chat template (via OgaTokenizer::ApplyChatTemplate).
///
/// @param messages       Ordered list of chat messages (system, user, assistant, tool, etc.)
/// @param tokenizer      ORT GenAI tokenizer (for ApplyChatTemplate)
/// @param tools_json     Optional JSON string describing available tools. Pass empty string for none.
/// @returns The formatted prompt string ready for tokenization
std::string BuildChatPrompt(const std::vector<MessageItem>& messages,
                            OgaTokenizer& tokenizer,
                            const std::string& tools_json = "");

/// Encode a prompt string into token sequences using the tokenizer.
/// Returns a unique_ptr to OgaSequences. Caller takes ownership.
///
/// @param prompt     The formatted prompt string (from BuildChatPrompt)
/// @param tokenizer  ORT GenAI tokenizer
/// @returns Encoded token sequences
std::unique_ptr<OgaSequences> EncodePrompt(const std::string& prompt,
                                           OgaTokenizer& tokenizer);

}  // namespace fl
