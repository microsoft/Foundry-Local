// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "inferencing/generative/chat/chat_session.h"

#include "contracts/chat_completions.h"
#include "contracts/chat_completions_converter.h"
#include "inferencing/generative/chat/media_input.h"
#include "inferencing/generative/chat/onnx_chat_generator.h"
#include "inferencing/generative/chat/reasoning_stream_splitter.h"
#include "inferencing/generative/genai_model_instance.h"
#include "inferencing/generative/toolcalling/tool_call_context.h"
#include "inferencing/generative/toolcalling/tool_call_stream_accumulator.h"
#include "inferencing/generative/toolcalling/tool_call_utils.h"
#include "items/text_item.h"
#include "items/tool_call_item.h"
#include "model.h"
#include "util/scope_guard.h"
#include "utils.h"

#include <chrono>
#include <fmt/format.h>
#include <optional>
#include <utility>

namespace fl {

namespace {

// Translate a parsed tool_choice into the text_output / tool_output flags on the tool-call context.
// Defaults (nullopt) match "auto": the model is free to emit text or tool calls.
void ApplyToolChoiceToContext(std::optional<flToolChoice> tool_choice, ToolCallContext& tool_ctx) {
  switch (tool_choice.value_or(FOUNDRY_LOCAL_TOOL_CHOICE_AUTO)) {
    case FOUNDRY_LOCAL_TOOL_CHOICE_NONE:
      tool_ctx.text_output = true;
      tool_ctx.tool_output = false;
      break;
    case FOUNDRY_LOCAL_TOOL_CHOICE_REQUIRED:
      tool_ctx.text_output = false;
      tool_ctx.tool_output = true;
      break;
    case FOUNDRY_LOCAL_TOOL_CHOICE_AUTO:
    default:
      tool_ctx.text_output = true;
      tool_ctx.tool_output = true;
      break;
  }
}

using TextSegment = ReasoningStreamSplitter::Segment;

/// Value of `key` in `options`, or an empty string when it is absent.
std::string GetOptionOrEmpty(const KeyValuePairs& options, const char* key) {
  auto it = options.find(key);
  return it != options.end() ? it->second : std::string{};
}

/// Build the assistant transcript message for a completed generation. The generation events are already in the order
/// the model produced them, so the transcript keeps visible text, reasoning, and calls interleaved exactly as emitted.
///
/// The turn's output has already been streamed to the caller by the time this runs, so a model that emitted argument
/// bytes that are not a JSON object must not fail the request. The raw bytes are kept verbatim on the call (and on
/// the response items), the normalized form degrades to an empty object, and the defect is logged.
TranscriptMessage MakeAssistantMessage(const std::vector<GeneratedOutputEvent>& events, ILogger& logger) {
  TranscriptMessage assistant;
  assistant.role = FOUNDRY_LOCAL_ROLE_ASSISTANT;

  for (const auto& event : events) {
    if (const auto* segment = std::get_if<TextSegment>(&event)) {
      if (segment->type == FOUNDRY_LOCAL_TEXT_ITEM_TYPE_REASONING) {
        assistant.AppendReasoning(segment->text);
      } else {
        assistant.AppendText(segment->text);
      }
      continue;
    }

    const auto& parsed = std::get<ParsedToolCall>(event);
    auto generated = MakeGeneratedToolCall(parsed.id, parsed.name, parsed.arguments);

    if (!generated.arguments_usable) {
      logger.Log(LogLevel::Warning,
                 fmt::format("Model emitted tool call '{}' with arguments that are not a JSON object; keeping the "
                             "raw arguments and treating the call as having none: {}",
                             parsed.name, parsed.arguments));
    }

    assistant.AppendToolCall(std::move(generated.call));
  }

  return assistant;
}

ReasoningStreamSplitter CreateReasoningSplitter(const ToolCallContext& tool_ctx,
                                                GenAIModelInstance& model,
                                                bool prompt_opens_reasoning) {
  if (!tool_ctx.supports_reasoning) {
    return {"", ""};
  }

  auto markers = ResolveReasoningMarkers(tool_ctx, model);
  auto ignored_token_ids = model.GetPreprocessor().GetEosTokenIds();
  return {std::move(markers.start), std::move(markers.end), std::move(markers.start_token_ids),
          std::move(markers.end_token_ids), std::move(ignored_token_ids), prompt_opens_reasoning};
}

void AppendSegment(std::vector<TextSegment>& destination, std::string text, flTextItemType type) {
  if (text.empty()) {
    return;
  }

  if (!destination.empty() && destination.back().type == type) {
    destination.back().text += text;
  } else {
    destination.push_back({std::move(text), type});
  }
}

void AppendGeneratedSegment(std::vector<GeneratedOutputEvent>& destination, std::string text, flTextItemType type) {
  if (text.empty()) {
    return;
  }

  if (!destination.empty()) {
    auto* previous = std::get_if<TextSegment>(&destination.back());
    if (previous && previous->type == type) {
      previous->text += text;
      return;
    }
  }

  destination.push_back(TextSegment{std::move(text), type});
}

/// Report a turn ended at its tool call because the model kept talking afterwards. Both generation paths share the
/// wording so the behaviour reads the same in a log wherever it happened.
void LogTurnEndedAfterToolCall(ILogger& logger, const std::string& dropped_text) {
  logger.Log(LogLevel::Warning,
             fmt::format("Model emitted visible text after a tool call; ending the turn at the call because the "
                         "text cannot be represented in the order it was produced: {}",
                         dropped_text));
}

bool AcceptVisibleText(AssistantTurnGuard& guard, const std::string& text, ILogger& logger) {
  const auto disposition = guard.OfferVisibleText(text);
  if (disposition == TextDisposition::kEndTurn) {
    LogTurnEndedAfterToolCall(logger, text);
  }

  return disposition == TextDisposition::kEmit;
}

}  // namespace

ChatSession::ChatSession(const fl::Model& catalog_model, GenAIModelInstance& model, ILogger& logger, ITelemetry& telemetry)
    : Session(catalog_model, logger, telemetry), logger_(logger), model_(model) {
  logger_.Log(LogLevel::Debug, fmt::format("Creating ChatSession for model: {}", model.ModelId()));
  // Last so a throw above does not leak a refcount; nothing below can throw.
  model_.AcquireSession();
}

ChatSession::~ChatSession() {
  if (owns_session_) {
    model_.ReleaseSession();
  }
}

ChatSession::ChatSession(ChatSession&& other) noexcept
    : Session(std::move(other)),
      logger_(other.logger_),
      model_(other.model_),
      owns_session_(other.owns_session_),
      transcript_(std::move(other.transcript_)),
      session_options_(std::move(other.session_options_)),
      cached_generator_(std::move(other.cached_generator_)),
      cached_tool_ctx_(std::move(other.cached_tool_ctx_)),
      system_prompt_(std::move(other.system_prompt_)) {
  other.owns_session_ = false;
}

SessionType ChatSession::Type() const {
  return SessionType::kChat;
}

void ChatSession::SetSessionOptionsImpl(const KeyValuePairs& options) {
  session_options_ = SearchOptions::FromParameters(options);
}

void ChatSession::UpdateToolContextForTurn(const Request& request, ToolCallContext& tool_ctx) const {
  // Re-derive tool_choice → text_output / tool_output for this turn.
  // ParseToolChoice rejects unknown values with FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT.
  auto tool_choice = SearchOptions::ParseToolChoice(request.options);
  if (!tool_choice.has_value()) {
    tool_choice = session_options_.tool_choice;
  }

  if (tool_ctx.HasTools()) {
    ApplyToolChoiceToContext(tool_choice, tool_ctx);
  }

  // Re-derive per-request guidance
  tool_ctx.guidance_type = GetOptionOrEmpty(request.options, "guidance_type");
  tool_ctx.guidance_data = GetOptionOrEmpty(request.options, "guidance_data");
}

ToolCallContext ChatSession::BuildToolCallContext(const Request& request) const {
  ToolCallContext tool_ctx;

  tool_ctx.tool_call_start = GetOptionOrEmpty(request.options, FOUNDRY_LOCAL_MODEL_PROP_TOOL_CALL_START_STR);
  tool_ctx.tool_call_end = GetOptionOrEmpty(request.options, FOUNDRY_LOCAL_MODEL_PROP_TOOL_CALL_END_STR);

  // Fall back to model info properties if not specified in the request
  const auto& info = CatalogModel().Info();

  // Check if the model supports tool calling
  const auto* tool_calling_val = info.GetPropertyInt(FOUNDRY_LOCAL_MODEL_PROP_SUPPORTS_TOOL_CALLING_INT);
  if (tool_calling_val && *tool_calling_val == 1) {
    tool_ctx.supports_tool_calling = true;
  }

  if (tool_ctx.tool_call_start.empty()) {
    const auto* val = info.GetPropertyStr(FOUNDRY_LOCAL_MODEL_PROP_TOOL_CALL_START_STR);
    if (val) {
      tool_ctx.tool_call_start = *val;
    }
  }

  if (tool_ctx.tool_call_end.empty()) {
    const auto* val = info.GetPropertyStr(FOUNDRY_LOCAL_MODEL_PROP_TOOL_CALL_END_STR);
    if (val) {
      tool_ctx.tool_call_end = *val;
    }
  }

  // Catalog metadata is immutable and may not contain markers for models whose
  // tokenizer defines them dynamically. Read those markers from the loaded GenAI
  // model without mutating the published ModelInfo.
  const auto& tag_info = model_.GetTagInfo();
  if (tool_ctx.tool_call_start.empty()) {
    tool_ctx.tool_call_start = tag_info.bot_str;
  }
  if (tool_ctx.tool_call_end.empty()) {
    tool_ctx.tool_call_end = tag_info.eot_str;
  }

  // Check if the model supports chain-of-thought reasoning
  const auto* reasoning_val = info.GetPropertyInt(FOUNDRY_LOCAL_MODEL_PROP_SUPPORTS_REASONING_INT);
  if (reasoning_val && *reasoning_val == 1) {
    tool_ctx.supports_reasoning = true;
  }

  // Read reasoning marker tokens — same pattern as tool_call tokens
  tool_ctx.reasoning_start = GetOptionOrEmpty(request.options, FOUNDRY_LOCAL_MODEL_PROP_REASONING_START_STR);
  tool_ctx.reasoning_end = GetOptionOrEmpty(request.options, FOUNDRY_LOCAL_MODEL_PROP_REASONING_END_STR);

  if (tool_ctx.reasoning_start.empty()) {
    const auto* val = info.GetPropertyStr(FOUNDRY_LOCAL_MODEL_PROP_REASONING_START_STR);
    if (val) {
      tool_ctx.reasoning_start = *val;
    }
  }

  if (tool_ctx.reasoning_end.empty()) {
    const auto* val = info.GetPropertyStr(FOUNDRY_LOCAL_MODEL_PROP_REASONING_END_STR);
    if (val) {
      tool_ctx.reasoning_end = *val;
    }
  }
  if (tool_ctx.reasoning_start.empty()) {
    tool_ctx.reasoning_start = tag_info.bor_str;
  }
  if (tool_ctx.reasoning_end.empty()) {
    tool_ctx.reasoning_end = tag_info.eor_str;
  }

  // Accumulate tool definitions from the session.
  // Tool definitions may come from two sources:
  // 1. Individual AddToolDefinition calls (name + description + parameters schema)
  // 2. ChatCompletions converter (pre-serialized full OpenAI tools JSON array, no name)
  // We need to produce a JSON array in OpenAI tools format for the chat template.
  nlohmann::json tools_array = nlohmann::json::array();
  bool has_preserialized = false;

  for (const auto& td : ToolDefinitions()) {
    if (!td.name.empty()) {
      // Individual tool: wrap in OpenAI format
      nlohmann::json tool;
      tool["type"] = "function";
      tool["function"]["name"] = td.name;
      tool["function"]["description"] = td.description;

      if (!td.json_schema.empty()) {
        tool["function"]["parameters"] = nlohmann::json::parse(td.json_schema);
      }

      tools_array.push_back(std::move(tool));
    } else if (!td.json_schema.empty()) {
      // Pre-serialized from ChatCompletions path — already a complete tools array
      has_preserialized = true;
      tool_ctx.tools_json += td.json_schema;
    }
  }

  if (!tools_array.empty()) {
    tool_ctx.tools_json = tools_array.dump();
  } else if (!has_preserialized) {
    tool_ctx.tools_json.clear();
  }

  // Determine text_output / tool_output from tool_choice parameter.
  // ParseToolChoice rejects unknown values with FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT.
  auto tool_choice = SearchOptions::ParseToolChoice(request.options);
  if (!tool_choice.has_value()) {
    tool_choice = session_options_.tool_choice;
  }

  if (tool_ctx.HasTools()) {
    ApplyToolChoiceToContext(tool_choice, tool_ctx);
  }

  // Read user-specified guidance from request parameters
  tool_ctx.guidance_type = GetOptionOrEmpty(request.options, "guidance_type");
  tool_ctx.guidance_data = GetOptionOrEmpty(request.options, "guidance_data");

  return tool_ctx;
}

void ChatSession::ProcessGeneratedOutput(std::vector<GeneratedOutputEvent> events,
                                         const SearchOptions& effective_options, bool canceled,
                                         Response& response, int prompt_tokens, int total_tokens,
                                         int reasoning_tokens) {
  int completion_tokens = total_tokens - prompt_tokens;
  bool has_tool_calls = false;
  std::vector<TextSegment> segments;

  auto flush_segments = [&]() {
    if (segments.empty()) {
      return;
    }

    std::unique_ptr<MessageItem> output_item;

    if (segments.size() == 1 && segments.front().type == FOUNDRY_LOCAL_TEXT_ITEM_TYPE_DEFAULT) {
      // Common case: pure visible text → single-text MessageItem.
      output_item = std::make_unique<MessageItem>(FOUNDRY_LOCAL_ROLE_ASSISTANT, std::move(segments.front().text));
    } else {
      // Mixed / reasoning-only → multi-part MessageItem of typed TextItems.
      std::vector<std::unique_ptr<Item>> parts;
      parts.reserve(segments.size());
      for (auto& seg : segments) {
        parts.push_back(std::make_unique<TextItem>(std::move(seg.text), seg.type));
      }
      output_item = std::make_unique<MessageItem>(FOUNDRY_LOCAL_ROLE_ASSISTANT, std::move(parts));
    }

    response.items.push_back(std::move(output_item));
    segments.clear();
  };

  for (auto& event : events) {
    if (auto* segment = std::get_if<TextSegment>(&event)) {
      AppendSegment(segments, std::move(segment->text), segment->type);
      continue;
    }

    flush_segments();
    auto& call = std::get<ParsedToolCall>(event);
    response.items.push_back(std::make_unique<ToolCallItem>(std::move(call.id), std::move(call.name),
                                                            std::move(call.arguments)));
    has_tool_calls = true;
  }
  flush_segments();

  if (canceled) {
    response.finish_reason = FOUNDRY_LOCAL_FINISH_NONE;
  } else if (has_tool_calls) {
    response.finish_reason = FOUNDRY_LOCAL_FINISH_TOOL_CALLS;
  } else {
    int max_output = effective_options.max_output_tokens.value_or(0);

    if (max_output > 0 && completion_tokens >= max_output) {
      response.finish_reason = FOUNDRY_LOCAL_FINISH_LENGTH;
    } else {
      response.finish_reason = FOUNDRY_LOCAL_FINISH_STOP;
    }
  }

  response.usage.prompt_tokens = prompt_tokens;
  response.usage.completion_tokens = completion_tokens;
  response.usage.total_tokens = total_tokens;
  response.usage.reasoning_tokens = reasoning_tokens;

  logger_.Log(LogLevel::Verbose,
              fmt::format(
                  "Completion stats: Total Tokens: {}, Prompt Tokens: {}, Completion Tokens: {}, Reasoning Tokens: {}",
                  total_tokens, prompt_tokens, completion_tokens, reasoning_tokens));
}

void ChatSession::ProcessRequestImpl(const Request& request, Response& response) {
  // OpenAI chat completions JSON pass-through: a TEXT item tagged OPENAI_JSON. Routes to a separate handler that
  // never uses the cached generator or the transcript (the JSON payload is self-contained).
  for (const auto* item : request.items) {
    if (item->type == FOUNDRY_LOCAL_ITEM_TEXT) {
      const auto& text_item = static_cast<const fl::TextItem&>(*item);

      if (text_item.text_type == FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON) {
        ProcessChatCompletionsJson(text_item.text, request, response);
        return;
      }
    }
  }

  // Collect this turn's input messages locally — nothing reaches the transcript until the turn commits. Replay
  // segment boundaries travel with the items so a reconstructed conversation regroups exactly as it was committed.
  auto ingest = IngestRequestItems(request.items, request.item_segment_starts);
  auto inputs = std::move(ingest.messages);

  // Reject bad tool-call correlation before any generator work: a rejected turn must cost nothing and leave no
  // state. This runs before the has-anything-to-say check below so a tool result that answers nothing is reported
  // as the correlation error it is — an empty result for an unknown call ID is a broken conversation, not an
  // empty request.
  transcript_.ValidateInputs(inputs);

  // Media is single-shot. One conversation-scoped rule covers a live session and a conversation replayed into this
  // request after the session cache dropped it, so a continuation is rejected identically either way.
  auto media = CollectMediaInput(request);
  const bool media_turn = !media.Empty();
  auto turn_tool_ctx = BuildToolCallContext(request);
  ValidateMediaTurn(media, inputs,
                    {.session_has_history = !transcript_.Empty(), .tools_declared = turn_tool_ctx.HasTools()});

  // Merge session-level and per-request options once for this turn.
  auto effective_kvp = MergedOptions(request.options);
  SearchOptions effective_options = SearchOptions::FromParameters(effective_kvp);

  // Request-scoped system prefix. It is not conversation history: it never enters the transcript, so it cannot
  // accumulate a copy per turn, and the value this request carries is the only one used. It is baked into a
  // generator's prompt, so a changed prefix has to rebuild while an unchanged one keeps the KV cache.
  const std::string turn_system_prompt = GetOptionOrEmpty(effective_kvp, kSystemPromptOption);

  // One gate for every way a turn can carry meaning: its own messages, media bytes, the conversation behind it, or
  // the instructions in front of it. A turn with none of them is the caller's mistake, so it is a client error —
  // and a turn with any of them generates, whether the conversation is held in this session or was replayed into
  // the request after the cache dropped it.
  if (!TurnCanGenerate(inputs, {.media = media_turn,
                                .history = !transcript_.Empty(),
                                .system_prefix = !turn_system_prompt.empty()})) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT,
             "the request has nothing to generate from: no message content, no image or audio, no instructions, "
             "and no conversation to continue");
  }

  int prompt_tokens = 0;
  // Empty until this turn's input is appended to an existing generator. A rebuilt generator bakes the input into its
  // prompt, so there is no pre-turn boundary that undo could rewind back to.
  std::optional<int> pre_turn_token_count;

  // A changed system prefix, an empty continuation, or tool activity all require rebuilding from complete history.
  // None can be appended as an independent suffix that preserves the prompt the model should see.
  if (cached_generator_ &&
      (turn_system_prompt != system_prompt_ || !cached_tool_ctx_.HasSameTools(turn_tool_ctx) || inputs.empty() ||
       CarriesToolActivity(inputs))) {
    InvalidateCachedGenerator();
  }

  if (cached_generator_) {
    // Check if guidance requirements changed since the generator was created. Guidance (LARK grammar) is baked into
    // the OGA generator at creation time and cannot be changed. If tool_choice went from "required" to "auto" (or
    // vice versa), we must recreate the generator from the full transcript.
    auto guidance_ctx = cached_tool_ctx_;
    UpdateToolContextForTurn(request, guidance_ctx);

    bool prev_has_user_guidance = !cached_tool_ctx_.guidance_type.empty() && !cached_tool_ctx_.guidance_data.empty();
    bool curr_has_user_guidance = !guidance_ctx.guidance_type.empty() && !guidance_ctx.guidance_data.empty();
    bool prev_needs_guidance = prev_has_user_guidance || (cached_tool_ctx_.tool_output && !cached_tool_ctx_.text_output);
    bool curr_needs_guidance = curr_has_user_guidance || (guidance_ctx.tool_output && !guidance_ctx.text_output);

    // Rebuild when: guidance requirements changed OR the previous turn had user-specified guidance
    // (the finite grammar may have completed, causing IsDone() to return true on the next turn,
    // and switching schemas requires a fresh grammar).
    if (prev_needs_guidance != curr_needs_guidance || prev_has_user_guidance) {
      InvalidateCachedGenerator();
    }
  }

  // Past this point the cached generator gets this turn's input appended or is created for it. Any exit before the
  // commit below leaves its KV cache holding tokens the transcript knows nothing about, so the guard drops it and the
  // next turn rebuilds deterministically from committed history. Cancellation of an appended turn is the one settled
  // case that can be rewound instead, and dismisses the guard explicitly below.
  bool turn_committed = false;
  ScopeGuard invalidate_uncommitted([&]() noexcept {
    if (!turn_committed) {
      InvalidateCachedGenerator();
    }
  });

  if (cached_generator_) {
    // Continuous decoding: append only the new messages to the existing generator.
    pre_turn_token_count = cached_generator_->TokenCount();
    cached_generator_->AppendMessages(inputs, Model(), cached_tool_ctx_.tools_json);
    // Public usage describes the complete logical prompt, not only the suffix appended to the KV cache. Taking the
    // count after AppendMessages also makes total - prompt exactly this turn's generated token count.
    prompt_tokens = cached_generator_->TokenCount();

    // Refresh per-turn fields (tool_choice, guidance) while keeping session-level definitions stable.
    UpdateToolContextForTurn(request, cached_tool_ctx_);
  } else {
    // First request (or cache invalidated): create the generator from scratch.
    auto tool_ctx = std::move(turn_tool_ctx);

    std::unique_ptr<OnnxChatGenerator> generator;
    if (media_turn) {
      // Media is single-shot: the generator is dropped after the turn (see
      // the cleanup below) because AppendMessages can't extend a sequence
      // whose state includes image-derived tokens. Sizing the KV cache to
      // the model's full context window would needlessly allocate gigabytes
      // (262k tokens × 28 layers × 8 heads × 128 dims for qwen3-vl-2b
      // ≈ 120 GB). Bound it to prompt + max_output_tokens.
      //
      // The system prefix is prepended as an ordinary message so a media turn honours instructions the same way a
      // text turn does. The validation above guarantees there is no prior history to carry.
      auto media_messages = std::move(media.messages);
      if (!turn_system_prompt.empty()) {
        media_messages.insert(media_messages.begin(), MessageItem(FOUNDRY_LOCAL_ROLE_SYSTEM, turn_system_prompt));
      }

      generator = OnnxChatGenerator::CreateWithMedia(media_messages, effective_options, Model(), media.images,
                                                     media.audios, tool_ctx, /*use_full_context*/ false);
    } else {
      // Combine the committed transcript with this turn's input for the full context, behind the request's system
      // prefix. The prefix is rebuilt from request state every time rather than replayed from history.
      std::vector<TranscriptMessage> all_messages;
      const auto& committed = transcript_.Messages();
      all_messages.reserve(committed.size() + inputs.size());
      all_messages.insert(all_messages.end(), committed.begin(), committed.end());
      all_messages.insert(all_messages.end(), inputs.begin(), inputs.end());

      generator = OnnxChatGenerator::Create(WithSystemPrompt(turn_system_prompt, std::move(all_messages)),
                                            effective_options, Model(), tool_ctx,
                                            /*use_full_context*/ true);
    }
    prompt_tokens = generator->PromptTokenCount();

    cached_generator_ = std::move(generator);
    cached_tool_ctx_ = std::move(tool_ctx);
    // Records the prefix baked into the generator above, so the next turn can tell whether it changed.
    system_prompt_ = turn_system_prompt;
  }

  int max_output = effective_options.max_output_tokens.value_or(0);

  // Generate token-by-token with optional streaming.
  // Check request.canceled each iteration — a streaming callback returning
  // non-zero sets this flag asynchronously via CallbackHandler.
  auto streaming_callback = CreateCallbackHandler(request);
  int output_tokens = 0;
  std::vector<GeneratedOutputEvent> generated_events;

  // The assistant-turn ordering invariant (see AssistantTurnGuard), enforced while the turn is produced rather than
  // after it has been streamed. A prefill the reply will merge into is part of the same assistant turn, so its calls
  // close this turn's visible text too.
  auto turn_guard = AssistantTurnGuard::ForReplyTo(inputs, ingest.last_segment_start);

  // Marker IDs are derived from the configured strings with the model tokenizer. This detects special markers even
  // when their decoded chunks are empty, while non-reasoning models retain the DEFAULT passthrough. The splitter is
  // seeded from the prompt so a template that already opened a reasoning block does not have its scratchpad
  // reported as visible text.
  auto splitter = CreateReasoningSplitter(cached_tool_ctx_, Model(), cached_generator_->PromptOpensReasoning());

  // Accumulator: separates visible text from tool-call blocks in the DEFAULT-segment stream. For models without
  // tool-call markers configured, both marker strings are empty and the accumulator degrades to passthrough.
  // REASONING segments bypass the accumulator entirely — tool-call-shaped text inside <think>...</think> is the
  // model's scratchpad and is not a real tool call.
  ToolCallStreamAccumulator tool_accumulator(
      cached_tool_ctx_.tool_output ? cached_tool_ctx_.tool_call_start : std::string{},
      cached_tool_ctx_.tool_output ? cached_tool_ctx_.tool_call_end : std::string{});

  auto emit_tool_output = [&](ToolCallStreamAccumulator::Output out) {
    for (auto& event : out.events) {
      if (auto* text = std::get_if<std::string>(&event)) {
        // A structured tool call is the end of the turn's visible text: a chat template renders an assistant turn as
        // content followed by its tool calls, so text produced after a call could only be replayed as though it came
        // before one. Ending the turn here means the caller is never shown output the conversation cannot keep.
        if (!AcceptVisibleText(turn_guard, *text, logger_)) {
          continue;
        }

        AppendGeneratedSegment(generated_events, *text, FOUNDRY_LOCAL_TEXT_ITEM_TYPE_DEFAULT);
        if (streaming_callback) {
          streaming_callback->PushItem(
              std::make_unique<TextItem>(*text, FOUNDRY_LOCAL_TEXT_ITEM_TYPE_DEFAULT));
        }
        continue;
      }

      auto call = std::move(std::get<ParsedToolCall>(event));
      turn_guard.RecordToolCall();
      if (streaming_callback) {
        streaming_callback->PushItem(std::make_unique<ToolCallItem>(call.id, call.name, call.arguments));
      }
      generated_events.push_back(std::move(call));
    }
  };

  auto emit_segments = [&](const std::vector<ReasoningStreamSplitter::Segment>& segments) {
    for (const auto& seg : segments) {
      if (seg.type != FOUNDRY_LOCAL_TEXT_ITEM_TYPE_DEFAULT) {
        // Release a visible prefix held as a potential tool marker before appending later reasoning.
        if (!tool_accumulator.InsideToolCall()) {
          emit_tool_output(tool_accumulator.Flush());
        }
        // REASONING goes straight through — never feed it to the tool-call accumulator.
        AppendGeneratedSegment(generated_events, seg.text, seg.type);
        if (streaming_callback) {
          streaming_callback->PushItem(std::make_unique<TextItem>(seg.text, seg.type));
        }
        continue;
      }

      emit_tool_output(tool_accumulator.Push(seg.text));
    }
  };

  auto flush_accumulator = [&]() { emit_tool_output(tool_accumulator.Flush()); };

  while (!cached_generator_->IsDone() && !request.canceled && !turn_guard.TurnEnded()) {
    cached_generator_->GenerateNextToken();
    const auto token_id = cached_generator_->CurrentTokenId();
    std::string token = cached_generator_->Decode();
    ++output_tokens;

    if (token_id.has_value()) {
      emit_segments(splitter.Push(*token_id, std::move(token)));
    } else if (!token.empty()) {
      emit_segments(splitter.Push(token));
    }

    // Enforce max_output_tokens — with use_full_context the OGA max_length
    // is the entire context window, so we must cap output ourselves.
    if (max_output > 0 && output_tokens >= max_output) {
      break;
    }
  }

  // End-of-stream: drain the reasoning splitter first so any final DEFAULT bytes feed into the tool accumulator,
  // then drain the tool accumulator.
  emit_segments(splitter.Flush());
  flush_accumulator();

  // Callback delivery is asynchronous. Drain it before the final cancellation check and commit decision so a callback
  // that rejects the last queued item cannot arrive after this turn has already changed the transcript.
  if (streaming_callback) {
    streaming_callback->Drain();
  }

  int total_tokens = cached_generator_->TokenCount();

  auto assistant_message = MakeAssistantMessage(generated_events, logger_);
  const bool generated_tool_calls = assistant_message.HasToolCalls();

  if (!request.canceled) {
    // Reject a generation whose calls cannot be correlated before it reaches the caller — a committed turn must never
    // leave the outstanding-call set inconsistent.
    transcript_.ValidateGeneratedOutput(assistant_message);
  }

  ProcessGeneratedOutput(std::move(generated_events), effective_options, request.canceled, response,
                         prompt_tokens, total_tokens, splitter.ReasoningTokenCount());

  if (request.canceled) {
    // A cancelled turn commits nothing. Cancellation is a settled stop between tokens, so a turn that was appended to
    // an existing generator can be rewound to the boundary it started from and the generator stays usable for a
    // retry. A generator created or rebuilt for this turn has no such boundary — the turn's input is part of its
    // prompt — so the guard drops it and the next turn rebuilds from the committed transcript. If the rewind itself
    // fails, the guard has not been dismissed yet and still invalidates.
    if (pre_turn_token_count.has_value()) {
      cached_generator_->RewindTo(*pre_turn_token_count);
      invalidate_uncommitted.Dismiss();
    }

    return;
  }

  // LARK grammar (tool-call-only mode) is a single-shot finite parse. If generation was truncated while grammar was
  // active, the parser is in an unrecoverable state. Additionally, a completed grammar signals EOS — IsDone() would
  // return true on the next turn. Invalidate after any grammar-guided generation so the next turn rebuilds.
  //
  // Reasoning models (qwen3, etc.) also need invalidation: continuous decoding leaves prior <think> tokens in the KV
  // cache and the model fails to close subsequent reasoning blocks. The template projection does not replay reasoning
  // from plain text turns, so a rebuild restores correct behavior. This matches C#, which always applies the full
  // template per turn.
  //
  // Media is single-shot too: AppendMessages cannot extend a media-decoded sequence, so any text follow-up must
  // rebuild rather than silently feed text into a state that includes media-derived tokens.
  //
  // A turn stopped at a tool call because the model kept talking afterwards is truncated mid-sequence with no turn
  // terminator, so its KV cache is no basis for the next turn either.
  bool grammar_was_active = cached_tool_ctx_.tool_output && !cached_tool_ctx_.text_output;
  bool reasoning_was_active = cached_tool_ctx_.supports_reasoning;

  // The reply may only merge into an input message from the last replay segment — this request's own input. Merging
  // into an earlier hop's assistant message would glue two recorded turns together.
  transcript_.CommitTurn(std::move(inputs), std::move(assistant_message),
                         {pre_turn_token_count, total_tokens}, ingest.last_segment_start);
  turn_committed = true;

  if (grammar_was_active || reasoning_was_active || media_turn || generated_tool_calls || turn_guard.TurnEnded()) {
    InvalidateCachedGenerator();
  }
}

