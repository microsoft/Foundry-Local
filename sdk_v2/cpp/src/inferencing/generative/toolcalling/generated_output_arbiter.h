// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "inferencing/generative/toolcalling/tool_call_context.h"
#include "inferencing/generative/toolcalling/tool_call_utils.h"
#include "inferencing/session/tool_registry.h"
#include "inferencing/session/types.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace fl {

/// The one tool whose call a model is allowed to express as a bare patch envelope in its visible output instead of
/// as a structured tool-call block. The name is matched exactly; nothing about the envelope identifies the tool, so
/// the turn's own tool set is what gives the envelope a meaning (see GeneratedOutputArbiter).
inline constexpr std::string_view kApplyPatchToolName = "apply_patch";

/// Opening line of a raw patch envelope, without its line terminator.
inline constexpr std::string_view kPatchBeginLine = "*** Begin Patch";

/// Closing line of a raw patch envelope, without its line terminator.
inline constexpr std::string_view kPatchEndLine = "*** End Patch";

/// Request-scoped state machine that decides what every byte of a turn's generated output *is*.
///
/// A generative chat model expresses a tool call in one of two shapes, and both arrive inline in the same token
/// stream as ordinary prose:
///
///   1. A structured block wrapped in the model's marker tokens, e.g. `<tool_call>{"name":…}</tool_call>`.
///   2. A bare patch envelope — `*** Begin Patch` … `*** End Patch` — written directly into visible output. Models
///      trained on that convention emit it in place of a structured call for the `apply_patch` tool.
///
/// Both readings compete for the same bytes, so they are arbitrated by one state machine rather than by chained
/// parsers. Chaining is not merely redundant here, it is wrong: a first-pass patch scanner would carve an envelope
/// out of a structured call's JSON payload, and a first-pass structured scanner would carve a `<tool_call>` out of
/// patch data. Arbitrating once means whichever shape opens first owns every byte until it closes, and the shape
/// that lost never sees those bytes at all.
///
/// State, in stream order:
///
///   kVisible    — assistant prose. Scans for whichever opener occurs earliest. Bytes that could still grow into
///                 either opener are held back rather than emitted, so a marker split across tokens is never leaked.
///   kStructured — inside a marker block. Every byte belongs to the block, including `*** Begin Patch`.
///   kRawPatch   — inside a patch envelope. Every byte is patch data, including `<tool_call>`.
///
/// Raw envelope grammar, enforced exactly:
///
///   - The begin line is exactly `*** Begin Patch`, starting at stream offset 0 or immediately after an LF, and
///     followed by LF or CRLF. Its terminator is part of the envelope.
///   - The end line is exactly `*** End Patch`, starting at a line boundary, and followed by LF, CRLF, or
///     end-of-stream. The closing marker is part of the envelope; the newline after it is visible output, because
///     it separates the call from whatever the model writes next.
///   - Everything between is opaque payload: no unescaping, no newline normalization, no trimming. An indented,
///     suffixed, misspelled, reversed, or unterminated marker is not a marker and stays visible.
///
/// `Push(chunk)` accepts any text chunk — a single decoded token or a multi-token segment from the upstream
/// `ReasoningStreamSplitter` — and returns ordered events: visible text, and tool calls whose envelope completed in
/// this chunk. A call's ID is minted exactly once, at the moment its envelope commits, and is what the streamed
/// item, the final response, the transcript, and the client's eventual result all correlate through.
///
/// `Flush()` drains end-of-stream: a natural stop, an output-token limit, a cancellation, or a failed turn. Every
/// buffered byte comes back as visible text, so an envelope that never closed is reported exactly as generated and
/// is never actionable.
///
/// Callers must not feed REASONING-tagged content into `Push`. Reasoning is the model's scratchpad; a marker inside
/// it is not a call of either shape. The upstream splitter routes REASONING segments around this arbiter entirely.
class GeneratedOutputArbiter {
 public:
  using Event = std::variant<std::string, ParsedToolCall>;

  struct Output {
    std::vector<Event> events;
  };

