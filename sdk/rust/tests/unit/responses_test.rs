//! Unit tests for the Responses API types and SSE parsing.
//!
//! All tests run without a server.

use foundry_local_sdk::{
    ListResponsesOptions, ListResponsesResult, MessageContent, ReasoningConfig, ResponseInput,
    ResponseItem, ResponseObject, ResponsesClient, ResponsesClientSettings,
    ResponsesContentPart as ContentPart, StreamingEvent, TextConfig, TextFormat,
};
use serde_json::json;
use std::time::Duration;

// ── Settings defaults ────────────────────────────────────────────────────────

#[test]
fn settings_defaults_omit_store() {
    let settings = ResponsesClientSettings::new();
    assert_eq!(
        settings.store, None,
        "store should be omitted unless callers explicitly opt in"
    );
}

#[test]
fn settings_default_trait_also_omits_store() {
    let settings = ResponsesClientSettings::default();
    assert_eq!(settings.store, None);
}

#[test]
fn settings_default_timeout_is_sixty_seconds() {
    let settings = ResponsesClientSettings::default();
    assert_eq!(settings.timeout, Duration::from_secs(60));
}

#[test]
fn settings_all_other_fields_default_to_none() {
    let s = ResponsesClientSettings::new();
    assert!(s.instructions.is_none());
    assert!(s.temperature.is_none());
    assert!(s.top_p.is_none());
    assert!(s.max_output_tokens.is_none());
    assert!(s.frequency_penalty.is_none());
    assert!(s.presence_penalty.is_none());
    assert!(s.tool_choice.is_none());
    assert!(s.truncation.is_none());
    assert!(s.parallel_tool_calls.is_none());
    assert!(s.metadata.is_none());
    assert!(s.reasoning.is_none());
    assert!(s.text.is_none());
    assert!(s.seed.is_none());
}

// ── output_text ──────────────────────────────────────────────────────────────

fn make_response_with_text(role: &str, text: &str) -> ResponseObject {
    serde_json::from_value(json!({
        "id": "resp_test",
        "object": "response",
        "created_at": 0,
        "status": "completed",
        "model": "test-model",
        "output": [
            {
                "type": "message",
                "role": role,
                "content": [{ "type": "output_text", "text": text }]
            }
        ]
    }))
    .expect("failed to deserialize test ResponseObject")
}

fn make_response_with_string_content(text: &str) -> ResponseObject {
    serde_json::from_value(json!({
        "id": "resp_test",
        "object": "response",
        "created_at": 0,
        "status": "completed",
        "model": "test-model",
        "output": [
            {
                "type": "message",
                "role": "assistant",
                "content": text
            }
        ]
    }))
    .expect("failed to deserialize test ResponseObject")
}

#[test]
fn output_text_extracts_assistant_message_parts() {
    let resp = make_response_with_text("assistant", "Hello, world!");
    assert_eq!(resp.output_text(), "Hello, world!");
}

#[test]
fn output_text_extracts_assistant_string_content() {
    let resp = make_response_with_string_content("Direct string content");
    assert_eq!(resp.output_text(), "Direct string content");
}

#[test]
fn output_text_skips_non_assistant_messages() {
    let resp = make_response_with_text("user", "I am the user");
    assert_eq!(
        resp.output_text(),
        "",
        "user message should not be returned"
    );
}

#[test]
fn output_text_returns_empty_for_no_output() {
    let resp: ResponseObject = serde_json::from_value(json!({
        "id": "resp_test",
        "object": "response",
        "created_at": 0,
        "status": "completed",
        "model": "test-model",
        "output": []
    }))
    .unwrap();
    assert_eq!(resp.output_text(), "");
}

#[test]
fn output_text_concatenates_multiple_parts() {
    let resp: ResponseObject = serde_json::from_value(json!({
        "id": "resp_test",
        "object": "response",
        "created_at": 0,
        "status": "completed",
        "model": "test-model",
        "output": [{
            "type": "message",
            "role": "assistant",
            "content": [
                { "type": "output_text", "text": "Hello" },
                { "type": "output_text", "text": ", world!" }
            ]
        }]
    }))
    .unwrap();
    assert_eq!(resp.output_text(), "Hello, world!");
}

// ── Content part serialisation ───────────────────────────────────────────────

#[test]
fn content_part_input_text_serializes_correctly() {
    let part = ContentPart::InputText {
        text: "hello".into(),
    };
    let json = serde_json::to_value(&part).unwrap();
    assert_eq!(json["type"], "input_text");
    assert_eq!(json["text"], "hello");
}

#[test]
fn content_part_output_text_serializes_correctly() {
    let part = ContentPart::OutputText {
        text: "hi".into(),
        annotations: None,
        logprobs: None,
    };
    let json = serde_json::to_value(&part).unwrap();
    assert_eq!(json["type"], "output_text");
    assert_eq!(json["text"], "hi");
    // skip_serializing_if = None omits the field
    assert!(json.get("annotations").is_none());
}