void ChatSession::ProcessChatCompletionsJson(const std::string& request_json, const Request& original_request,
                                             Response& response) {
  // Parse the OpenAI chat completions request
  auto req_json = nlohmann::json::parse(request_json);
  auto req = req_json.get<ChatCompletionRequest>();

  // Apply catalog defaults passed via request options
  chat_completions::ApplyCatalogDefaults(req, CatalogModel().Info().model_settings);

  std::string model_name = req.model;
  std::string completion_id = chat_completions::GenerateCompletionId();
  auto now = std::chrono::system_clock::now();
  int64_t created = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

  // Build the internal request from the chat completions request
  Request internal_request;

  // We don't use history_ for this request as it's for backwards compat and all messages come from the input.
  chat_completions::BuildRequestItems(req, internal_request);
  if (internal_request.items.empty()) {
    FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_ARGUMENT,
             "the request has nothing to generate from: `messages` carried no content");
  }

  std::string tools_json = chat_completions::ExtractToolDefinitions(req, internal_request);
  chat_completions::MapRequestParameters(req, internal_request);
  chat_completions::MapGuidance(req, internal_request);
  chat_completions::MapStopSequences(req, internal_request);

  // Merge options from the original request (e.g. tool_call_start/end from model properties)
  for (const auto& [key, value] : original_request.options) {
    if (internal_request.options.find(key) == internal_request.options.end()) {
      internal_request.options[key] = value;
    }
  }

  // Build tool call context
  if (!tools_json.empty()) {
    // we don't expect a Session to get re-used on this path so this should always be empty
    if (ToolDefinitions().size() > 0) {
      FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_USAGE,
               "Tool definitions cannot be used with OpenAI JSON input; the JSON payload must be fully self-contained");
    }

    AddToolDefinition({{}, {}, std::move(tools_json)});
  }

  auto tool_ctx = BuildToolCallContext(internal_request);

  // Merge session-level and per-request options once.
  auto effective_kvp = MergedOptions(internal_request.options);
  SearchOptions options = SearchOptions::FromParameters(effective_kvp);

  // Collect transcript messages from the internal request for the generator.
  // The session transcript is not used here — everything comes from the parsed JSON input.
  auto messages = BuildTranscriptMessages(internal_request.items);

  // The payload is self-contained, so correlate its tool calls and results against an empty transcript. This gives
  // the same stable errors a session turn would produce for a history the model cannot interpret.
  const ChatTranscript payload_transcript;
  payload_transcript.ValidateInputs(messages);

  // Create generator
  auto generator = OnnxChatGenerator::Create(messages, options, Model(), tool_ctx);
  int prompt_tokens = generator->PromptTokenCount();

  auto streaming_callback = CreateCallbackHandler(original_request);
  bool is_streaming = (streaming_callback != nullptr);

  // Emit initial streaming chunk
  if (is_streaming) {
    auto initial_json = chat_completions::FormatInitialStreamingChunk(completion_id, created, model_name);
    streaming_callback->PushItem(std::make_unique<TextItem>(std::move(initial_json),
                                                            FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON));
  }

  // Tool-call accumulator: parses tool-call blocks out of the visible stream. Empty markers degrade to passthrough.
  // Replaces a prior inline accumulator that did exact per-segment marker matching — that only worked because the
  // qwen tokenizer happens to emit `<tool_call>` as a single special token. Tokenizers that split the marker across
  // multiple tokens (or chat templates that produce marker-shaped text gradually) would silently fail. The shared
  // accumulator buffers across tokens and is verified by unit tests.
  ToolCallStreamAccumulator tool_accumulator(tool_ctx.tool_output ? tool_ctx.tool_call_start : std::string{},
                                             tool_ctx.tool_output ? tool_ctx.tool_call_end : std::string{});

  int next_tool_call_index = 0;
  std::vector<GeneratedOutputEvent> generated_events;

  // A trailing assistant message is a prefill. If it already carries a tool call, visible text generated after it
  // would be merged into that same turn when the client replays the response, so seed the same ordering guard used
  // by the stateful path.
  auto turn_guard = AssistantTurnGuard::ForReplyTo(messages, 0);

  // Use the same typed segments for streaming and final response construction.
  auto splitter = CreateReasoningSplitter(tool_ctx, Model(), generator->PromptOpensReasoning());

  auto emit_visible_text = [&](std::string visible) {
    if (visible.empty() || !is_streaming) {
      return;
    }

    auto chunk_json = chat_completions::FormatStreamingChunk(visible, completion_id, created, model_name);
    streaming_callback->PushItem(std::make_unique<TextItem>(std::move(chunk_json),
                                                            FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON));
  };

  auto process_tool_output = [&](ToolCallStreamAccumulator::Output out) {
    for (auto& event : out.events) {
      if (auto* text = std::get_if<std::string>(&event)) {
        // A chat completion carries the assistant reply as `content` plus a `tool_calls` array — the same schema the
        // transcript projection uses, with the same limitation. Text produced after a call would be reported in
        // `content`, in front of the call that actually came first, and a client replaying that message back would
        // be replaying an order of events that never happened. End the turn at the call instead.
        if (!AcceptVisibleText(turn_guard, *text, logger_)) {
          continue;
        }

        AppendGeneratedSegment(generated_events, *text, FOUNDRY_LOCAL_TEXT_ITEM_TYPE_DEFAULT);
        emit_visible_text(*text);
        continue;
      }

      auto call = std::move(std::get<ParsedToolCall>(event));
      turn_guard.RecordToolCall();
      if (is_streaming) {
        ChatCompletionToolCall streamed;
        streamed.index = next_tool_call_index++;
        streamed.id = call.id;
        streamed.type = "function";
        streamed.function.name = call.name;
        streamed.function.arguments = call.arguments;
        auto chunk_json = chat_completions::FormatToolCallStreamingChunk(
            {streamed}, completion_id, created, model_name);
        streaming_callback->PushItem(std::make_unique<TextItem>(
            std::move(chunk_json), FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON));
      }
      generated_events.push_back(std::move(call));
    }
  };

  auto process_segments = [&](const std::vector<ReasoningStreamSplitter::Segment>& segments) {
    for (const auto& seg : segments) {
      // REASONING segments: never feed reasoning text to the tool-call accumulator — tool-call-shaped text inside
      // <think>...</think> is scratchpad, not a real call. Emit via reasoning_content, not content.
      if (seg.type != FOUNDRY_LOCAL_TEXT_ITEM_TYPE_DEFAULT) {
        if (!tool_accumulator.InsideToolCall()) {
          process_tool_output(tool_accumulator.Flush());
        }
        AppendGeneratedSegment(generated_events, seg.text, seg.type);

        if (is_streaming && !seg.text.empty()) {
          auto chunk_json = chat_completions::FormatReasoningStreamingChunk(
              seg.text, completion_id, created, model_name);
          streaming_callback->PushItem(std::make_unique<TextItem>(
              std::move(chunk_json), FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON));
        }
        continue;
      }

      process_tool_output(tool_accumulator.Push(seg.text));
    }
  };

  // Generate token-by-token.
  while (!generator->IsDone() && !original_request.canceled && !turn_guard.TurnEnded()) {
    generator->GenerateNextToken();
    const auto token_id = generator->CurrentTokenId();
    std::string token = generator->Decode();

    if (token_id.has_value()) {
      process_segments(splitter.Push(*token_id, std::move(token)));
    } else if (!token.empty()) {
      process_segments(splitter.Push(token));
    }
  }

  // Drain any buffered partial-marker bytes at end-of-stream. Reasoning splitter first so any final DEFAULT bytes
  // feed into the tool accumulator; then drain the tool accumulator.
  process_segments(splitter.Flush());
  process_tool_output(tool_accumulator.Flush());

  int total_tokens = generator->TokenCount();

  ProcessGeneratedOutput(std::move(generated_events), options, original_request.canceled, response,
                         prompt_tokens, total_tokens, splitter.ReasoningTokenCount());

  // Emit final streaming chunk with finish_reason
  if (is_streaming) {
    auto final_json = chat_completions::FormatFinalStreamingChunk(response.finish_reason, completion_id, created,
                                                                  model_name);
    streaming_callback->PushItem(std::make_unique<TextItem>(std::move(final_json),
                                                            FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON));
  }

  // Store completion envelope metadata so callers can access without parsing JSON
  response.metadata["completion_id"] = completion_id;
  response.metadata["created"] = std::to_string(created);
  response.metadata["model"] = model_name;

  // Build the ChatCompletionResponse and replace response items with a single OPENAI_JSON-tagged TextItem.
  auto chat_response = chat_completions::BuildResponse(response, completion_id, created, model_name);
  response.items.clear();
  response.items.push_back(std::make_unique<TextItem>(nlohmann::json(chat_response).dump(),
                                                      FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON));
}

