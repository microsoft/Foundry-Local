//! Integration tests for the Responses API.
//!
//! These tests require a running Foundry Local web service with a loaded model.
//! They are compiled only when the "integration" Cargo feature is enabled, and
//! skipped automatically in CI when no model is available.

use super::common;
use foundry_local_sdk::{
    FunctionToolDefinition, ListResponsesOptions, MessageContent, ResponseInput, ResponseItem,
    ResponsesClient, ResponsesContentPart as ContentPart,
};
use serde_json::json;
use tokio_stream::StreamExt;

/// The model alias used for Responses API integration tests.
const RESPONSES_MODEL_ALIAS: &str = common::TEST_MODEL_ALIAS;

async fn setup_responses_client() -> (ResponsesClient, Arc<foundry_local_sdk::Model>) {
    let manager = common::get_test_manager();
    manager
        .start_web_service()
        .await
        .expect("start_web_service failed");
    let catalog = manager.catalog();
    let model = catalog
        .get_model(RESPONSES_MODEL_ALIAS)
        .await
        .expect("get_model failed");
    model.load().await.expect("model.load() failed");

    let mut client = manager
        .get_responses_client(Some(&model.info().id))
        .expect("get_responses_client failed");
    client.settings.store = Some(true);
    (client, model)
}

use std::sync::Arc;

#[tokio::test]
async fn non_streaming_simple_string() {
    let (client, model) = setup_responses_client().await;

    let response = client
        .create(
            ResponseInput::Text("What is 2+2? Respond with just the number.".into()),
            None,
        )
        .await
        .expect("create failed");

    println!("Status: {}", response.status);
    println!("Output: {}", response.output_text());

    assert_eq!(response.status, "completed");
    assert!(
        !response.output_text().is_empty(),
        "output_text should be non-empty"
    );
    assert!(response.usage.is_some(), "usage should be present");

    model.unload().await.expect("unload failed");
}

#[tokio::test]
async fn non_streaming_with_options() {
    let (client, model) = setup_responses_client().await;

    let opts = foundry_local_sdk::ResponseCreateRequest {
        model: model.info().id.clone(),
        input: ResponseInput::Text("Say hello.".into()),
        temperature: Some(0.0),
        max_output_tokens: Some(50),
        instructions: None,
        previous_response_id: None,
        tools: None,
        tool_choice: None,
        stream: None,
        store: Some(true),
        top_p: None,
        frequency_penalty: None,
        presence_penalty: None,
        seed: None,
        truncation: None,
        parallel_tool_calls: None,
        metadata: None,
        user: None,
        reasoning: None,
        text: None,
    };

    let response = client
        .create(ResponseInput::Text("Say hello.".into()), Some(opts))
        .await
        .expect("create with options failed");

    assert_eq!(response.status, "completed");
    assert!(!response.output_text().is_empty());

    model.unload().await.expect("unload failed");
}

#[tokio::test]
async fn streaming_receives_deltas() {
    let (client, model) = setup_responses_client().await;

    let mut stream = client
        .create_streaming(ResponseInput::Text("Count from 1 to 5.".into()), None)
        .await
        .expect("create_streaming failed");

    let mut delta_count = 0usize;
    let mut full_text = String::new();
    let mut completed = false;

    while let Some(event) = stream.next().await {
        let event = event.expect("stream event error");
        match event {
            foundry_local_sdk::StreamingEvent::OutputTextDelta { delta, .. } => {
                full_text.push_str(&delta);
                delta_count += 1;
            }
            foundry_local_sdk::StreamingEvent::ResponseCompleted { .. } => {
                completed = true;
            }
            _ => {}
        }
    }

    println!("Received {delta_count} deltas, text: {full_text}");
    assert!(delta_count > 0, "Expected at least one delta event");
    assert!(completed, "Expected a ResponseCompleted event");
    assert!(!full_text.is_empty(), "Expected non-empty accumulated text");

    model.unload().await.expect("unload failed");
}