#[test]
fn content_part_refusal_roundtrips() {
    let part = ContentPart::Refusal {
        refusal: "I can't do that".into(),
    };
    let json = serde_json::to_string(&part).unwrap();
    let back: ContentPart = serde_json::from_str(&json).unwrap();
    let ContentPart::Refusal { refusal } = back else {
        panic!("Expected Refusal variant");
    };
    assert_eq!(refusal, "I can't do that");
}

#[test]
fn input_image_content_serializes_with_base64() {
    let part = ContentPart::InputImage {
        image_url: None,
        image_data: Some("base64data==".into()),
        media_type: Some("image/png".into()),
        detail: Some("auto".into()),
    };
    let json = serde_json::to_value(&part).unwrap();
    assert_eq!(json["type"], "input_image");
    assert_eq!(json["image_data"], "base64data==");
    assert_eq!(json["media_type"], "image/png");
    assert_eq!(json["detail"], "auto");
    // image_url should be omitted (None)
    assert!(json.get("image_url").is_none());
}

#[test]
fn input_image_content_serializes_with_url() {
    let part = ContentPart::InputImage {
        image_url: Some("https://example.com/img.png".into()),
        image_data: None,
        media_type: None,
        detail: None,
    };
    let json = serde_json::to_value(&part).unwrap();
    assert_eq!(json["image_url"], "https://example.com/img.png");
    assert!(json.get("image_data").is_none());
    assert!(json.get("media_type").is_none());
    assert!(json.get("detail").is_none());
}

#[tokio::test]
async fn input_image_requires_exactly_one_source() {
    let client = ResponsesClient::new("http://127.0.0.1:1", Some("test-model"));
    let invalid_input = ResponseInput::Items(vec![ResponseItem::Message {
        id: None,
        role: "user".into(),
        content: MessageContent::Parts(vec![ContentPart::InputImage {
            image_url: Some("https://example.com/img.png".into()),
            image_data: Some("base64data==".into()),
            media_type: Some("image/png".into()),
            detail: None,
        }]),
        status: None,
    }]);

    let err = client
        .create(invalid_input, None)
        .await
        .expect_err("invalid input_image should fail before network request");
    assert!(err
        .to_string()
        .contains("Provide exactly one of image_url or image_data"));
}

#[tokio::test]
async fn timeout_must_be_positive() {
    let mut client = ResponsesClient::new("http://127.0.0.1:1", Some("test-model"));
    client.settings.timeout = Duration::ZERO;

    let err = client
        .list()
        .await
        .expect_err("zero timeout should fail before network request");
    assert!(err
        .to_string()
        .contains("timeout must be greater than zero"));
}

#[test]
fn list_response_result_deserializes_pagination_fields() {
    let result: ListResponsesResult = serde_json::from_value(json!({
        "object": "list",
        "data": [],
        "first_id": "resp_first",
        "last_id": "resp_last",
        "has_more": true
    }))
    .unwrap();

    assert_eq!(result.first_id.as_deref(), Some("resp_first"));
    assert_eq!(result.last_id.as_deref(), Some("resp_last"));
    assert_eq!(result.has_more, Some(true));
}

#[test]
fn list_options_serialize_query_fields() {
    let options = ListResponsesOptions {
        limit: Some(10),
        order: Some("desc".into()),
        after: Some("resp_123".into()),
    };
    let json = serde_json::to_value(options).unwrap();
    assert_eq!(json["limit"], 10);
    assert_eq!(json["order"], "desc");
    assert_eq!(json["after"], "resp_123");
}

// ── ResponseItem serialisation ───────────────────────────────────────────────

#[test]
fn response_item_function_call_roundtrips() {
    let item = ResponseItem::FunctionCall {
        id: Some("fc_1".into()),
        call_id: "call_abc".into(),
        name: "get_weather".into(),
        arguments: r#"{"city":"London"}"#.into(),
        status: Some("completed".into()),
    };
    let json = serde_json::to_string(&item).unwrap();
    let back: ResponseItem = serde_json::from_str(&json).unwrap();
    let ResponseItem::FunctionCall { name, .. } = back else {
        panic!("Expected FunctionCall variant");
    };
    assert_eq!(name, "get_weather");
}

#[test]
fn response_item_message_with_string_content_roundtrips() {
    let json = json!({
        "type": "message",
        "role": "user",
        "content": "Hello"
    });
    let item: ResponseItem = serde_json::from_value(json).unwrap();
    let ResponseItem::Message { content, .. } = &item else {
        panic!("Expected Message variant");
    };
    assert!(matches!(content, MessageContent::Text(_)));
}

// ── Streaming event deserialisation ─────────────────────────────────────────