const ChatTranscript& ChatSession::Transcript() const {
  return transcript_;
}

void ChatSession::InvalidateCachedGenerator() noexcept {
  cached_generator_.reset();
  cached_tool_ctx_ = {};
  // The prefix describes the dropped generator's prompt; the next turn supplies its own.
  system_prompt_.clear();
}

size_t ChatSession::TurnCount() const {
  return transcript_.TurnCount();
}

void ChatSession::UndoTurns(size_t count) {
  auto request_lock = LockRequestMutex();

  if (count == 0) {
    return;
  }

  const bool undo_all = count == transcript_.TurnCount();

  // The transcript validates `count` and rolls back messages, turn records, and outstanding-call state as one step.
  auto tokens = transcript_.UndoTurns(count);

  if (!cached_generator_) {
    return;
  }

  if (undo_all || !tokens.pre_turn.has_value()) {
    // Undoing every turn, or undoing back to a turn whose generator was rebuilt — in both cases the current KV cache
    // has no boundary matching the target state, so drop it and let the next turn rebuild.
    InvalidateCachedGenerator();
    return;
  }

  // The transcript is already truncated. A failed rewind would leave the KV cache describing a conversation the
  // transcript no longer has, so drop the generator before the failure propagates.
  ScopeGuard invalidate_on_failed_rewind([this]() noexcept { InvalidateCachedGenerator(); });
  cached_generator_->RewindTo(*tokens.pre_turn);
  invalidate_on_failed_rewind.Dismiss();
}

size_t ChatSession::MessageCount() const {
  return transcript_.MessageCount();
}

}  // namespace fl
