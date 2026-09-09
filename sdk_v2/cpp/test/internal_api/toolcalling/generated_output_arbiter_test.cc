// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Tests for GeneratedOutputArbiter — the one state machine that decides whether a byte of generated output is
// visible prose, part of a structured tool-call block, or part of a raw `*** Begin Patch` envelope.
//
// The suite pins down four things the chat streaming paths depend on:
//   - the raw envelope grammar, exactly (line anchoring, terminators, byte-exact payloads);
//   - that the two readings arbitrate rather than intercept each other;
//   - that chunking is irrelevant — every split point and a byte-at-a-time feed must read identically;
//   - that end-of-stream is lossless, whatever state the machine is in when the turn stops.
//
#include "inferencing/generative/toolcalling/generated_output_arbiter.h"

#include "contracts/responses.h"
#include "contracts/tool_definitions.h"
#include "inferencing/generative/chat/reasoning_stream_splitter.h"
#include "inferencing/generative/openresponses/response_converter.h"
#include "inferencing/session/tool_registry.h"
#include "util/sha256.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace fl;

namespace {

using Output = GeneratedOutputArbiter::Output;

constexpr const char* kStart = "<tool_call>";
constexpr const char* kEnd = "</tool_call>";

/// A turn that offered `apply_patch` as a custom tool alongside an ordinary function tool. This is the only shape
/// that makes a bare patch envelope mean anything.
ToolCallContext PatchTurn() {
  ToolCallContext context;
  context.tool_call_start = kStart;
  context.tool_call_end = kEnd;
  context.tool_output = true;
  context.tool_kinds = {{"apply_patch", ToolKind::kCustom}, {"get_weather", ToolKind::kFunction}};
  return context;
}

/// The same turn without `apply_patch` — structured calls only.
ToolCallContext StructuredOnlyTurn() {
  ToolCallContext context = PatchTurn();
  context.tool_kinds = {{"get_weather", ToolKind::kFunction}};
  return context;
}

/// Everything an arbiter reported for one run, with adjacent text runs merged so that chunk boundaries cannot show
/// up as event boundaries.
struct Trace {
  std::vector<GeneratedOutputArbiter::Event> events;

  std::string Visible() const {
    std::string visible;
    for (const auto& event : events) {
      if (const auto* text = std::get_if<std::string>(&event)) {
        visible += *text;
      }
    }
    return visible;
  }

  std::vector<ParsedToolCall> Calls() const {
    std::vector<ParsedToolCall> calls;
    for (const auto& event : events) {
      if (const auto* call = std::get_if<ParsedToolCall>(&event)) {
        calls.push_back(*call);
      }
    }
    return calls;
  }
};

void Absorb(Trace& trace, Output out) {
  for (auto& event : out.events) {
    if (auto* text = std::get_if<std::string>(&event)) {
      if (!trace.events.empty()) {
        if (auto* previous = std::get_if<std::string>(&trace.events.back())) {
          *previous += *text;
          continue;
        }
      }
    }

    trace.events.push_back(std::move(event));
  }
}

/// Feed chunks through a fresh arbiter and drain it, the way both chat streaming paths do.
Trace ReadStream(const ToolCallContext& context, const std::vector<std::string>& chunks) {
  GeneratedOutputArbiter arbiter(context);
  Trace trace;

  for (const auto& chunk : chunks) {
    Absorb(trace, arbiter.Push(chunk));
  }

  Absorb(trace, arbiter.Flush());
  EXPECT_FALSE(arbiter.InsideCall()) << "a drained arbiter must be back outside every envelope";
  return trace;
}

Trace ReadStream(const ToolCallContext& context, const std::string& text) {
  return ReadStream(context, std::vector<std::string>{text});
}

/// Everything a run reported, in order, with call IDs excluded — IDs are minted randomly, so two runs of the same
/// bytes agree on everything but them.
std::string Describe(const Trace& trace) {
  std::string description;
  for (const auto& event : trace.events) {
    if (const auto* text = std::get_if<std::string>(&event)) {
      description += "\n[text]" + *text;
    } else {
      const auto& call = std::get<ParsedToolCall>(event);
      description += "\n[call " + call.name + "]" + call.arguments;
    }
  }
  return description;
}

std::vector<std::string> Bytes(const std::string& text) {
  std::vector<std::string> bytes;
  bytes.reserve(text.size());
  for (char c : text) {
    bytes.emplace_back(1, c);
  }
  return bytes;
}

/// Assert that how the stream was chunked cannot change how it reads: whole, split at every byte boundary, and one
/// byte at a time. A marker straddling a chunk boundary is the normal case for a real tokenizer, not an edge case.
void ExpectChunkingIsIrrelevant(const ToolCallContext& context, const std::string& text) {
  const auto expected = Describe(ReadStream(context, text));

  for (size_t split = 0; split <= text.size(); ++split) {
    SCOPED_TRACE("split after byte " + std::to_string(split));
    EXPECT_EQ(Describe(ReadStream(context, {text.substr(0, split), text.substr(split)})), expected);
  }

  SCOPED_TRACE("byte at a time");
  EXPECT_EQ(Describe(ReadStream(context, Bytes(text))), expected);
}

const std::string kSimplePatch = "*** Begin Patch\n*** Update File: a.txt\n-old\n+new\n*** End Patch";

const std::string kStructuredCall = R"(<tool_call>{"name":"get_weather","arguments":{"city":"Seattle"}}</tool_call>)";

fl::responses::ResponseCreateParams ParamsWithTools(const std::string& tools_array_json) {
  auto request = nlohmann::json::parse(R"({"model": "m", "input": "hi"})");
  request["tools"] = nlohmann::json::parse(tools_array_json);
  return request.get<fl::responses::ResponseCreateParams>();
}

}  // namespace