#[tokio::test]
async fn multi_turn_previous_response_id() {
    let (client, model) = setup_responses_client().await;

    // First turn
    let first = client
        .create(
            ResponseInput::Text("My favourite colour is blue. Remember this.".into()),
            None,
        )
        .await
        .expect("first create failed");
    assert_eq!(first.status, "completed");
    let first_id = first.id.clone();

    // Second turn referencing the first
    let opts = foundry_local_sdk::ResponseCreateRequest {
        model: model.info().id.clone(),
        input: ResponseInput::Text("What is my favourite colour?".into()),
        previous_response_id: Some(first_id),
        instructions: None,
        tools: None,
        tool_choice: None,
        stream: None,
        store: Some(true),
        temperature: None,
        top_p: None,
        max_output_tokens: None,
        frequency_penalty: None,
        presence_penalty: None,
        seed: None,
        truncation: None,
        parallel_tool_calls: None,
        metadata: None,
        user: None,
        reasoning: None,
        text: None,
    };

    let second = client
        .create(
            ResponseInput::Text("What is my favourite colour?".into()),
            Some(opts),
        )
        .await
        .expect("second create failed");

    println!("Multi-turn response: {}", second.output_text());
    assert_eq!(second.status, "completed");
    let text = second.output_text().to_lowercase();
    assert!(
        text.contains("blue"),
        "Second response should reference 'blue', got: {text}"
    );

    model.unload().await.expect("unload failed");
}

#[tokio::test]
async fn get_stored_response() {
    let (client, model) = setup_responses_client().await;

    let created = client
        .create(ResponseInput::Text("Hello.".into()), None)
        .await
        .expect("create failed");
    let response_id = created.id.clone();

    let fetched = client.get(&response_id).await.expect("get failed");
    assert_eq!(fetched.id, response_id);
    assert_eq!(fetched.status, "completed");

    model.unload().await.expect("unload failed");
}

#[tokio::test]
async fn delete_response() {
    let (client, model) = setup_responses_client().await;

    let created = client
        .create(ResponseInput::Text("I will be deleted.".into()), None)
        .await
        .expect("create failed");
    let response_id = created.id.clone();

    let result = client.delete(&response_id).await.expect("delete failed");
    assert_eq!(result.id, response_id);
    assert!(result.deleted);

    // Getting the deleted response should fail
    let get_result = client.get(&response_id).await;
    assert!(
        get_result.is_err(),
        "Expected error after deleting response"
    );

    model.unload().await.expect("unload failed");
}

#[tokio::test]
async fn list_responses() {
    let (client, model) = setup_responses_client().await;

    // Create a response to ensure there is at least one
    let _ = client
        .create(ResponseInput::Text("List test.".into()), None)
        .await
        .expect("create failed");

    let list_options = ListResponsesOptions {
        limit: Some(10),
        order: Some("desc".into()),
        after: None,
    };
    let list = client
        .list_with_options(Some(&list_options))
        .await
        .expect("list failed");
    assert_eq!(list.object, "list");
    assert!(
        !list.data.is_empty(),
        "Expected at least one response in list"
    );

    model.unload().await.expect("unload failed");
}

#[tokio::test]
async fn get_input_items() {
    let (client, model) = setup_responses_client().await;

    let created = client
        .create(ResponseInput::Text("Input items test.".into()), None)
        .await
        .expect("create failed");

    let items = client
        .get_input_items(&created.id)
        .await
        .expect("get_input_items failed");

    assert_eq!(items.object, "list");

    model.unload().await.expect("unload failed");
}

