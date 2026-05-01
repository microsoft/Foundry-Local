use super::common;
use serde_json::{json, Value};
use std::sync::Arc;

type TestResult<T> = Result<T, Box<dyn std::error::Error + Send + Sync>>;

static RESPONSES_TEST_LOCK: tokio::sync::Mutex<()> = tokio::sync::Mutex::const_new(());

struct ResponsesServiceContext {
    manager: &'static foundry_local_sdk::FoundryLocalManager,
    model: Arc<foundry_local_sdk::Model>,
    base_url: String,
    http: reqwest::Client,
}

impl ResponsesServiceContext {
    async fn start() -> Option<Self> {
        let manager = common::get_test_manager();
        let catalog = manager.catalog();
        let model = match catalog.get_model(common::TEST_MODEL_ALIAS).await {
            Ok(model) => model,
            Err(e) => {
                eprintln!(
                    "Skipping Responses web-service test: model '{}' unavailable: {e}",
                    common::TEST_MODEL_ALIAS
                );
                return None;
            }
        };

        if !model.is_cached().await.unwrap_or(false) {
            eprintln!(
                "Skipping Responses web-service test: model '{}' is not cached",
                common::TEST_MODEL_ALIAS
            );
            return None;
        }

        model.load().await.expect("model.load() failed");
        manager
            .start_web_service()
            .await
            .expect("start_web_service failed");

        let base_url = format!(
            "{}/v1",
            manager
                .urls()
                .expect("urls() should succeed")
                .first()
                .expect("no URL returned")
                .trim_end_matches('/')
        );

        Some(Self {
            manager,
            model,
            base_url,
            http: reqwest::Client::new(),
        })
    }

    async fn cleanup(&self) {
        self.manager
            .stop_web_service()
            .await
            .expect("stop_web_service failed");
        self.model.unload().await.expect("model.unload() failed");
    }
}

#[tokio::test]
async fn should_create_non_streaming_response_via_rest_api() {
    let _guard = RESPONSES_TEST_LOCK.lock().await;
    let Some(ctx) = ResponsesServiceContext::start().await else {
        return;
    };

    let result = post_response_json(
        &ctx,
        json!({
            "model": ctx.model.id(),
            "input": "What is 2 + 2? Respond with just the answer.",
            "temperature": 0.0
        }),
    )
    .await;

    ctx.cleanup().await;

    let body = result.expect("Responses non-streaming request failed");
    let text = output_text(&body);
    println!("Responses non-streaming text: {text}");
    assert!(!text.trim().is_empty(), "response text should not be empty");
}

#[tokio::test]
async fn should_stream_response_via_rest_api() {
    let _guard = RESPONSES_TEST_LOCK.lock().await;
    let Some(ctx) = ResponsesServiceContext::start().await else {
        return;
    };

    let result = async {
        let response = ctx
            .http
            .post(format!("{}/responses", ctx.base_url))
            .json(&json!({
                "model": ctx.model.id(),
                "input": "Count from 1 to 3.",
                "temperature": 0.0,
                "stream": true
            }))
            .send()
            .await?;

        read_responses_sse(response).await
    }
    .await;

    ctx.cleanup().await;

    let summary = result.expect("Responses streaming request failed");
    assert!(
        summary.delta_count > 0,
        "expected at least one response.output_text.delta event"
    );
    assert!(
        summary.completed,
        "expected a response.completed event in the stream"
    );
}