// ==========================================================================
// Passthrough and plain text.
// ==========================================================================

TEST(GeneratedOutputArbiterTest, NoMarkersAndNoPatchToolIsPassthrough) {
  ToolCallContext context;
  const auto trace = ReadStream(context, "any text including <tool_call> and *** Begin Patch\n markers");

  EXPECT_EQ(trace.Visible(), "any text including <tool_call> and *** Begin Patch\n markers");
  EXPECT_TRUE(trace.Calls().empty());
}

TEST(GeneratedOutputArbiterTest, EmptyChunkProducesNothing) {
  GeneratedOutputArbiter arbiter(PatchTurn());
  EXPECT_TRUE(arbiter.Push("").events.empty());
  EXPECT_FALSE(arbiter.InsideCall());
}

TEST(GeneratedOutputArbiterTest, PlainTextPassesThroughVerbatim) {
  const auto trace = ReadStream(PatchTurn(), {"Hello", ", ", "world!"});

  EXPECT_EQ(trace.Visible(), "Hello, world!");
  EXPECT_TRUE(trace.Calls().empty());
}

TEST(GeneratedOutputArbiterTest, MarkdownRuleThatSharesTheBeginPrefixStaysVisible) {
  const std::string text = "intro\n***\nnot a patch\n";
  ExpectChunkingIsIrrelevant(PatchTurn(), text);

  const auto trace = ReadStream(PatchTurn(), text);
  EXPECT_EQ(trace.Visible(), text);
  EXPECT_TRUE(trace.Calls().empty());
}

// ==========================================================================
// Structured tool-call blocks — behavior preserved from the accumulator this arbiter replaces.
// ==========================================================================

TEST(GeneratedOutputArbiterTest, StructuredCallInOneChunkKeepsPrefixAndSuffixVisible) {
  const auto trace = ReadStream(PatchTurn(), "prefix " + kStructuredCall + " suffix");

  ASSERT_EQ(trace.events.size(), 3u);
  EXPECT_EQ(std::get<std::string>(trace.events[0]), "prefix ");
  EXPECT_EQ(std::get<ParsedToolCall>(trace.events[1]).name, "get_weather");
  EXPECT_EQ(std::get<ParsedToolCall>(trace.events[1]).arguments, R"({"city":"Seattle"})");
  EXPECT_EQ(std::get<std::string>(trace.events[2]), " suffix");
}

TEST(GeneratedOutputArbiterTest, StructuredCallReadsIdenticallyAtEverySplitPoint) {
  ExpectChunkingIsIrrelevant(PatchTurn(), "before " + kStructuredCall + " after");
}

TEST(GeneratedOutputArbiterTest, TwoSequentialStructuredCallsKeepMiddleTextVisible) {
  const auto trace = ReadStream(PatchTurn(),
                                R"(<tool_call>{"name":"a","arguments":{}}</tool_call> middle )"
                                R"(<tool_call>{"name":"b","arguments":{}}</tool_call>)");

  EXPECT_EQ(trace.Visible(), " middle ");
  const auto calls = trace.Calls();
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_EQ(calls[0].name, "a");
  EXPECT_EQ(calls[1].name, "b");
  EXPECT_NE(calls[0].id, calls[1].id);
}

TEST(GeneratedOutputArbiterTest, UnterminatedStructuredBlockBecomesVisibleOnFlush) {
  const std::string text = R"(prefix <tool_call>{"name":"truncated)";
  const auto trace = ReadStream(PatchTurn(), text);

  EXPECT_EQ(trace.Visible(), text);
  EXPECT_TRUE(trace.Calls().empty());
}

TEST(GeneratedOutputArbiterTest, CompletedMalformedStructuredBlockBecomesVisible) {
  const std::string text = "before <tool_call>not json</tool_call> after";
  const auto trace = ReadStream(PatchTurn(), text);

  EXPECT_EQ(trace.Visible(), text);
  EXPECT_TRUE(trace.Calls().empty());
}

TEST(GeneratedOutputArbiterTest, FalseStartPrefixIsHeldThenReleased) {
  GeneratedOutputArbiter arbiter(PatchTurn());

  auto first = arbiter.Push("hello <tool");
  ASSERT_EQ(first.events.size(), 1u);
  EXPECT_EQ(std::get<std::string>(first.events[0]), "hello ");

  auto second = arbiter.Push("box>");
  ASSERT_EQ(second.events.size(), 1u);
  EXPECT_EQ(std::get<std::string>(second.events[0]), "<toolbox>");
}

TEST(GeneratedOutputArbiterTest, StructuredCustomToolPayloadIsUnwrappedOnce) {
  nlohmann::json call{{"name", "apply_patch"}, {"arguments", {{"input", kSimplePatch}}}};
  const auto trace = ReadStream(PatchTurn(), kStart + call.dump() + kEnd);

  const auto calls = trace.Calls();
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].name, "apply_patch");
  EXPECT_EQ(calls[0].arguments, kSimplePatch) << "the synthesized {\"input\": ...} wrapper is removed exactly once";
}