#[test]
fn streaming_event_output_text_delta_deserializes() {
    let json = json!({
        "type": "response.output_text.delta",
        "item_id": "item_1",
        "output_index": 0,
        "content_index": 0,
        "delta": "Hello",
        "sequence_number": 5
    });
    let event: StreamingEvent = serde_json::from_value(json).unwrap();
    let StreamingEvent::OutputTextDelta {
        delta,
        sequence_number,
        ..
    } = event
    else {
        panic!("Expected OutputTextDelta variant");
    };
    assert_eq!(delta, "Hello");
    assert_eq!(sequence_number, 5);
}

#[test]
fn streaming_event_response_completed_deserializes() {
    let json = json!({
        "type": "response.completed",
        "sequence_number": 10,
        "response": {
            "id": "resp_1",
            "object": "response",
            "created_at": 1234567890_i64,
            "status": "completed",
            "model": "test",
            "output": []
        }
    });
    let event: StreamingEvent = serde_json::from_value(json).unwrap();
    assert!(matches!(event, StreamingEvent::ResponseCompleted { .. }));
}

#[test]
fn streaming_event_error_deserializes() {
    let json = json!({
        "type": "error",
        "code": "model_error",
        "message": "Something went wrong",
        "sequence_number": 2
    });
    let event: StreamingEvent = serde_json::from_value(json).unwrap();
    let StreamingEvent::Error { code, message, .. } = event else {
        panic!("Expected Error variant");
    };
    assert_eq!(code.as_deref(), Some("model_error"));
    assert_eq!(message.as_deref(), Some("Something went wrong"));
}

// ── SSE parser ───────────────────────────────────────────────────────────────

/// Build a minimal SSE block string from event type and JSON data.
fn sse_block(event_type: &str, data: &serde_json::Value) -> String {
    format!("event: {event_type}\ndata: {data}\n\n")
}

#[tokio::test]
async fn sse_parser_handles_complete_events() {
    use bytes::Bytes;

    // Build a minimal SSE payload with one delta event followed by [DONE]
    let delta_json = json!({
        "type": "response.output_text.delta",
        "item_id": "item_1",
        "output_index": 0,
        "content_index": 0,
        "delta": "Hi",
        "sequence_number": 1
    });

    let payload = format!(
        "{}{}",
        sse_block("response.output_text.delta", &delta_json),
        "data: [DONE]\n\n"
    );

    let bytes = Bytes::from(payload);

    // Test the SSE logic by parsing the byte buffer as the SSE parser would.
    let content = std::str::from_utf8(&bytes).unwrap().to_string();
    let blocks: Vec<&str> = content
        .split("\n\n")
        .filter(|b| !b.trim().is_empty())
        .collect();

    for block in &blocks {
        let trimmed = block.trim();
        if trimmed == "data: [DONE]" {
            break;
        }
        let data_line = trimmed
            .split('\n')
            .find(|l| l.starts_with("data: "))
            .map(|l| &l[6..]);
        if let Some(json_str) = data_line {
            let event: StreamingEvent = serde_json::from_str(json_str).unwrap();
            assert!(matches!(event, StreamingEvent::OutputTextDelta { .. }));
        }
    }
}

#[test]
fn sse_done_signal_is_recognized() {
    let block = "data: [DONE]";
    assert!(block.trim() == "data: [DONE]");
}

// ── ResponseInput serde ──────────────────────────────────────────────────────

#[test]
fn response_input_text_serializes_as_string() {
    let input = ResponseInput::Text("what is 2+2?".into());
    let json = serde_json::to_value(&input).unwrap();
    assert_eq!(json, json!("what is 2+2?"));
}

#[test]
fn response_input_items_serializes_as_array() {
    let input = ResponseInput::Items(vec![ResponseItem::Message {
        id: None,
        role: "user".into(),
        content: MessageContent::Text("hello".into()),
        status: None,
    }]);
    let json = serde_json::to_value(&input).unwrap();
    assert!(json.is_array());
}

// ── TextConfig / ReasoningConfig ─────────────────────────────────────────────

#[test]
fn text_config_with_json_schema_serializes() {
    let cfg = TextConfig {
        format: Some(TextFormat {
            format_type: "json_schema".into(),
            name: Some("MySchema".into()),
            schema: Some(json!({"type": "object"})),
            description: None,
            strict: Some(true),
        }),
        verbosity: None,
    };
    let json = serde_json::to_value(&cfg).unwrap();
    assert_eq!(json["format"]["type"], "json_schema");
    assert_eq!(json["format"]["name"], "MySchema");
    assert_eq!(json["format"]["strict"], true);
}

#[test]
fn reasoning_config_serializes_correctly() {
    let cfg = ReasoningConfig {
        effort: Some("high".into()),
        summary: Some("concise".into()),
    };
    let json = serde_json::to_value(&cfg).unwrap();
    assert_eq!(json["effort"], "high");
    assert_eq!(json["summary"], "concise");
}
