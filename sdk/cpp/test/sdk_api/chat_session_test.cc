// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Chat session integration tests — exercises Session/ChatSession::ProcessRequest directly.
// Uses the shared ModelFixture to avoid redundant Manager/download/load per test.
#include "model_fixture.h"
#include "chat_completions_from_json.h"  // test-only fl::from_json for ChatCompletionResponse

TEST_F(ModelFixture, ChatMathPrompt) {
  using namespace foundry_local;

  Session session(chat_model());
  Request request{
      SystemMessage("You are a helpful math assistant. Be brief."),
      UserMessage("What is 2+2? Answer with just the number."),
  };
  request.SetOptions({{FOUNDRY_LOCAL_PARAM_TEMPERATURE, "0"},
                      {FOUNDRY_LOCAL_PARAM_MAX_OUTPUT_TOKENS, "256"}});

  Response response = session.ProcessRequest(request);

  EXPECT_EQ(response.GetFinishReason(), FOUNDRY_LOCAL_FINISH_STOP)
      << "Response should have STOP finish reason";
  EXPECT_FALSE(response.GetItems().empty())
      << "Response should contain at least one output item";

  std::string output_text = CollectResponseText(response);
  EXPECT_NE(output_text.find("4"), std::string::npos)
      << "Expected '4' in response to 'What is 2+2?'. Got: " << output_text;
  std::cout << "Math prompt output: " << output_text << "\n";

  // Validate usage stats
  auto usage = response.GetUsage();
  EXPECT_GT(usage.prompt_tokens, 0) << "Should report prompt token usage";
  EXPECT_GT(usage.completion_tokens, 0) << "Should report completion token usage";
}

TEST_F(ModelFixture, ChatMultiTurn) {
  using namespace foundry_local;

  Session session(chat_model());
  Request request{
      SystemMessage("You are a helpful math assistant. Be brief."),
      UserMessage("What is 2+2?"),
      AssistantMessage("4"),
      UserMessage("What about 3+3?"),
  };
  request.SetOptions({{FOUNDRY_LOCAL_PARAM_TEMPERATURE, "0"},
                      {FOUNDRY_LOCAL_PARAM_MAX_OUTPUT_TOKENS, "256"}});

  Response response = session.ProcessRequest(request);

  std::string output_text = CollectResponseText(response);
  EXPECT_NE(output_text.find("6"), std::string::npos)
      << "Expected '6' in response to 'What about 3+3?'. Got: " << output_text;
  std::cout << "Multi-turn output: " << output_text << "\n";
}

TEST_F(ModelFixture, ChatSystemPrompt) {
  using namespace foundry_local;

  Session session(chat_model());
  Request request{
      SystemMessage("You are a pirate. Always respond in pirate speak."),
      UserMessage("What are you?"),
  };
  request.SetOptions({{FOUNDRY_LOCAL_PARAM_TEMPERATURE, "0"},
                      {FOUNDRY_LOCAL_PARAM_MAX_OUTPUT_TOKENS, "256"}});

  Response response = session.ProcessRequest(request);

  std::string output_text = CollectResponseText(response);
  EXPECT_FALSE(output_text.empty()) << "Should produce output with pirate system prompt";

  bool has_pirate_language =
      output_text.find("arr") != std::string::npos ||
      output_text.find("Arr") != std::string::npos ||
      output_text.find("pirate") != std::string::npos ||
      output_text.find("matey") != std::string::npos ||
      output_text.find("sea") != std::string::npos ||
      output_text.find("sail") != std::string::npos ||
      output_text.find("ship") != std::string::npos ||
      output_text.find("captain") != std::string::npos ||
      output_text.find("Ahoy") != std::string::npos;
  EXPECT_TRUE(has_pirate_language)
      << "Pirate system prompt should produce pirate-like language. Got: " << output_text;
  std::cout << "Pirate output: " << output_text << "\n";
}