  /// Build the arbiter for one turn from the context that shaped that turn's prompt.
  ///
  /// Everything the arbiter needs is copied, not referenced: the context belongs to the session and a turn must be
  /// read back with the tool set it was prompted with even if another thread re-registers tools meanwhile.
  ///
  /// Raw patch mode is enabled only when this turn actually offered `apply_patch` as a custom tool. `tool_output`
  /// covers `tool_choice: "none"`, and the kinds map has already been narrowed by forced-tool and allowed-tool
  /// filtering, so a turn that excluded `apply_patch` reads an envelope as the prose it is.
  explicit GeneratedOutputArbiter(const ToolCallContext& context)
      : start_marker_(context.tool_output ? context.tool_call_start : std::string{}),
        end_marker_(context.tool_output ? context.tool_call_end : std::string{}),
        tool_kinds_(context.tool_kinds),
        raw_patch_enabled_(context.tool_output && context.IsCustomTool(std::string(kApplyPatchToolName))) {}

  /// Feed a chunk of DEFAULT-typed generated text. Returns ordered visible-text and completed-call events.
  Output Push(const std::string& chunk) {
    Output out;

    if (chunk.empty()) {
      return out;
    }

    if (!DetectionEnabled()) {
      EmitVisible(out, chunk);
      return out;
    }

    buffer_ += chunk;
    Drain(out, /*flushing=*/false);

    return out;
  }

  /// Drain at end-of-stream. Anything still buffered — a partial marker, an unterminated structured block, an
  /// unterminated patch envelope — is emitted as visible text, byte for byte.
  Output Flush() {
    Output out;

    if (!DetectionEnabled()) {
      return out;
    }

    Drain(out, /*flushing=*/true);

    return out;
  }

  /// Whether an envelope of either shape is currently open and owns incoming bytes.
  bool InsideCall() const noexcept { return state_ != State::kVisible; }

  /// Whether a structured marker block is currently open.
  bool InsideStructuredCall() const noexcept { return state_ == State::kStructured; }

  /// Whether a raw patch envelope is currently open.
  bool InsideRawPatch() const noexcept { return state_ == State::kRawPatch; }

  /// Whether this turn's tool set makes a bare patch envelope an `apply_patch` call.
  bool RawPatchEnabled() const noexcept { return raw_patch_enabled_; }

 private:
  enum class State {
    kVisible,
    kStructured,
    kRawPatch,
  };

  /// Where a line-anchored marker line was found, or where one might still begin.
  struct LineMarkerScan {
    /// Earliest candidate position, or npos when no position in the scanned text could be one.
    size_t index = std::string::npos;
    /// Bytes the complete marker line occupies from `index`, terminator included. Meaningless unless `complete`.
    size_t length = 0;
    /// False when the candidate needs more bytes before it can be accepted or rejected.
    bool complete = false;
  };

  bool StructuredDetectionEnabled() const noexcept { return !start_marker_.empty() && !end_marker_.empty(); }

  bool DetectionEnabled() const noexcept { return StructuredDetectionEnabled() || raw_patch_enabled_; }

  bool IsCustomTool(const std::string& name) const {
    auto it = tool_kinds_.find(name);
    return it != tool_kinds_.end() && it->second == ToolKind::kCustom;
  }

  /// Run the state machine until it stops making progress. Each step either consumes buffered bytes or changes
  /// state, so a step that does neither means the machine is waiting for more input (or is fully drained).
  void Drain(Output& out, bool flushing) {
    for (;;) {
      const auto state_before = state_;
      const auto buffered_before = buffer_.size();

      switch (state_) {
        case State::kVisible:
          StepVisible(out, flushing);
          break;
        case State::kStructured:
          StepStructured(out, flushing);
          break;
        case State::kRawPatch:
          StepRawPatch(out, flushing);
          break;
      }

      if (state_ == state_before && buffer_.size() == buffered_before) {
        return;
      }
    }
  }

