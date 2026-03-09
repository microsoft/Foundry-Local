//! Integration tests for the [`ChatClient`] (non-streaming, streaming, and
//! tool-calling variants).
//!
//! Mirrors `chatClient.test.ts` from the JavaScript SDK.

mod common;

use foundry_local_sdk::openai::ChatClient;
use foundry_local_sdk::{
    ChatCompletionMessageToolCalls, ChatCompletionRequestMessage,
    ChatCompletionRequestSystemMessage, ChatCompletionRequestToolMessage,
    ChatCompletionRequestUserMessage, ChatToolChoice,
};
use serde_json::json;
use tokio_stream::StreamExt;

mod tests {
    use super::*;

    // ── Helpers ──────────────────────────────────────────────────────────

    /// Load the test model and return a [`ChatClient`] ready for use.
    async fn setup_chat_client() -> ChatClient {
        let manager = common::get_test_manager();
        let catalog = manager.catalog();
        let model = catalog
            .get_model(common::TEST_MODEL_ALIAS)
            .await
            .expect("get_model failed");
        model.load().await.expect("model.load() failed");

        let mut client = model.create_chat_client();
        client.max_tokens(500).temperature(0.0);
        client
    }

    fn user_message(content: &str) -> ChatCompletionRequestMessage {
        ChatCompletionRequestUserMessage::from(content).into()
    }

    fn system_message(content: &str) -> ChatCompletionRequestMessage {
        ChatCompletionRequestSystemMessage::from(content).into()
    }

    fn assistant_message(content: &str) -> ChatCompletionRequestMessage {
        serde_json::from_value(json!({ "role": "assistant", "content": content }))
            .expect("failed to construct assistant message")
    }

    // ── Non-streaming completion ─────────────────────────────────────────

    #[tokio::test]
    async fn should_perform_chat_completion() {
        let client = setup_chat_client().await;
        let messages = vec![
            system_message("You are a helpful math assistant. Respond with just the answer."),
            user_message("What is 7*6?"),
        ];

        let response = client
            .complete_chat(&messages, None)
            .await
            .expect("complete_chat failed");
        let content = response
            .choices
            .first()
            .and_then(|c| c.message.content.as_deref())
            .unwrap_or("");

        assert!(
            content.contains("42"),
            "Expected response to contain '42', got: {content}"
        );
    }

    // ── Streaming completion ─────────────────────────────────────────────

    #[tokio::test]
    async fn should_perform_streaming_chat_completion() {
        let client = setup_chat_client().await;
        let mut messages = vec![
            system_message("You are a helpful math assistant. Respond with just the answer."),
            user_message("What is 7*6?"),
        ];

        // First turn — expect "42"
        let mut first_result = String::new();
        let mut stream = client
            .complete_streaming_chat(&messages, None)
            .await
            .expect("streaming chat (first turn) setup failed");
        while let Some(chunk) = stream.next().await {
            let chunk = chunk.expect("stream chunk error");
            if let Some(choice) = chunk.choices.first() {
                if let Some(ref content) = choice.delta.content {
                    first_result.push_str(content);
                }
            }
        }
        stream.close().await.expect("stream close failed");

        assert!(
            first_result.contains("42"),
            "First turn should contain '42', got: {first_result}"
        );

        // Follow-up turn — expect "67"
        messages.push(assistant_message(&first_result));
        messages.push(user_message("Now add 25 to that result."));

        let mut second_result = String::new();
        let mut stream = client
            .complete_streaming_chat(&messages, None)
            .await
            .expect("streaming chat (follow-up) setup failed");
        while let Some(chunk) = stream.next().await {
            let chunk = chunk.expect("stream chunk error");
            if let Some(choice) = chunk.choices.first() {
                if let Some(ref content) = choice.delta.content {
                    second_result.push_str(content);
                }
            }
        }
        stream.close().await.expect("stream close failed");

        assert!(
            second_result.contains("67"),
            "Follow-up should contain '67', got: {second_result}"
        );
    }

    // ── Validation: empty / invalid messages ─────────────────────────────

    #[tokio::test]
    async fn should_throw_when_completing_chat_with_empty_messages() {
        let client = setup_chat_client().await;
        let messages: Vec<ChatCompletionRequestMessage> = vec![];

        let result = client.complete_chat(&messages, None).await;
        assert!(result.is_err(), "Expected error for empty messages");
    }

    #[tokio::test]
    async fn should_throw_when_completing_streaming_chat_with_empty_messages() {
        let client = setup_chat_client().await;
        let messages: Vec<ChatCompletionRequestMessage> = vec![];

        let result = client.complete_streaming_chat(&messages, None).await;
        assert!(
            result.is_err(),
            "Expected error for empty messages in streaming"
        );
    }

    #[tokio::test]
    async fn should_throw_when_completing_streaming_chat_with_invalid_callback() {
        let client = setup_chat_client().await;
        let messages: Vec<ChatCompletionRequestMessage> = vec![];

        let result = client.complete_streaming_chat(&messages, None).await;
        assert!(result.is_err(), "Expected error even with empty messages");
    }

    // ── Tool calling (non-streaming) ─────────────────────────────────────