TEST(GeneratedOutputArbiterTest, StructuredFunctionToolPayloadIsNotUnwrapped) {
  const auto trace = ReadStream(StructuredOnlyTurn(), kStructuredCall);

  const auto calls = trace.Calls();
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].arguments, R"({"city":"Seattle"})");
}

// ==========================================================================
// Raw mode activation — the turn's own tool set is what gives an envelope a meaning.
// ==========================================================================

TEST(GeneratedOutputArbiterTest, RawModeNeedsApplyPatchRegisteredAsCustom) {
  EXPECT_TRUE(GeneratedOutputArbiter(PatchTurn()).RawPatchEnabled());

  const auto trace = ReadStream(PatchTurn(), kSimplePatch + "\n");
  const auto calls = trace.Calls();
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].name, "apply_patch");
}

TEST(GeneratedOutputArbiterTest, ApplyPatchRegisteredAsFunctionDoesNotActivateRawMode) {
  auto context = PatchTurn();
  context.tool_kinds["apply_patch"] = ToolKind::kFunction;

  EXPECT_FALSE(GeneratedOutputArbiter(context).RawPatchEnabled());

  const std::string text = kSimplePatch + "\n";
  const auto trace = ReadStream(context, text);
  EXPECT_EQ(trace.Visible(), text);
  EXPECT_TRUE(trace.Calls().empty());
}

TEST(GeneratedOutputArbiterTest, UnregisteredApplyPatchDoesNotActivateRawMode) {
  const std::string text = kSimplePatch + "\n";
  const auto trace = ReadStream(StructuredOnlyTurn(), text);

  EXPECT_FALSE(GeneratedOutputArbiter(StructuredOnlyTurn()).RawPatchEnabled());
  EXPECT_EQ(trace.Visible(), text);
  EXPECT_TRUE(trace.Calls().empty());
}

TEST(GeneratedOutputArbiterTest, ADifferentCustomToolDoesNotActivateRawMode) {
  auto context = StructuredOnlyTurn();
  context.tool_kinds["write_file"] = ToolKind::kCustom;

  EXPECT_FALSE(GeneratedOutputArbiter(context).RawPatchEnabled());

  const std::string text = kSimplePatch + "\n";
  EXPECT_EQ(ReadStream(context, text).Visible(), text);
}

TEST(GeneratedOutputArbiterTest, ToolChoiceNoneDisablesBothReadings) {
  auto context = PatchTurn();
  context.tool_output = false;

  EXPECT_FALSE(GeneratedOutputArbiter(context).RawPatchEnabled());

  const std::string text = kSimplePatch + "\n" + kStructuredCall;
  const auto trace = ReadStream(context, text);
  EXPECT_EQ(trace.Visible(), text);
  EXPECT_TRUE(trace.Calls().empty());
}

TEST(GeneratedOutputArbiterTest, AllowedToolsFilteringThatDropsApplyPatchDisablesRawMode) {
  // The kinds a turn carries come from the definitions left after forced / allowed filtering, so a request that
  // excludes apply_patch cannot have its prose reinterpreted as a call.
  auto params = ParamsWithTools(R"([
    {"type": "custom", "name": "apply_patch"},
    {"type": "function", "name": "bash", "parameters": {"type": "object"}}
  ])");
  params.allowed_tools = std::vector<std::string>{"bash"};

  Request session_request;
  const auto definitions = ResponseConverter::ExtractResponsesToolDefinitions(params, session_request);

  auto context = PatchTurn();
  context.tool_kinds = tools::KindsByName(definitions);

  ASSERT_EQ(definitions.size(), 1u);
  EXPECT_EQ(definitions[0].name, "bash");
  EXPECT_FALSE(GeneratedOutputArbiter(context).RawPatchEnabled());
  EXPECT_EQ(ReadStream(context, kSimplePatch + "\n").Visible(), kSimplePatch + "\n");
}

TEST(GeneratedOutputArbiterTest, ForcingADifferentToolDropsApplyPatchAndDisablesRawMode) {
  auto params = ParamsWithTools(R"([
    {"type": "custom", "name": "apply_patch"},
    {"type": "function", "name": "bash", "parameters": {"type": "object"}}
  ])");
  params.tool_choice = fl::responses::ForcedFunction{"bash"};

  Request session_request;
  const auto definitions = ResponseConverter::ExtractResponsesToolDefinitions(params, session_request);

  auto context = PatchTurn();
  context.tool_kinds = tools::KindsByName(definitions);

  ASSERT_EQ(definitions.size(), 1u);
  EXPECT_EQ(definitions[0].name, "bash");
  EXPECT_FALSE(GeneratedOutputArbiter(context).RawPatchEnabled());
  EXPECT_EQ(ReadStream(context, kSimplePatch + "\n").Visible(), kSimplePatch + "\n");
}

TEST(GeneratedOutputArbiterTest, RawModeWorksWithoutStructuredMarkers) {
  // A model that publishes no tool-call marker tokens can still write a patch envelope.
  auto context = PatchTurn();
  context.tool_call_start.clear();
  context.tool_call_end.clear();

  const auto trace = ReadStream(context, "here:\n" + kSimplePatch + "\ndone");

  EXPECT_EQ(trace.Visible(), "here:\n\ndone");
  const auto calls = trace.Calls();
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].arguments, kSimplePatch);
}

// ==========================================================================
// Raw envelope grammar.
// ==========================================================================