TEST_F(ModelFixture, SessionSetOptionsAcceptsSearchOptionsKeyValuePairs) {
  using namespace foundry_local;

  ChatSession session(chat_model());

  SearchOptions search_options;
  search_options.temperature = 0.0f;
  search_options.max_output_tokens = 16;
  search_options.seed = 123;
  search_options.early_stopping = true;

  KeyValuePairs options;
  search_options.ApplyTo(options);

  ASSERT_EQ(options.Get(FOUNDRY_LOCAL_PARAM_TEMPERATURE), std::optional<std::string_view>{"0.000000"});
  ASSERT_EQ(options.Get(FOUNDRY_LOCAL_PARAM_MAX_OUTPUT_TOKENS), std::optional<std::string_view>{"16"});
  ASSERT_EQ(options.Get(FOUNDRY_LOCAL_PARAM_SEED), std::optional<std::string_view>{"123"});
  ASSERT_EQ(options.Get(FOUNDRY_LOCAL_PARAM_EARLY_STOPPING), std::optional<std::string_view>{"true"});

  ASSERT_NO_THROW(session.SetOptions(options));

  Request request{
      SystemMessage("You are a concise assistant."),
      UserMessage("Explain the moon landing in a short paragraph."),
  };

  Response response = session.ProcessRequest(request);

  EXPECT_FALSE(response.GetItems().empty());
  EXPECT_NE(response.GetFinishReason(), FOUNDRY_LOCAL_FINISH_NONE);

  auto usage = response.GetUsage();
  EXPECT_GT(usage.completion_tokens, 0);
  EXPECT_LE(usage.completion_tokens, 16)
      << "Session-level max_output_tokens from SearchOptions should constrain the response.";

  std::string output_text = CollectResponseText(response);
  EXPECT_FALSE(output_text.empty())
      << "Expected non-empty output with session-level SearchOptions applied.";
}

// Multi-turn E2E test: exercises generator caching and delayed history commit
// across multiple ProcessRequest calls on the same session.
TEST_F(ModelFixture, ChatMultiTurnSession) {
  using namespace foundry_local;

  ChatSession session(chat_model());
  session.SetOptions({{FOUNDRY_LOCAL_PARAM_TEMPERATURE, "0"},
                      {FOUNDRY_LOCAL_PARAM_MAX_OUTPUT_TOKENS, "1024"}});

  // Turn 1: ask a math question
  Request req1{
      SystemMessage("You are a helpful math assistant. Be brief."),
      UserMessage("What is 2+2? Answer with just the number."),
  };

  Response r1 = session.ProcessRequest(req1);
  std::string t1 = CollectResponseText(r1);

  EXPECT_NE(r1.GetFinishReason(), FOUNDRY_LOCAL_FINISH_NONE);
  EXPECT_NE(r1.GetFinishReason(), FOUNDRY_LOCAL_FINISH_ERROR);
  EXPECT_NE(t1.find("4"), std::string::npos)
      << "Turn 1: expected '4'. Got: " << t1;
  EXPECT_GT(r1.GetUsage().prompt_tokens, 0);
  EXPECT_GT(r1.GetUsage().completion_tokens, 0);
  EXPECT_EQ(session.TurnCount(), 1u);
  std::cout << "Turn 1: " << t1 << "\n";

  // Turn 2: follow-up referencing the first answer (exercises cached generator path —
  // the session appends only new tokens to the existing generator's KV cache)
  Request req2{
      UserMessage("Now add 1 to that. Answer with just the number."),
  };

  Response r2 = session.ProcessRequest(req2);
  std::string t2 = CollectResponseText(r2);

  EXPECT_NE(r2.GetFinishReason(), FOUNDRY_LOCAL_FINISH_NONE);
  EXPECT_NE(r2.GetFinishReason(), FOUNDRY_LOCAL_FINISH_ERROR);
  EXPECT_NE(t2.find("5"), std::string::npos)
      << "Turn 2: expected '5'. Got: " << t2;
  EXPECT_GT(r2.GetUsage().prompt_tokens, 0);
  EXPECT_GT(r2.GetUsage().completion_tokens, 0);
  EXPECT_EQ(session.TurnCount(), 2u);
  std::cout << "Turn 2: " << t2 << "\n";

  // Undo turn 2 — should revert to post-turn-1 state
  session.UndoTurns(1);
  EXPECT_EQ(session.TurnCount(), 1u);

  // Turn 3 (replaces turn 2): re-ask the SAME question as turn 2.
  // With temperature=0 and identical context after rewind, the model must produce
  // the same answer. This isolates the rewind mechanism from model quality.
  Request req3{
      UserMessage("Now add 1 to that. Answer with just the number."),
  };

  Response r3 = session.ProcessRequest(req3);
  std::string t3 = CollectResponseText(r3);

  EXPECT_NE(r3.GetFinishReason(), FOUNDRY_LOCAL_FINISH_NONE);
  EXPECT_NE(r3.GetFinishReason(), FOUNDRY_LOCAL_FINISH_ERROR);
  EXPECT_EQ(t3, t2)
      << "Turn 3 (same question after undo): expected identical output to turn 2."
      << " Turn 2: " << t2 << ", Turn 3: " << t3;
  EXPECT_GT(r3.GetUsage().prompt_tokens, 0);
  EXPECT_GT(r3.GetUsage().completion_tokens, 0);
  EXPECT_EQ(session.TurnCount(), 2u);
  std::cout << "Turn 3 (after undo, same as turn 2): " << t3 << "\n";

  // Turn 4: continue from the replayed turn 3 (which answered "5").
  // Asking "add 1" again should yield "6", proving the session continues correctly
  // after the undo+replay cycle.
  Request req4{
      UserMessage("Now add 1 to that. Answer with just the number."),
  };

  Response r4 = session.ProcessRequest(req4);
  std::string t4 = CollectResponseText(r4);

  EXPECT_NE(r4.GetFinishReason(), FOUNDRY_LOCAL_FINISH_NONE);
  EXPECT_NE(r4.GetFinishReason(), FOUNDRY_LOCAL_FINISH_ERROR);
  EXPECT_NE(t4.find("6"), std::string::npos)
      << "Turn 4: expected '6'. Got: " << t4;
  EXPECT_GT(r4.GetUsage().prompt_tokens, 0);
  EXPECT_GT(r4.GetUsage().completion_tokens, 0);
  EXPECT_EQ(session.TurnCount(), 3u);
  std::cout << "Turn 4: " << t4 << "\n";
}

