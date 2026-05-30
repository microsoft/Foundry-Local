// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Reasoning-model integration tests.
//
// These tests target models with supports_reasoning=1 (e.g. qwen3) which emit <think>...</think> blocks. The session
// splits reasoning into a typed REASONING TextItem alongside the visible (DEFAULT) text on the assistant message,
// and invalidates the cached generator after each reasoning turn so the chat template re-strips prior <think> blocks
// from history (matching the C# implementation). This prevents the model from producing unterminated reasoning on
// continuous-decoding follow-up turns.
//
// Assertions are intentionally tolerant:
//   * Visible text must be non-empty.
//   * Visible text must NOT contain raw <think> / </think> tags (the markers are consumed by the splitter).
//   * Reasoning text must be exposed on at least one turn — the model's chain-of-thought is now first-class output.
//   * Multi-turn correctness is validated by finish reason and structural properties (turn count, usage), not by
//     exact-string answer matching, because reasoning models often paraphrase rather than echo.
#include "model_fixture.h"

namespace {

bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle) {
  if (needle.empty()) {
    return true;
  }
  auto it = std::search(haystack.begin(), haystack.end(),
                        needle.begin(), needle.end(),
                        [](char a, char b) {
                          return std::tolower(static_cast<unsigned char>(a)) ==
                                 std::tolower(static_cast<unsigned char>(b));
                        });
  return it != haystack.end();
}

}  // namespace

// Single-turn: reasoning model produces a non-empty visible answer plus exposed reasoning content.
TEST_F(ReasoningFixture, SingleTurnStripsThinkBlock) {
  using namespace foundry_local;

  Session session(reasoning_model());
  Request request{
      SystemMessage("You are a helpful math assistant. Be brief."),
      UserMessage("What is 2+2? Answer with just the number."),
  };
  RequestOptions opts;
  opts.search.temperature = 0.0f;
  opts.search.max_output_tokens = 1024;
  opts.search.frequency_penalty = 1.2f;
  request.SetOptions(opts);

  Response response = session.ProcessRequest(request);

  EXPECT_EQ(response.GetFinishReason(), FOUNDRY_LOCAL_FINISH_STOP);

  std::string visible = CollectResponseVisibleText(response);
  std::string reasoning = CollectResponseReasoningText(response);

  EXPECT_FALSE(visible.empty()) << "Visible text should be non-empty.";
  EXPECT_EQ(visible.find("<think>"), std::string::npos)
      << "Visible text must not contain raw <think>. Got: " << visible;
  EXPECT_EQ(visible.find("</think>"), std::string::npos)
      << "Visible text must not contain raw </think>. Got: " << visible;
  EXPECT_NE(visible.find("4"), std::string::npos)
      << "Reasoning model should still arrive at the answer '4'. Got: " << visible;
  EXPECT_FALSE(reasoning.empty())
      << "Reasoning content should be exposed on the response (typed REASONING TextItem).";
  EXPECT_EQ(reasoning.find("<think>"), std::string::npos)
      << "Reasoning content must have its markers stripped. Got: " << reasoning;

  std::cout << "Reasoning single-turn visible: " << visible << "\n";
  std::cout << "Reasoning single-turn thoughts: " << reasoning << "\n";
}