TEST(GeneratedOutputArbiterTest, EnvelopeAtStreamOffsetZeroIsACall) {
  const auto trace = ReadStream(PatchTurn(), kSimplePatch + "\n");

  ASSERT_EQ(trace.events.size(), 2u);
  EXPECT_EQ(std::get<ParsedToolCall>(trace.events[0]).arguments, kSimplePatch);
  EXPECT_EQ(std::get<std::string>(trace.events[1]), "\n")
      << "the newline after the closing marker separates the call from later output and belongs to that output";
}

TEST(GeneratedOutputArbiterTest, PrefixBetweenAndSuffixTextKeepTheirOrder) {
  const std::string second = "*** Begin Patch\n*** Delete File: gone.txt\n*** End Patch";
  const std::string text = "First:\n" + kSimplePatch + "\nThen:\n" + second + "\nDone.";

  const auto trace = ReadStream(PatchTurn(), text);

  ASSERT_EQ(trace.events.size(), 5u);
  EXPECT_EQ(std::get<std::string>(trace.events[0]), "First:\n");
  EXPECT_EQ(std::get<ParsedToolCall>(trace.events[1]).arguments, kSimplePatch);
  EXPECT_EQ(std::get<std::string>(trace.events[2]), "\nThen:\n");
  EXPECT_EQ(std::get<ParsedToolCall>(trace.events[3]).arguments, second);
  EXPECT_EQ(std::get<std::string>(trace.events[4]), "\nDone.");

  const auto calls = trace.Calls();
  EXPECT_NE(calls[0].id, calls[1].id) << "each committed envelope mints its own ID";
  ExpectChunkingIsIrrelevant(PatchTurn(), text);
}

TEST(GeneratedOutputArbiterTest, EnvelopeNotAtALineBoundaryStaysVisible) {
  const std::string text = "see *** Begin Patch\nbody\n*** End Patch\n";
  const auto trace = ReadStream(PatchTurn(), text);

  EXPECT_EQ(trace.Visible(), text);
  EXPECT_TRUE(trace.Calls().empty());
  ExpectChunkingIsIrrelevant(PatchTurn(), text);
}

TEST(GeneratedOutputArbiterTest, IndentedBeginMarkerStaysVisible) {
  const std::string text = "  *** Begin Patch\nbody\n*** End Patch\n";
  const auto trace = ReadStream(PatchTurn(), text);

  EXPECT_EQ(trace.Visible(), text);
  EXPECT_TRUE(trace.Calls().empty());
}

TEST(GeneratedOutputArbiterTest, SuffixedBeginMarkerStaysVisible) {
  const std::string text = "*** Begin Patchwork\nbody\n*** End Patch\n";
  const auto trace = ReadStream(PatchTurn(), text);

  EXPECT_EQ(trace.Visible(), text);
  EXPECT_TRUE(trace.Calls().empty());
  ExpectChunkingIsIrrelevant(PatchTurn(), text);
}

TEST(GeneratedOutputArbiterTest, MisspelledOrRecasedBeginMarkerStaysVisible) {
  for (const char* opener : {"*** begin patch", "*** Begin patch", "*** Bgein Patch", "**** Begin Patch",
                             "** Begin Patch", "*** Begin  Patch", "*** BeginPatch"}) {
    SCOPED_TRACE(opener);
    const std::string text = std::string(opener) + "\nbody\n*** End Patch\n";
    const auto trace = ReadStream(PatchTurn(), text);

    EXPECT_EQ(trace.Visible(), text);
    EXPECT_TRUE(trace.Calls().empty());
  }
}

TEST(GeneratedOutputArbiterTest, ReversedMarkersProduceNoCall) {
  const std::string text = "*** End Patch\nbody\n*** Begin Patch\n";
  const auto trace = ReadStream(PatchTurn(), text);

  EXPECT_EQ(trace.Visible(), text) << "a closing marker opens nothing, and the trailing opener never closes";
  EXPECT_TRUE(trace.Calls().empty());
  ExpectChunkingIsIrrelevant(PatchTurn(), text);
}

TEST(GeneratedOutputArbiterTest, BeginMarkerWithoutATerminatorStaysVisible) {
  const std::string text = "*** Begin Patch";
  const auto trace = ReadStream(PatchTurn(), text);

  EXPECT_EQ(trace.Visible(), text);
  EXPECT_TRUE(trace.Calls().empty());
}

TEST(GeneratedOutputArbiterTest, BeginMarkerFollowedByBareCarriageReturnStaysVisible) {
  const std::string text = "*** Begin Patch\rbody\n*** End Patch\n";
  const auto trace = ReadStream(PatchTurn(), text);

  EXPECT_EQ(trace.Visible(), text);
  EXPECT_TRUE(trace.Calls().empty());
  ExpectChunkingIsIrrelevant(PatchTurn(), text);
}

TEST(GeneratedOutputArbiterTest, CrlfEnvelopeIsACallAndKeepsItsBytes) {
  const std::string patch = "*** Begin Patch\r\n*** Update File: a.txt\r\n-old\r\n+new\r\n*** End Patch";
  const auto trace = ReadStream(PatchTurn(), patch + "\r\n");

  ASSERT_EQ(trace.events.size(), 2u);
  EXPECT_EQ(std::get<ParsedToolCall>(trace.events[0]).arguments, patch);
  EXPECT_EQ(std::get<std::string>(trace.events[1]), "\r\n");
  ExpectChunkingIsIrrelevant(PatchTurn(), patch + "\r\n");
}