// --- Tool Calling Tests ---

TEST_F(ToolCallFixture, ToolCallWithRequired) {
  using namespace foundry_local;

  ChatSession session(tool_model());

  session.AddToolDefinition(ToolDefinition{
      "multiply_numbers",
      "A tool for multiplying two numbers.",
      R"({
        "type": "object",
        "properties": {
          "first": { "type": "integer", "description": "The first number in the operation" },
          "second": { "type": "integer", "description": "The second number in the operation" }
        },
        "required": ["first", "second"]
      })"});

  Request request{
      SystemMessage("You are a helpful AI assistant. If necessary, you can use any provided tools to answer the question."),
      UserMessage("What is the answer to 7 multiplied by 6?"),
  };
  request.SetOptions({{FOUNDRY_LOCAL_PARAM_TOOL_CHOICE, "required"},
                      {FOUNDRY_LOCAL_PARAM_TEMPERATURE, "0"},
                      {FOUNDRY_LOCAL_PARAM_MAX_OUTPUT_TOKENS, "256"}});

  Response response = session.ProcessRequest(request);

  EXPECT_EQ(response.GetFinishReason(), FOUNDRY_LOCAL_FINISH_TOOL_CALLS)
      << "Expected tool_calls finish reason";

  // Find the tool call item in the response
  bool found_tool_call = false;
  for (const auto& item : response.GetItems()) {
    if (item.GetType() == FOUNDRY_LOCAL_ITEM_TOOL_CALL) {
      auto tc = item.GetToolCall();
      EXPECT_EQ(tc.name, "multiply_numbers")
          << "Tool call name mismatch. Got: " << tc.name;
      EXPECT_FALSE(tc.arguments.empty());
      std::cout << "Tool call: " << tc.name << "(" << tc.arguments << ")\n";
      found_tool_call = true;
      break;
    }
  }

  EXPECT_TRUE(found_tool_call) << "No TOOL_CALL item in response";
}