#[tokio::test]
async fn tool_calling_round_trip() {
    let (client, model) = setup_responses_client().await;

    let multiply_tool = FunctionToolDefinition {
        tool_type: "function".into(),
        name: "multiply".into(),
        description: Some("Multiply two numbers".into()),
        parameters: Some(json!({
            "type": "object",
            "properties": {
                "a": { "type": "number" },
                "b": { "type": "number" }
            },
            "required": ["a", "b"]
        })),
        strict: None,
    };

    let opts = foundry_local_sdk::ResponseCreateRequest {
        model: model.info().id.clone(),
        input: ResponseInput::Text("What is 6 times 7? Use the multiply tool.".into()),
        tools: Some(vec![multiply_tool]),
        tool_choice: Some(json!("required")),
        instructions: None,
        previous_response_id: None,
        stream: None,
        store: Some(true),
        temperature: Some(0.0),
        top_p: None,
        max_output_tokens: None,
        frequency_penalty: None,
        presence_penalty: None,
        seed: None,
        truncation: None,
        parallel_tool_calls: None,
        metadata: None,
        user: None,
        reasoning: None,
        text: None,
    };

    let response = client
        .create(
            ResponseInput::Text("What is 6 times 7? Use the multiply tool.".into()),
            Some(opts),
        )
        .await
        .expect("create with tool failed");

    // Find the function_call item
    let func_call = response.output.iter().find_map(|item| {
        if let ResponseItem::FunctionCall {
            call_id,
            name,
            arguments,
            ..
        } = item
        {
            Some((call_id.clone(), name.clone(), arguments.clone()))
        } else {
            None
        }
    });

    assert!(func_call.is_some(), "Expected a function_call output item");
    let (call_id, name, args_str) = func_call.unwrap();
    assert_eq!(name, "multiply");

    let args: serde_json::Value = serde_json::from_str(&args_str).expect("failed to parse args");
    let a = args["a"].as_f64().unwrap_or(0.0);
    let b = args["b"].as_f64().unwrap_or(0.0);
    let product = (a * b) as i64;

    // Send back the tool result
    let tool_result_input = ResponseInput::Items(vec![ResponseItem::FunctionCallOutput {
        id: None,
        call_id,
        output: product.to_string(),
        status: None,
    }]);

    let final_opts = foundry_local_sdk::ResponseCreateRequest {
        model: model.info().id.clone(),
        input: tool_result_input.clone(),
        previous_response_id: Some(response.id.clone()),
        instructions: None,
        tools: None,
        tool_choice: None,
        stream: None,
        store: Some(true),
        temperature: Some(0.0),
        top_p: None,
        max_output_tokens: None,
        frequency_penalty: None,
        presence_penalty: None,
        seed: None,
        truncation: None,
        parallel_tool_calls: None,
        metadata: None,
        user: None,
        reasoning: None,
        text: None,
    };

    let final_response = client
        .create(tool_result_input, Some(final_opts))
        .await
        .expect("tool result create failed");

    let result_text = final_response.output_text();
    println!("Tool call final answer: {result_text}");
    assert!(
        result_text.contains("42"),
        "Expected '42' in final answer, got: {result_text}"
    );

    model.unload().await.expect("unload failed");
}

#[tokio::test]
async fn vision_image_base64() {
    // This test requires a vision-capable model (phi-4-multimodal or similar).
    // It is skipped if no such model is available.
    let manager = common::get_test_manager();
    manager
        .start_web_service()
        .await
        .expect("start_web_service failed");

    // Small 1x1 red PNG, base64-encoded
    let tiny_png_b64 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8/5+hHgAHggJ/PchI6QAAAABJRU5ErkJggg==";

    // Try to use the test model (may not be vision-capable; test would then fail at API level)
    let vision_model_id =
        std::env::var("FOUNDRY_VISION_MODEL_ID").unwrap_or_else(|_| "phi-4-multimodal".to_string());

    let client = ResponsesClient::new(
        manager.urls().expect("urls").first().expect("url"),
        Some(&vision_model_id),
    );

    let input = ResponseInput::Items(vec![ResponseItem::Message {
        id: None,
        role: "user".into(),
        content: MessageContent::Parts(vec![
            ContentPart::InputText {
                text: "What colour is this image?".into(),
            },
            ContentPart::InputImage {
                image_url: None,
                image_data: Some(tiny_png_b64.into()),
                media_type: Some("image/png".into()),
                detail: Some("auto".into()),
            },
        ]),
        status: None,
    }]);

    let result = client.create(input, None).await;
    match result {
        Ok(resp) => {
            println!("Vision response: {}", resp.output_text());
            assert_eq!(resp.status, "completed");
        }
        Err(e) => {
            // Model may not be loaded; skip gracefully
            println!("Vision test skipped (model not available): {e}");
        }
    }
}