  /// Outside any envelope: emit prose, and open whichever envelope starts earliest.
  void StepVisible(Output& out, bool flushing) {
    const size_t structured_index = StructuredDetectionEnabled() ? buffer_.find(start_marker_) : std::string::npos;
    const LineMarkerScan raw = raw_patch_enabled_
                                   ? ScanForMarkerLine(buffer_, at_line_start_, kPatchBeginLine,
                                                       /*eof_terminates=*/false)
                                   : LineMarkerScan{};

    // Earliest position whose reading is still undecided. Bytes from here on must not be emitted: more input could
    // turn them into an opener. At end-of-stream nothing is undecided — no more input is coming.
    size_t undecided = std::string::npos;

    if (!flushing) {
      // Only a marker that has not already occurred can still arrive: a complete occurrence always starts at or
      // before any partial suffix, so opening at it settles the suffix too.
      if (StructuredDetectionEnabled() && structured_index == std::string::npos) {
        const auto hold = LongestSuffixThatIsPrefixOf(buffer_, start_marker_);
        if (hold > 0) {
          undecided = buffer_.size() - hold;
        }
      }

      if (raw.index != std::string::npos && !raw.complete) {
        undecided = std::min(undecided, raw.index);
      }
    }

    // A raw envelope at or before the structured marker wins: whichever opener comes first owns the rest.
    const bool raw_wins = raw.complete && (structured_index == std::string::npos || raw.index <= structured_index);
    const size_t open_index = raw_wins ? raw.index : structured_index;

    // An opener inside the undecided region is not yet safe to act on — a longer, earlier opener could still
    // swallow it. Wait for the bytes that settle it.
    if (open_index != std::string::npos && (undecided == std::string::npos || open_index < undecided)) {
      EmitVisible(out, Consume(open_index));

      if (raw_wins) {
        // The begin line's terminator is part of the envelope the tool receives.
        patch_buffer_ = Consume(raw.length);
        state_ = State::kRawPatch;
      } else {
        // ParseToolCalls is handed the full `<tool_call>…</tool_call>` substring, so keep the opening marker.
        structured_buffer_ = Consume(start_marker_.size());
        state_ = State::kStructured;
      }

      return;
    }

    EmitVisible(out, Consume(undecided == std::string::npos ? buffer_.size() : undecided));
  }

  /// Inside a structured marker block: every byte belongs to the block until the closing marker.
  void StepStructured(Output& out, bool flushing) {
    const size_t found = buffer_.find(end_marker_);

    if (found != std::string::npos) {
      structured_buffer_ += Consume(found + end_marker_.size());

      auto parsed = ParseToolCalls(structured_buffer_, start_marker_, end_marker_);

      if (parsed.empty()) {
        // A marker-shaped block that does not parse is model prose, not a call. Preserve it rather than dropping
        // generated output on the floor.
        EmitVisible(out, structured_buffer_);
      } else {
        for (auto& call : parsed) {
          // A custom tool's payload crosses the API boundary as raw text, not as the synthesized `{"input": …}`
          // wrapper the model was prompted with. Unwrapping here — once, for both the native and the Chat JSON
          // path — is what makes the stream, the final response, and the transcript agree on the same bytes.
          if (IsCustomTool(call.name)) {
            call.arguments = ExtractCustomToolInput(call.arguments);
          }

          out.events.emplace_back(std::move(call));
        }
      }

      structured_buffer_.clear();
      state_ = State::kVisible;
      return;
    }

    if (flushing) {
      EmitVisible(out, TakeBufferedWith(structured_buffer_));
      state_ = State::kVisible;
      return;
    }

    structured_buffer_ += Consume(buffer_.size() - LongestSuffixThatIsPrefixOf(buffer_, end_marker_));
  }

  /// Inside a raw patch envelope: every byte is payload until a line that is exactly the closing marker.
  void StepRawPatch(Output& out, bool flushing) {
    // End-of-stream is a valid terminator for the closing line, so a model that stops right after `*** End Patch`
    // still produces a call.
    const LineMarkerScan end = ScanForMarkerLine(buffer_, at_line_start_, kPatchEndLine, /*eof_terminates=*/flushing);

    if (end.complete) {
      // Take the closing marker but not its terminator: the newline after `*** End Patch` separates the call from
      // the model's next visible output and belongs to that output.
      patch_buffer_ += Consume(end.index + kPatchEndLine.size());

      out.events.emplace_back(ParsedToolCall{GenerateToolCallId(), std::string(kApplyPatchToolName),
                                             std::move(patch_buffer_)});
      patch_buffer_.clear();
      state_ = State::kVisible;
      return;
    }

    if (flushing) {
      // An envelope that never closed is not a call. Every byte of it is reported as generated.
      EmitVisible(out, TakeBufferedWith(patch_buffer_));
      state_ = State::kVisible;
      return;
    }

    const size_t hold = end.index == std::string::npos ? 0 : buffer_.size() - end.index;
    patch_buffer_ += Consume(buffer_.size() - hold);
  }