TEST_F(ToolCallFixture, ToolCallWithResult) {
  using namespace foundry_local;

  // --- Turn 1: Force a tool call ---
  ChatSession session(tool_model());

  session.AddToolDefinition(ToolDefinition{
      "multiply_numbers",
      "A tool for multiplying two numbers.",
      R"({
        "type": "object",
        "properties": {
          "first": { "type": "integer", "description": "The first number in the operation" },
          "second": { "type": "integer", "description": "The second number in the operation" }
        },
        "required": ["first", "second"]
      })"});

  Request request1{
      SystemMessage("You are a helpful AI assistant. If necessary, you can use any provided tools to answer the question."),
      UserMessage("What is the answer to 7 multiplied by 6?"),
  };
  request1.SetOptions({{FOUNDRY_LOCAL_PARAM_TOOL_CHOICE, "required"},
                       {FOUNDRY_LOCAL_PARAM_TEMPERATURE, "0"},
                       {FOUNDRY_LOCAL_PARAM_MAX_OUTPUT_TOKENS, "256"}});

  Response response1 = session.ProcessRequest(request1);

  ASSERT_EQ(response1.GetFinishReason(), FOUNDRY_LOCAL_FINISH_TOOL_CALLS)
      << "Turn 1 should have tool_calls finish reason";

  // Extract the tool call from the response
  std::string tool_call_id;
  std::string tool_name;
  std::string tool_args;

  for (const auto& item : response1.GetItems()) {
    if (item.GetType() == FOUNDRY_LOCAL_ITEM_TOOL_CALL) {
      auto tc = item.GetToolCall();
      tool_call_id = tc.call_id;
      tool_name = tc.name;
      tool_args = tc.arguments;
      break;
    }
  }

  ASSERT_FALSE(tool_name.empty()) << "No TOOL_CALL item in turn 1 response";
  EXPECT_EQ(tool_name, "multiply_numbers");
  std::cout << "Tool call: " << tool_name << "(" << tool_args << ") id=" << tool_call_id << "\n";

  // --- Turn 2: Supply the tool result via the same session ---
  // The session accumulates history from turn 1 (system, user, assistant+tool_calls).
  // We only need to provide new input: the tool result and a follow-up prompt.
  Request request2{};
  request2.AddItem(Item::ToolResult(tool_call_id, "42"));
  request2.AddItem(SystemMessage("Respond only with the answer generated by the tool."));
  request2.SetOptions({{FOUNDRY_LOCAL_PARAM_TEMPERATURE, "0"},
                       {FOUNDRY_LOCAL_PARAM_MAX_OUTPUT_TOKENS, "256"}});

  Response response2 = session.ProcessRequest(request2);

  EXPECT_EQ(response2.GetFinishReason(), FOUNDRY_LOCAL_FINISH_STOP)
      << "Turn 2 should have stop finish reason";

  std::string final_text = CollectResponseText(response2);
  std::cout << "Turn 2 response: " << final_text << "\n";

  EXPECT_NE(final_text.find("42"), std::string::npos)
      << "Expected '42' in final response. Got: " << final_text;
}

// ========================================================================
// OPENAI_JSON pass-through tests on ChatSession (public C++ API).
//
// JSON requests are documented as stateless — they always create a fresh
// generator and never use the cache (chat_session.h:27-29). These tests
// lock that contract down at the public-API layer where every other-language
// SDK consumes it.
// ========================================================================

namespace {

// Run a JSON request through ChatSession via the public API. Asserts the
// shape of the response (single TEXT item, OPENAI_JSON subtype) and returns
// the raw payload text for the caller to parse.
std::string RunChatOpenAIJsonRequest(foundry_local::ChatSession& session,
                                     const std::string& request_json) {
  using namespace foundry_local;

  Request request;
  request.AddItem(Item::Text(request_json, FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON));

  Response response = session.ProcessRequest(request);

  const auto& items = response.GetItems();
  EXPECT_EQ(items.size(), 1u) << "OpenAI JSON path must produce exactly one response item";

  if (items.size() != 1) {
    return {};
  }

  EXPECT_EQ(items[0].GetType(), FOUNDRY_LOCAL_ITEM_TEXT);

  auto tc = items[0].GetText();
  EXPECT_EQ(tc.type, FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON);

  return tc.text;
}

}  // namespace