TEST(GeneratedOutputArbiterTest, MixedLineEndingsAreNeverNormalized) {
  const std::string patch = "*** Begin Patch\r\n*** Update File: a.txt\n@@\r\n-old\n+new\r\n*** End Patch";
  const auto trace = ReadStream(PatchTurn(), patch + "\n");

  const auto calls = trace.Calls();
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].arguments, patch);
}

TEST(GeneratedOutputArbiterTest, EndMarkerAtEndOfStreamNeedsNoTerminator) {
  const auto trace = ReadStream(PatchTurn(), kSimplePatch);

  ASSERT_EQ(trace.events.size(), 1u);
  EXPECT_EQ(std::get<ParsedToolCall>(trace.events[0]).arguments, kSimplePatch);
  ExpectChunkingIsIrrelevant(PatchTurn(), kSimplePatch);
}

TEST(GeneratedOutputArbiterTest, EndMarkerFollowedByBareCarriageReturnAtEndOfStreamIsNotAClose) {
  const std::string text = "*** Begin Patch\nbody\n*** End Patch\r";
  const auto trace = ReadStream(PatchTurn(), text);

  EXPECT_EQ(trace.Visible(), text) << "a CR suffixes the closing line, so the envelope never closed";
  EXPECT_TRUE(trace.Calls().empty());
  ExpectChunkingIsIrrelevant(PatchTurn(), text);
}

TEST(GeneratedOutputArbiterTest, IndentedOrSuffixedEndMarkerStaysPayload) {
  const std::string patch = "*** Begin Patch\n  *** End Patch\n*** End Patchwork\n*** End Patch";
  const auto trace = ReadStream(PatchTurn(), patch);

  const auto calls = trace.Calls();
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].arguments, patch);
  ExpectChunkingIsIrrelevant(PatchTurn(), patch);
}

TEST(GeneratedOutputArbiterTest, EmptyEnvelopeIsAValidCall) {
  const std::string patch = "*** Begin Patch\n*** End Patch";
  const auto trace = ReadStream(PatchTurn(), patch + "\n");

  const auto calls = trace.Calls();
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].arguments, patch);
  EXPECT_EQ(trace.Visible(), "\n");
}

TEST(GeneratedOutputArbiterTest, NestedBeginMarkerIsPayload) {
  const std::string patch = "*** Begin Patch\n*** Begin Patch\ninner\n*** End Patch";
  const auto trace = ReadStream(PatchTurn(), patch + "\n");

  const auto calls = trace.Calls();
  ASSERT_EQ(calls.size(), 1u)
      << "the first envelope owns every byte until it closes, so the second opener is patch data";
  EXPECT_EQ(calls[0].arguments, patch);
  ExpectChunkingIsIrrelevant(PatchTurn(), patch + "\n");
}

TEST(GeneratedOutputArbiterTest, AddUpdateAndDeleteEnvelopesAreAllOpaquePayload) {
  const std::vector<std::string> patches = {
      "*** Begin Patch\n*** Add File: src/new.txt\n+first\n+second\n*** End Patch",
      "*** Begin Patch\n*** Update File: src/app.py\n@@ def main():\n-    old()\n+    new()\n*** End Patch",
      "*** Begin Patch\n*** Delete File: obsolete.txt\n*** End Patch",
  };

  for (const auto& patch : patches) {
    SCOPED_TRACE(patch);
    const auto calls = ReadStream(PatchTurn(), patch + "\n").Calls();
    ASSERT_EQ(calls.size(), 1u);
    EXPECT_EQ(calls[0].name, "apply_patch");
    EXPECT_EQ(calls[0].arguments, patch);
  }
}

TEST(GeneratedOutputArbiterTest, TabsTrailingSpacesAndUnicodeSurviveByteForByte) {
  const std::string patch =
      "*** Begin Patch\n"
      "*** Update File: src/app.py\n"
      "@@ def main():\n"
      "-    print(\"naïve\")\t\n"
      "+    print(\"naïve → fixed\")  \n"
      "*** End Patch";

  const auto calls = ReadStream(PatchTurn(), patch + "\n").Calls();
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].arguments, patch);
  EXPECT_EQ(calls[0].arguments.size(), 127u);
}

TEST(GeneratedOutputArbiterTest, FixedPayloadHashesToAPinnedValueHoweverItIsChunked) {
  // Byte equality against a pinned digest: a change to trimming, escaping, or newline handling anywhere in the
  // pipeline changes this hash even if the payload still "looks" right in a string comparison.
  const std::string patch =
      "*** Begin Patch\n"
      "*** Update File: src/app.py\n"
      "@@ def main():\n"
      "-    print(\"naïve\")\t\n"
      "+    print(\"naïve → fixed\")  \n"
      "*** End Patch";
  constexpr const char* kExpectedSha256 = "780C4698FA53F0D3DA274446D4722502D254676C32079E0D5DD963C8AF3D6424";

  ASSERT_EQ(Sha256String(patch), kExpectedSha256) << "the literal in this test drifted from the pinned digest";

  const auto whole = ReadStream(PatchTurn(), patch + "\nafter\n").Calls();
  ASSERT_EQ(whole.size(), 1u);
  EXPECT_EQ(Sha256String(whole[0].arguments), kExpectedSha256);

  const auto streamed = ReadStream(PatchTurn(), Bytes(patch + "\nafter\n")).Calls();
  ASSERT_EQ(streamed.size(), 1u);
  EXPECT_EQ(Sha256String(streamed[0].arguments), kExpectedSha256);
  EXPECT_EQ(streamed[0].arguments, whole[0].arguments);
}