// Multi-turn: continuous decoding through a reasoning model must not crash and must produce non-empty visible output
// on every turn. This is the regression guard for the "Turn 2 emits unterminated <think>" failure mode.
TEST_F(ReasoningFixture, MultiTurnContinuousDecoding) {
  using namespace foundry_local;

  ChatSession session(reasoning_model());
  RequestOptions session_opts;
  session_opts.search.temperature = 0.0f;
  session_opts.search.max_output_tokens = 1024;
  session_opts.search.frequency_penalty = 1.2f;
  session.SetOptions(session_opts);

  // Turn 1
  Request req1{
      SystemMessage("You are a helpful math assistant. Be brief."),
      UserMessage("What is 2+2? Answer with just the number."),
  };
  Response r1 = session.ProcessRequest(req1);
  std::string t1 = CollectResponseVisibleText(r1);
  std::string r1_reasoning = CollectResponseReasoningText(r1);

  EXPECT_EQ(r1.GetFinishReason(), FOUNDRY_LOCAL_FINISH_STOP);
  EXPECT_FALSE(t1.empty()) << "Turn 1 visible text should be non-empty.";
  EXPECT_EQ(t1.find("<think>"), std::string::npos);
  EXPECT_FALSE(r1_reasoning.empty())
      << "Turn 1 reasoning content should be exposed (typed REASONING TextItem).";
  EXPECT_EQ(session.TurnCount(), 1u);
  std::cout << "Reasoning Turn 1 visible: " << t1 << "\n";
  std::cout << "Reasoning Turn 1 thoughts: " << r1_reasoning << "\n";

  // Turn 2 — exercises post-reasoning generator invalidation. Without the invalidation, the model leaves an
  // unterminated <think> in the KV cache and Turn 2 visible text is empty.
  Request req2{
      UserMessage("Now add 1 to that. Answer with just the number."),
  };
  Response r2 = session.ProcessRequest(req2);
  std::string t2 = CollectResponseVisibleText(r2);

  EXPECT_EQ(r2.GetFinishReason(), FOUNDRY_LOCAL_FINISH_STOP);
  EXPECT_FALSE(t2.empty())
      << "Turn 2 visible text should be non-empty (regression guard for"
      << " unterminated <think> on continuous decoding).";
  EXPECT_EQ(t2.find("<think>"), std::string::npos);
  EXPECT_EQ(t2.find("</think>"), std::string::npos);
  EXPECT_GT(r2.GetUsage().completion_tokens, 0);
  EXPECT_EQ(session.TurnCount(), 2u);
  std::cout << "Reasoning Turn 2 visible: " << t2 << "\n";

  // Turn 3 — verify the session keeps producing valid output beyond Turn 2.
  Request req3{
      UserMessage("Now add 1 to that. Answer with just the number."),
  };
  Response r3 = session.ProcessRequest(req3);
  std::string t3 = CollectResponseVisibleText(r3);

  EXPECT_EQ(r3.GetFinishReason(), FOUNDRY_LOCAL_FINISH_STOP);
  EXPECT_FALSE(t3.empty()) << "Turn 3 visible text should be non-empty.";
  EXPECT_EQ(t3.find("<think>"), std::string::npos);
  EXPECT_EQ(session.TurnCount(), 3u);
  std::cout << "Reasoning Turn 3 visible: " << t3 << "\n";
}

// Reasoning model handles a recall task across turns. Tolerant assertion: the model may paraphrase or quote the secret
// word; we only require that some form of the word survives in the answer.
TEST_F(ReasoningFixture, RecallAcrossTurns) {
  using namespace foundry_local;

  ChatSession session(reasoning_model());
  RequestOptions session_opts;
  session_opts.search.temperature = 0.0f;
  session_opts.search.max_output_tokens = 1024;
  session.SetOptions(session_opts);

  Request req1{
      SystemMessage("You are a helpful assistant. Always remember facts the user tells you."),
      UserMessage("Please remember that the secret word is 'banana'. Acknowledge briefly."),
  };
  Response r1 = session.ProcessRequest(req1);
  std::string t1 = CollectResponseVisibleText(r1);

  EXPECT_EQ(r1.GetFinishReason(), FOUNDRY_LOCAL_FINISH_STOP);
  EXPECT_FALSE(t1.empty());
  std::cout << "Reasoning Recall Turn 1 visible: " << t1 << "\n";

  Request req2{
      UserMessage("What is the secret word? Reply with just the word."),
  };
  Response r2 = session.ProcessRequest(req2);
  std::string t2 = CollectResponseVisibleText(r2);

  EXPECT_EQ(r2.GetFinishReason(), FOUNDRY_LOCAL_FINISH_STOP);
  EXPECT_FALSE(t2.empty());
  EXPECT_EQ(t2.find("<think>"), std::string::npos);
  EXPECT_TRUE(ContainsCaseInsensitive(t2, "banana"))
      << "Turn 2 should recall 'banana'. Got: " << t2;
  std::cout << "Reasoning Recall Turn 2 visible: " << t2 << "\n";
}