// ------------------------------------------------------------------------
// Single-turn JSON request: model returns assistant message containing the
// expected answer; the response parses cleanly as a ChatCompletionResponse.
// ------------------------------------------------------------------------
TEST_F(ModelFixture, OpenAIJsonSingleTurn) {
  using namespace foundry_local;

  ChatSession session(chat_model());

  json req = {
      {"model", model_id()},
      {"messages", json::array({
                       {{"role", "user"}, {"content", "What is 2+2? Answer with just the number."}},
                   })},
      {"temperature", 0},
      {"max_tokens", 256},
  };

  std::string response_text = RunChatOpenAIJsonRequest(session, req.dump());
  ASSERT_FALSE(response_text.empty());

  auto resp_json = nlohmann::json::parse(response_text);
  auto chat_response = resp_json.get<fl::ChatCompletionResponse>();

  ASSERT_EQ(chat_response.choices.size(), 1u);

  const auto& choice = chat_response.choices[0];
  EXPECT_EQ(choice.message.role, "assistant");

  ASSERT_TRUE(choice.message.content.has_value()) << "Expected assistant content";
  const std::string& content = *choice.message.content;
  EXPECT_FALSE(content.empty());
  EXPECT_NE(content.find("4"), std::string::npos)
      << "Expected '4' in response. Got: " << content;

  EXPECT_GT(chat_response.usage.total_tokens, 0);

  EXPECT_TRUE(choice.finish_reason == "stop" || choice.finish_reason == "length")
      << "Unexpected finish_reason: " << choice.finish_reason;

  std::cout << "OpenAI-JSON single-turn output: " << content << "\n";
}

// ------------------------------------------------------------------------
// JSON requests are stateless: TurnCount() must NOT advance across two
// JSON calls on the same session. Both calls must succeed independently.
// This is the documented contract in chat_session.h:27-29.
// ------------------------------------------------------------------------
TEST_F(ModelFixture, OpenAIJsonMultipleRequestsAreStateless) {
  using namespace foundry_local;

  ChatSession session(chat_model());

  json req = {
      {"model", model_id()},
      {"messages", json::array({
                       {{"role", "user"}, {"content", "What is 2+2? Answer with just the number."}},
                   })},
      {"temperature", 0},
      {"max_tokens", 64},
  };

  ASSERT_EQ(session.TurnCount(), 0u);

  std::string response_text_1 = RunChatOpenAIJsonRequest(session, req.dump());
  ASSERT_FALSE(response_text_1.empty());

  EXPECT_EQ(session.TurnCount(), 0u)
      << "JSON requests must not commit to history (TurnCount must stay 0)";

  std::string response_text_2 = RunChatOpenAIJsonRequest(session, req.dump());
  ASSERT_FALSE(response_text_2.empty());

  EXPECT_EQ(session.TurnCount(), 0u)
      << "Second JSON request must also leave TurnCount at 0";

  // Both responses must parse and produce non-empty assistant content.
  auto chat_response_1 = nlohmann::json::parse(response_text_1).get<fl::ChatCompletionResponse>();
  auto chat_response_2 = nlohmann::json::parse(response_text_2).get<fl::ChatCompletionResponse>();

  ASSERT_EQ(chat_response_1.choices.size(), 1u);
  ASSERT_EQ(chat_response_2.choices.size(), 1u);

  ASSERT_TRUE(chat_response_1.choices[0].message.content.has_value());
  ASSERT_TRUE(chat_response_2.choices[0].message.content.has_value());

  const std::string& content_1 = *chat_response_1.choices[0].message.content;
  const std::string& content_2 = *chat_response_2.choices[0].message.content;

  EXPECT_FALSE(content_1.empty());
  EXPECT_FALSE(content_2.empty());

  // The prompt is "What is 2+2?" — both independent replies must contain the
  // correct answer. Asserts the JSON path actually executes inference rather
  // than returning some empty/cached/stale state.
  EXPECT_NE(content_1.find("4"), std::string::npos)
      << "First JSON reply missing '4'. Got: " << content_1;
  EXPECT_NE(content_2.find("4"), std::string::npos)
      << "Second JSON reply missing '4'. Got: " << content_2;
}