    #[tokio::test]
    async fn should_perform_tool_calling_chat_completion_non_streaming() {
        let mut client = setup_chat_client().await;
        client.tool_choice(ChatToolChoice::Required);

        let tools = vec![common::get_multiply_tool()];
        let mut messages = vec![
            system_message("You are a math assistant. Use the multiply tool to answer."),
            user_message("What is 6 times 7?"),
        ];

        // Step 1 — the model should request the multiply tool.
        let response = client
            .complete_chat(&messages, Some(&tools))
            .await
            .expect("complete_chat with tools failed");

        let choice = response
            .choices
            .first()
            .expect("Expected at least one choice");
        let tool_calls = choice
            .message
            .tool_calls
            .as_ref()
            .expect("Expected tool_calls");
        assert!(
            !tool_calls.is_empty(),
            "Expected at least one tool call in the response"
        );

        let tool_call = match &tool_calls[0] {
            ChatCompletionMessageToolCalls::Function(tc) => tc,
            _ => panic!("Expected a function tool call"),
        };
        assert_eq!(
            tool_call.function.name, "multiply",
            "Expected tool call to 'multiply'"
        );

        // Parse arguments and compute the result.
        let args: serde_json::Value = serde_json::from_str(&tool_call.function.arguments)
            .expect("Failed to parse tool call arguments");
        let a = args["a"].as_f64().unwrap_or(0.0);
        let b = args["b"].as_f64().unwrap_or(0.0);
        let product = (a * b) as i64;

        // Step 2 — feed the tool result back and get the final answer.
        let tool_call_id = &tool_call.id;
        let assistant_msg: ChatCompletionRequestMessage = serde_json::from_value(json!({
            "role": "assistant",
            "content": null,
            "tool_calls": [{
                "id": tool_call_id,
                "type": "function",
                "function": {
                    "name": tool_call.function.name,
                    "arguments": tool_call.function.arguments,
                }
            }]
        }))
        .expect("failed to construct assistant message");
        messages.push(assistant_msg);
        messages.push(
            ChatCompletionRequestToolMessage {
                content: product.to_string().into(),
                tool_call_id: tool_call_id.clone(),
            }
            .into(),
        );

        // Switch to auto so the model can answer freely.
        client.tool_choice(ChatToolChoice::Auto);

        let final_response = client
            .complete_chat(&messages, Some(&tools))
            .await
            .expect("follow-up complete_chat with tools failed");
        let content = final_response
            .choices
            .first()
            .and_then(|c| c.message.content.as_deref())
            .unwrap_or("");

        assert!(
            content.contains("42"),
            "Final answer should contain '42', got: {content}"
        );
    }

    // ── Tool calling (streaming) ─────────────────────────────────────────

    #[tokio::test]
    async fn should_perform_tool_calling_chat_completion_streaming() {
        let mut client = setup_chat_client().await;
        client.tool_choice(ChatToolChoice::Required);

        let tools = vec![common::get_multiply_tool()];
        let mut messages = vec![
            system_message("You are a math assistant. Use the multiply tool to answer."),
            user_message("What is 6 times 7?"),
        ];

        // Step 1 — collect streaming tool call chunks.
        let mut tool_call_name = String::new();
        let mut tool_call_args = String::new();
        let mut tool_call_id = String::new();

        let mut stream = client
            .complete_streaming_chat(&messages, Some(&tools))
            .await
            .expect("streaming tool call setup failed");

        while let Some(chunk) = stream.next().await {
            let chunk = chunk.expect("stream chunk error");
            if let Some(choice) = chunk.choices.first() {
                if let Some(ref tool_calls) = choice.delta.tool_calls {
                    for call in tool_calls {
                        if let Some(ref func) = call.function {
                            if let Some(ref name) = func.name {
                                tool_call_name.push_str(name);
                            }
                            if let Some(ref args) = func.arguments {
                                tool_call_args.push_str(args);
                            }
                        }
                        if let Some(ref id) = call.id {
                            tool_call_id = id.clone();
                        }
                    }
                }
            }
        }
        stream.close().await.expect("stream close failed");

        assert_eq!(
            tool_call_name, "multiply",
            "Expected streamed tool call to 'multiply'"
        );

        // Parse arguments and compute.
        let args: serde_json::Value =
            serde_json::from_str(&tool_call_args).unwrap_or_else(|_| json!({}));
        let a = args["a"].as_f64().unwrap_or(0.0);
        let b = args["b"].as_f64().unwrap_or(0.0);
        let product = (a * b) as i64;

        // Step 2 — feed the result back and stream the final answer.
        let assistant_msg: ChatCompletionRequestMessage = serde_json::from_value(json!({
            "role": "assistant",
            "tool_calls": [{
                "id": tool_call_id,
                "type": "function",
                "function": {
                    "name": tool_call_name,
                    "arguments": tool_call_args
                }
            }]
        }))
        .expect("failed to construct assistant message");
        messages.push(assistant_msg);
        messages.push(
            ChatCompletionRequestToolMessage {
                content: product.to_string().into(),
                tool_call_id: tool_call_id.clone(),
            }
            .into(),
        );

        client.tool_choice(ChatToolChoice::Auto);

        let mut final_result = String::new();
        let mut stream = client
            .complete_streaming_chat(&messages, Some(&tools))
            .await
            .expect("streaming follow-up setup failed");
        while let Some(chunk) = stream.next().await {
            let chunk = chunk.expect("stream chunk error");
            if let Some(choice) = chunk.choices.first() {
                if let Some(ref content) = choice.delta.content {
                    final_result.push_str(content);
                }
            }
        }
        stream.close().await.expect("stream close failed");

        assert!(
            final_result.contains("42"),
            "Streamed final answer should contain '42', got: {final_result}"
        );
    }
}