TEST(GeneratedOutputArbiterTest, JsonEscapeSequencesInPayloadAreNotUnescaped) {
  const std::string patch = "*** Begin Patch\n+line with \\n and \\\" and \\\\ literals\n*** End Patch";
  const auto calls = ReadStream(PatchTurn(), patch).Calls();

  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].arguments, patch) << "raw payload is opaque — nothing about it is decoded";
}

TEST(GeneratedOutputArbiterTest, RepeatedEnvelopesBackToBackYieldOrderedCalls) {
  const std::string first = "*** Begin Patch\none\n*** End Patch";
  const std::string second = "*** Begin Patch\ntwo\n*** End Patch";
  const std::string third = "*** Begin Patch\nthree\n*** End Patch";

  const auto trace = ReadStream(PatchTurn(), first + "\n" + second + "\n" + third + "\n");
  const auto calls = trace.Calls();

  ASSERT_EQ(calls.size(), 3u);
  EXPECT_EQ(calls[0].arguments, first);
  EXPECT_EQ(calls[1].arguments, second);
  EXPECT_EQ(calls[2].arguments, third);
  EXPECT_EQ(trace.Visible(), "\n\n\n");
  EXPECT_NE(calls[0].id, calls[1].id);
  EXPECT_NE(calls[1].id, calls[2].id);
  EXPECT_NE(calls[0].id, calls[2].id);
}

TEST(GeneratedOutputArbiterTest, ClosingMarkerRunTogetherWithTheNextOpenerIsNotAClose) {
  // Without a newline after `*** End Patch` the closing line is suffixed, so it does not close anything and the
  // following opener is payload. One envelope, spanning everything, is the only reading of these bytes.
  const std::string text = "*** Begin Patch\none\n*** End Patch*** Begin Patch\ntwo\n*** End Patch";
  const auto trace = ReadStream(PatchTurn(), text);
  const auto calls = trace.Calls();

  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].arguments, text);
  EXPECT_TRUE(trace.Visible().empty());
  ExpectChunkingIsIrrelevant(PatchTurn(), text);
}

TEST(GeneratedOutputArbiterTest, CommittedCallIdsUseTheSharedToolCallIdShape) {
  const auto calls = ReadStream(PatchTurn(), kSimplePatch).Calls();

  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].id.substr(0, 5), "call_");
  EXPECT_EQ(calls[0].id.size(), 14u);
}

// ==========================================================================
// Arbitration — whichever envelope opens first owns every byte until it closes.
// ==========================================================================

TEST(GeneratedOutputArbiterTest, StructuredMarkerInsideAnEnvelopeIsPatchData) {
  const std::string patch = "*** Begin Patch\n" + kStructuredCall + "\n*** End Patch";
  const auto trace = ReadStream(PatchTurn(), patch + "\n");
  const auto calls = trace.Calls();

  ASSERT_EQ(calls.size(), 1u) << "the structured reading never sees bytes an open envelope owns";
  EXPECT_EQ(calls[0].name, "apply_patch");
  EXPECT_EQ(calls[0].arguments, patch);
  ExpectChunkingIsIrrelevant(PatchTurn(), patch + "\n");
}

TEST(GeneratedOutputArbiterTest, LineAnchoredPatchMarkersInsideAStructuredBlockAreStructuredData) {
  // The block cannot parse as a tool call, so it is preserved whole as visible text — and crucially it does not
  // become an apply_patch call, which is what a chained patch scanner would have produced.
  const std::string text = "<tool_call>\n*** Begin Patch\nbody\n*** End Patch\n</tool_call>";
  const auto trace = ReadStream(PatchTurn(), text);

  EXPECT_EQ(trace.Visible(), text);
  EXPECT_TRUE(trace.Calls().empty());
  ExpectChunkingIsIrrelevant(PatchTurn(), text);
}

TEST(GeneratedOutputArbiterTest, EscapedPatchMarkersInsideAStructuredCallStayOneStructuredCall) {
  nlohmann::json call{{"name", "apply_patch"}, {"arguments", {{"input", kSimplePatch + "\n"}}}};
  const std::string text = kStart + call.dump() + kEnd;

  const auto trace = ReadStream(PatchTurn(), text);
  const auto calls = trace.Calls();

  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].arguments, kSimplePatch + "\n");
  EXPECT_TRUE(trace.Visible().empty());
  ExpectChunkingIsIrrelevant(PatchTurn(), text);
}

TEST(GeneratedOutputArbiterTest, StructuredMarkerAfterAnEnvelopeClosesIsStillAStructuredCall) {
  const std::string text = kSimplePatch + "\n" + kStructuredCall;
  const auto trace = ReadStream(PatchTurn(), text);
  const auto calls = trace.Calls();

  ASSERT_EQ(calls.size(), 2u);
  EXPECT_EQ(calls[0].name, "apply_patch");
  EXPECT_EQ(calls[1].name, "get_weather");
  EXPECT_EQ(trace.Visible(), "\n");
  ExpectChunkingIsIrrelevant(PatchTurn(), text);
}