  /// Take `count` bytes off the front of the scan buffer, keeping the line-boundary tracker in step.
  std::string Consume(size_t count) {
    std::string consumed = buffer_.substr(0, count);
    buffer_.erase(0, count);

    if (!consumed.empty()) {
      at_line_start_ = consumed.back() == '\n';
    }

    return consumed;
  }

  /// Concatenate an in-progress envelope with everything still buffered, clearing both.
  std::string TakeBufferedWith(std::string& envelope) {
    std::string text = std::move(envelope);
    envelope.clear();
    text += Consume(buffer_.size());
    return text;
  }

  /// Index just past the next LF at or after `from`, or npos when there is none. A line start at the very end of
  /// `text` is a real candidate: an empty tail can still grow into a marker.
  static size_t NextLineStart(std::string_view text, size_t from) {
    const auto newline = text.find('\n', from);
    return newline == std::string_view::npos ? std::string::npos : newline + 1;
  }

  /// Find the earliest position at which `line` stands alone as a complete line, or could still become one.
  ///
  /// A candidate must start at a line boundary and be followed by LF, CRLF, or — when `eof_terminates` — the end of
  /// the scanned text. Any other trailing byte suffixes the line, which makes it ordinary text and moves the scan
  /// to the next line. `complete == false` with a valid `index` means the text ran out mid-candidate and the caller
  /// must hold those bytes until more arrive.
  static LineMarkerScan ScanForMarkerLine(std::string_view text, bool first_is_line_start, std::string_view line,
                                          bool eof_terminates) {
    for (size_t pos = first_is_line_start ? 0 : NextLineStart(text, 0); pos != std::string::npos;
         pos = NextLineStart(text, pos)) {
      const size_t available = text.size() - pos;
      const size_t comparable = std::min(available, line.size());

      if (text.compare(pos, comparable, line.substr(0, comparable)) != 0) {
        continue;
      }

      if (comparable < line.size()) {
        return {pos, 0, false};
      }

      const size_t after = pos + line.size();

      if (after == text.size()) {
        if (eof_terminates) {
          return {pos, line.size(), true};
        }

        return {pos, 0, false};
      }

      if (text[after] == '\n') {
        return {pos, line.size() + 1, true};
      }

      if (text[after] == '\r') {
        if (after + 1 == text.size()) {
          // A bare CR at end-of-stream suffixes the line; mid-stream it may still become CRLF.
          if (!eof_terminates) {
            return {pos, 0, false};
          }
        } else if (text[after + 1] == '\n') {
          return {pos, line.size() + 2, true};
        }
      }
    }

    return {};
  }

  static void EmitVisible(Output& out, std::string text) {
    if (text.empty()) {
      return;
    }

    if (!out.events.empty()) {
      if (auto* previous = std::get_if<std::string>(&out.events.back())) {
        *previous += text;
        return;
      }
    }

    out.events.emplace_back(std::move(text));
  }

  /// Length of the longest suffix of `s` that is also a prefix of `m`. O(min(|s|, |m|)).
  static size_t LongestSuffixThatIsPrefixOf(const std::string& s, const std::string& m) {
    const size_t max_length = std::min(s.size(), m.size());

    for (size_t length = max_length; length > 0; --length) {
      if (s.compare(s.size() - length, length, m, 0, length) == 0) {
        return length;
      }
    }

    return 0;
  }

  std::string start_marker_;
  std::string end_marker_;
  std::unordered_map<std::string, ToolKind> tool_kinds_;
  bool raw_patch_enabled_ = false;

  State state_ = State::kVisible;
  std::string buffer_;             // bytes pushed but not yet routed
  std::string structured_buffer_;  // in-progress marker block, opening marker included
  std::string patch_buffer_;       // in-progress patch envelope, begin line included

  // Whether buffer_[0] sits at a line boundary. Seeded true because generation starts at stream offset 0, and
  // updated from the last byte of every consumed run — patch markers are line-anchored, so this is what
  // distinguishes an envelope from prose that merely quotes one.
  bool at_line_start_ = true;
};

}  // namespace fl