// ------------------------------------------------------------------------
// JSON request with a tools[] array. Model behavior varies by version —
// it may invoke the tool or just answer in prose. Both shapes are valid;
// we assert only that the response parses and contains *some* assistant
// signal (tool_calls OR content).
// ------------------------------------------------------------------------
TEST_F(ToolCallFixture, OpenAIJsonWithToolDefinition) {
  using namespace foundry_local;

  ChatSession session(tool_model());

  std::string tool_model_id(tool_model().GetInfo().Id());

  json req = {
      {"model", tool_model_id},
      {"messages", json::array({
                       {{"role", "system"},
                        {"content", "You are a helpful AI assistant. Use the provided tool when appropriate."}},
                       {{"role", "user"}, {"content", "What is 7 multiplied by 6?"}},
                   })},
      {"tools", json::array({
                    {{"type", "function"},
                     {"function",
                      {{"name", "multiply_numbers"},
                       {"description", "A tool for multiplying two numbers."},
                       {"parameters",
                        {{"type", "object"},
                         {"properties",
                          {{"first", {{"type", "integer"}, {"description", "first number"}}},
                           {"second", {{"type", "integer"}, {"description", "second number"}}}}},
                         {"required", json::array({"first", "second"})}}}}}},
                })},
      {"temperature", 0},
      {"max_tokens", 256},
  };

  std::string response_text = RunChatOpenAIJsonRequest(session, req.dump());
  ASSERT_FALSE(response_text.empty());

  auto resp_json = nlohmann::json::parse(response_text);
  auto chat_response = resp_json.get<fl::ChatCompletionResponse>();

  ASSERT_EQ(chat_response.choices.size(), 1u);
  const auto& msg = chat_response.choices[0].message;

  bool has_tool_calls = msg.tool_calls.has_value() && !msg.tool_calls->empty();
  bool has_content = msg.content.has_value() && !msg.content->empty();

  ASSERT_TRUE(has_tool_calls || has_content)
      << "Expected either tool_calls or content in assistant response";

  if (has_tool_calls) {
    // Tool-calling path: the model invoked our tool. Validate the call shape
    // matches the contract — correct function name and the two integer
    // arguments {7, 6} (order-insensitive: model may legitimately swap).
    const auto& call = (*msg.tool_calls)[0];
    EXPECT_EQ(call.function.name, "multiply_numbers")
        << "Model invoked unexpected tool: " << call.function.name;

    auto args = nlohmann::json::parse(call.function.arguments);
    ASSERT_TRUE(args.contains("first")) << "Tool args missing 'first': " << call.function.arguments;
    ASSERT_TRUE(args.contains("second")) << "Tool args missing 'second': " << call.function.arguments;

    int first = args["first"].get<int>();
    int second = args["second"].get<int>();

    EXPECT_TRUE((first == 7 && second == 6) || (first == 6 && second == 7))
        << "Tool args don't match the prompt (7 * 6). Got first=" << first
        << ", second=" << second;

    std::cout << "Tool-call JSON path: model invoked multiply_numbers(" << first << ", " << second << ")\n";
  } else {
    // Direct-answer path: model declined to call the tool. Reply must still
    // contain the correct product, otherwise the model didn't actually answer
    // the question.
    EXPECT_NE(msg.content->find("42"), std::string::npos)
        << "Direct-answer reply missing '42'. Got: " << *msg.content;

    std::cout << "Tool-call JSON path: model produced content: " << *msg.content << "\n";
  }
}

// ------------------------------------------------------------------------
// Malformed JSON in the OPENAI_JSON TextItem body propagates through the
// public-API boundary as a foundry_local::Error (the C ABI wraps all
// exceptions, including nlohmann's parse_error, but preserves the message).
// ------------------------------------------------------------------------
TEST_F(ModelFixture, OpenAIJsonInvalidJsonThrows) {
  using namespace foundry_local;

  ChatSession session(chat_model());

  Request request;
  request.AddItem(Item::Text("not valid json {{{", FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON));

  try {
    session.ProcessRequest(request);
    FAIL() << "Expected ProcessRequest to throw on malformed JSON";
  } catch (const Error& e) {
    EXPECT_NE(std::string(e.what()).find("parse_error"), std::string::npos)
        << "Expected nlohmann parse_error in message: " << e.what();
  }
}

// ------------------------------------------------------------------------
// Missing "messages" key. ProcessChatCompletionsJson parses the request
// then BuildRequestItems produces an empty list; FL_THROW raises
// FOUNDRY_LOCAL_ERROR_INVALID_USAGE, surfaced through the public C ABI as
// a foundry_local::Error.
// ------------------------------------------------------------------------
TEST_F(ModelFixture, OpenAIJsonMissingMessagesThrows) {
  using namespace foundry_local;

  ChatSession session(chat_model());

  json req = {
      {"model", model_id()},
  };

  Request request;
  request.AddItem(Item::Text(req.dump(), FOUNDRY_LOCAL_TEXT_ITEM_TYPE_OPENAI_JSON));

  EXPECT_THROW(session.ProcessRequest(request), Error);
}