#[tokio::test]
async fn should_complete_tool_calling_response_via_rest_api() {
    let _guard = RESPONSES_TEST_LOCK.lock().await;
    let Some(ctx) = ResponsesServiceContext::start().await else {
        return;
    };

    let result = async {
        let weather_tool = get_weather_tool();
        let tool_response = post_response_json(
            &ctx,
            json!({
                "model": ctx.model.id(),
                "input": "Use the get_weather tool for Seattle, then answer.",
                "tools": [weather_tool.clone()],
                "tool_choice": "required",
                "temperature": 0.0,
                "store": true
            }),
        )
        .await?;

        let (call_id, name) = find_function_call(&tool_response)
            .ok_or("expected a function_call item in the tool response")?;
        if name != "get_weather" {
            return Err(format!("expected get_weather function call, got {name}").into());
        }

        let final_response = post_response_json(
            &ctx,
            json!({
                "model": ctx.model.id(),
                "previous_response_id": tool_response["id"].clone(),
                "input": [{
                    "type": "function_call_output",
                    "call_id": call_id,
                    "output": "Seattle weather is 72F and sunny."
                }],
                "tools": [weather_tool],
                "temperature": 0.0
            }),
        )
        .await?;

        Ok::<String, Box<dyn std::error::Error + Send + Sync>>(output_text(&final_response))
    }
    .await;

    ctx.cleanup().await;

    let text = result.expect("Responses tool-calling flow failed");
    println!("Responses tool final text: {text}");
    assert!(
        !text.trim().is_empty(),
        "final response text should not be empty"
    );
}

async fn post_response_json(ctx: &ResponsesServiceContext, body: Value) -> TestResult<Value> {
    let response = ctx
        .http
        .post(format!("{}/responses", ctx.base_url))
        .json(&body)
        .send()
        .await?;
    let status = response.status();
    let text = response.text().await?;
    if !status.is_success() {
        return Err(format!("Responses API returned {status}: {text}").into());
    }
    Ok(serde_json::from_str(&text)?)
}

fn output_text(response: &Value) -> String {
    response
        .get("output")
        .and_then(Value::as_array)
        .into_iter()
        .flatten()
        .find_map(|item| {
            if item.get("type").and_then(Value::as_str) != Some("message") {
                return None;
            }
            match item.get("content") {
                Some(Value::String(text)) => Some(text.clone()),
                Some(Value::Array(parts)) => Some(
                    parts
                        .iter()
                        .filter_map(|part| {
                            (part.get("type").and_then(Value::as_str) == Some("output_text"))
                                .then(|| part.get("text").and_then(Value::as_str))
                                .flatten()
                        })
                        .collect::<String>(),
                ),
                _ => None,
            }
        })
        .unwrap_or_default()
}

fn find_function_call(response: &Value) -> Option<(String, String)> {
    response.get("output")?.as_array()?.iter().find_map(|item| {
        if item.get("type").and_then(Value::as_str) != Some("function_call") {
            return None;
        }
        let call_id = item.get("call_id")?.as_str()?.to_string();
        let name = item.get("name")?.as_str()?.to_string();
        Some((call_id, name))
    })
}

fn get_weather_tool() -> Value {
    json!({
        "type": "function",
        "name": "get_weather",
        "description": "Get the current weather for a city.",
        "parameters": {
            "type": "object",
            "properties": {
                "city": { "type": "string", "description": "City name" }
            },
            "required": ["city"]
        }
    })
}

#[derive(Default)]
struct StreamSummary {
    delta_count: usize,
    completed: bool,
}

async fn read_responses_sse(mut response: reqwest::Response) -> TestResult<StreamSummary> {
    let status = response.status();
    if !status.is_success() {
        let text = response.text().await?;
        return Err(format!("Responses API returned {status}: {text}").into());
    }

    let mut buffer = String::new();
    let mut summary = StreamSummary::default();

    while let Some(chunk) = response.chunk().await? {
        buffer.push_str(&String::from_utf8_lossy(&chunk).replace("\r\n", "\n"));
        while let Some(block_end) = buffer.find("\n\n") {
            let block = buffer[..block_end].to_string();
            buffer = buffer[block_end + 2..].to_string();
            if handle_sse_block(&block, &mut summary) {
                return Ok(summary);
            }
        }
    }

    Ok(summary)
}

fn handle_sse_block(block: &str, summary: &mut StreamSummary) -> bool {
    let data = block
        .lines()
        .filter_map(|line| line.trim().strip_prefix("data: "))
        .collect::<Vec<_>>()
        .join("\n");

    if data.is_empty() {
        return false;
    }
    if data == "[DONE]" {
        return true;
    }

    if let Ok(event) = serde_json::from_str::<Value>(&data) {
        match event.get("type").and_then(Value::as_str) {
            Some("response.output_text.delta") => summary.delta_count += 1,
            Some("response.completed") => summary.completed = true,
            _ => {}
        }
    }

    false
}