TEST(GeneratedOutputArbiterTest, MalformedStructuredBlockThenValidEnvelope) {
  const std::string text = "<tool_call>not json</tool_call>\n" + kSimplePatch + "\n";
  const auto trace = ReadStream(PatchTurn(), text);
  const auto calls = trace.Calls();

  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].name, "apply_patch");
  EXPECT_EQ(calls[0].arguments, kSimplePatch);
  EXPECT_EQ(trace.Visible(), "<tool_call>not json</tool_call>\n\n");
  ExpectChunkingIsIrrelevant(PatchTurn(), text);
}

TEST(GeneratedOutputArbiterTest, MalformedEnvelopeThenValidStructuredBlock) {
  // The indented opener is prose, so the structured block that follows is read normally.
  const std::string text = "  *** Begin Patch\nnot a patch\n" + kStructuredCall;
  const auto trace = ReadStream(PatchTurn(), text);
  const auto calls = trace.Calls();

  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].name, "get_weather");
  EXPECT_EQ(trace.Visible(), "  *** Begin Patch\nnot a patch\n");
  ExpectChunkingIsIrrelevant(PatchTurn(), text);
}

TEST(GeneratedOutputArbiterTest, MisspelledEnvelopeThenValidEnvelope) {
  const std::string text = "*** Begin Patchh\noops\n*** End Patch\n" + kSimplePatch + "\n";
  const auto trace = ReadStream(PatchTurn(), text);
  const auto calls = trace.Calls();

  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].arguments, kSimplePatch);
  EXPECT_EQ(trace.Visible(), "*** Begin Patchh\noops\n*** End Patch\n\n");
}

TEST(GeneratedOutputArbiterTest, OverlappingMarkerPrefixesAreHeldUntilTheyResolve) {
  GeneratedOutputArbiter arbiter(PatchTurn());

  // "*** Begin Pat" is a live prefix of the opener and must not be emitted yet.
  auto held = arbiter.Push("intro\n*** Begin Pat");
  ASSERT_EQ(held.events.size(), 1u);
  EXPECT_EQ(std::get<std::string>(held.events[0]), "intro\n");
  EXPECT_FALSE(arbiter.InsideCall());

  auto resolved = arbiter.Push("terns are fine.\n");
  ASSERT_EQ(resolved.events.size(), 1u);
  EXPECT_EQ(std::get<std::string>(resolved.events[0]), "*** Begin Patterns are fine.\n");
}

TEST(GeneratedOutputArbiterTest, MarkerPrefixSpanningManyChunksIsNeverLeaked) {
  GeneratedOutputArbiter arbiter(PatchTurn());
  std::string leaked;

  for (const auto& chunk : Bytes("*** Begin Patch\nbody\n*** End Patch")) {
    for (const auto& event : arbiter.Push(chunk).events) {
      if (const auto* text = std::get_if<std::string>(&event)) {
        leaked += *text;
      }
    }
  }

  EXPECT_TRUE(leaked.empty()) << "no byte of an in-progress envelope may be emitted as visible text";

  auto final_events = arbiter.Flush();
  ASSERT_EQ(final_events.events.size(), 1u);
  EXPECT_EQ(std::get<ParsedToolCall>(final_events.events[0]).arguments, "*** Begin Patch\nbody\n*** End Patch");
}

// ==========================================================================
// Terminal semantics: natural stop, output limit, cancellation, failure.
// All four reach the arbiter the same way — Flush() — and all four must be lossless.
// ==========================================================================

TEST(GeneratedOutputArbiterTest, FlushOutsideAnEnvelopeReleasesHeldPrefixes) {
  for (const char* tail : {"<too", "*** Begin Pat", "*** Begin Patch", "*", "**", "***"}) {
    SCOPED_TRACE(tail);
    GeneratedOutputArbiter arbiter(PatchTurn());
    arbiter.Push("hello\n");
    arbiter.Push(tail);

    auto drained = arbiter.Flush();
    ASSERT_EQ(drained.events.size(), 1u);
    EXPECT_EQ(std::get<std::string>(drained.events[0]), tail);
    EXPECT_FALSE(arbiter.InsideCall());
  }
}

TEST(GeneratedOutputArbiterTest, FlushInsideAnEnvelopeReportsItVerbatimAndNeverAsACall) {
  const std::string truncated = "*** Begin Patch\n*** Update File: a.txt\n+half a line";

  GeneratedOutputArbiter arbiter(PatchTurn());
  arbiter.Push(truncated);
  EXPECT_TRUE(arbiter.InsideRawPatch());

  auto drained = arbiter.Flush();
  ASSERT_EQ(drained.events.size(), 1u);
  EXPECT_EQ(std::get<std::string>(drained.events[0]), truncated)
      << "an incomplete envelope is visible output, never an actionable call";
  EXPECT_FALSE(arbiter.InsideCall());
}

TEST(GeneratedOutputArbiterTest, FlushInsideAStructuredBlockReportsItVerbatim) {
  const std::string truncated = R"(<tool_call>{"name":"get_weather")";

  GeneratedOutputArbiter arbiter(PatchTurn());
  arbiter.Push(truncated);
  EXPECT_TRUE(arbiter.InsideStructuredCall());

  auto drained = arbiter.Flush();
  ASSERT_EQ(drained.events.size(), 1u);
  EXPECT_EQ(std::get<std::string>(drained.events[0]), truncated);
}

TEST(GeneratedOutputArbiterTest, StoppingInEveryParserStateIsLossless) {
  // Every prefix of a stream that mixes prose, a completed envelope, and a truncated one models a turn that was
  // cancelled, hit its output-token limit, or failed at that exact byte. Concatenating the visible text with the
  // payload of every committed call must reproduce the bytes generated so far.
  const std::string stream = "intro\n" + kSimplePatch + "\ntail\n*** Begin Patch\ntruncated";

  for (size_t stop = 0; stop <= stream.size(); ++stop) {
    SCOPED_TRACE("stopped after byte " + std::to_string(stop));
    const auto prefix = stream.substr(0, stop);
    const auto trace = ReadStream(PatchTurn(), Bytes(prefix));

    std::string reconstructed;
    for (const auto& event : trace.events) {
      if (const auto* text = std::get_if<std::string>(&event)) {
        reconstructed += *text;
      } else {
        reconstructed += std::get<ParsedToolCall>(event).arguments;
      }
    }

    EXPECT_EQ(reconstructed, prefix);
  }
}

TEST(GeneratedOutputArbiterTest, CompletedCallSurvivesAStopLaterInTheSameTurn) {
  const std::string stream = kSimplePatch + "\nthen the turn was cut off mid *** Begin Pat";
  const auto trace = ReadStream(PatchTurn(), Bytes(stream));
  const auto calls = trace.Calls();

  ASSERT_EQ(calls.size(), 1u) << "a committed envelope stays committed; only the unfinished one is dropped";
  EXPECT_EQ(calls[0].arguments, kSimplePatch);
  EXPECT_EQ(trace.Visible(), "\nthen the turn was cut off mid *** Begin Pat");
}

TEST(GeneratedOutputArbiterTest, FlushIsIdempotentAndTheArbiterKeepsWorkingAfterIt) {
  // The native path flushes at every reasoning boundary, so a flushed arbiter must still be usable.
  GeneratedOutputArbiter arbiter(PatchTurn());
  arbiter.Push("before\n");

  EXPECT_TRUE(arbiter.Flush().events.empty());
  EXPECT_TRUE(arbiter.Flush().events.empty());

  auto after = arbiter.Push(kSimplePatch);
  Output drained = arbiter.Flush();

  Trace trace;
  Absorb(trace, std::move(after));
  Absorb(trace, std::move(drained));

  const auto calls = trace.Calls();
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].arguments, kSimplePatch);
}

// ==========================================================================
// Reasoning bypasses every reading — a marker in the scratchpad is scratchpad.
// ==========================================================================

TEST(GeneratedOutputArbiterTest, PatchMarkersInsideReasoningStayReasoning) {
  ReasoningStreamSplitter splitter("<think>", "</think>", {10}, {20});
  GeneratedOutputArbiter arbiter(PatchTurn());

  const std::vector<std::pair<int32_t, std::string>> tokens = {
      {10, ""},
      {1, "I could write "},
      {2, "*** Begin Patch\n*** Update File: a.txt\n*** End Patch\n"},
      {3, "but let me think more."},
      {20, ""},
      {4, "Here it is:\n"},
      {5, kSimplePatch},
      {6, "\n"},
  };

  std::string reasoning;
  Trace trace;

  for (const auto& [id, text] : tokens) {
    for (const auto& segment : splitter.Push(id, text)) {
      if (segment.type == FOUNDRY_LOCAL_TEXT_ITEM_TYPE_REASONING) {
        Absorb(trace, arbiter.Flush());
        reasoning += segment.text;
        continue;
      }

      Absorb(trace, arbiter.Push(segment.text));
    }
  }

  Absorb(trace, arbiter.Flush());

  EXPECT_NE(reasoning.find("*** Begin Patch"), std::string::npos)
      << "the scratchpad keeps its marker text exactly as produced";

  const auto calls = trace.Calls();
  ASSERT_EQ(calls.size(), 1u) << "only the envelope written outside reasoning is a call";
  EXPECT_EQ(calls[0].arguments, kSimplePatch);
  EXPECT_EQ(trace.Visible(), "Here it is:\n\n");
}

TEST(GeneratedOutputArbiterTest, ReasoningMarkerInsidePatchAbandonsRatherThanCorruptsTheCall) {
  ReasoningStreamSplitter splitter("<think>", "</think>");
  GeneratedOutputArbiter arbiter(PatchTurn());

  const std::vector<std::string> chunks = {
      "*** Begin Patch\n*** Update File: t.cc\n+const char* marker = \"<think>",
      "literal marker",
      "</think>\";\n*** End Patch\n",
  };

  std::string reasoning;
  Trace trace;

  for (const auto& chunk : chunks) {
    for (const auto& segment : splitter.Push(chunk)) {
      if (segment.type == FOUNDRY_LOCAL_TEXT_ITEM_TYPE_REASONING) {
        Absorb(trace, arbiter.Flush());
        reasoning += segment.text;
        continue;
      }

      Absorb(trace, arbiter.Push(segment.text));
    }
  }

  for (const auto& segment : splitter.Flush()) {
    if (segment.type == FOUNDRY_LOCAL_TEXT_ITEM_TYPE_REASONING) {
      Absorb(trace, arbiter.Flush());
      reasoning += segment.text;
    } else {
      Absorb(trace, arbiter.Push(segment.text));
    }
  }
  Absorb(trace, arbiter.Flush());

  EXPECT_TRUE(trace.Calls().empty()) << "a patch missing splitter-consumed bytes must never become actionable";
  EXPECT_NE(trace.Visible().find("*** Begin Patch"), std::string::npos);
  EXPECT_NE(trace.Visible().find("*** End Patch"), std::string::npos);
  EXPECT_EQ(reasoning, "literal marker");
}
