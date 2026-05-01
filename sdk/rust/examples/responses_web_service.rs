//! Responses API web-service sample.
//!
//! This sample uses the Rust SDK only for Foundry Local setup and lifecycle:
//! manager initialization, model lookup/download/load, and local web-service
//! start/stop. The actual `/v1/responses` calls use raw HTTP against the
//! OpenAI-compatible local endpoint.

use std::error::Error;
use std::io::{self, Write};

use foundry_local_sdk::{FoundryLocalConfig, FoundryLocalManager};
use serde_json::{json, Value};

type SampleResult<T> = Result<T, Box<dyn Error + Send + Sync>>;

#[tokio::main]
async fn main() -> SampleResult<()> {
    let config = FoundryLocalConfig::new("foundry_local_responses_web_service_sample");
    let manager = FoundryLocalManager::create(config)?;

    let models = manager.catalog().get_models().await?;
    let model_alias = ["qwen2.5-0.5b", "phi-4-mini", "phi-3.5-mini"]
        .iter()
        .find(|alias| models.iter().any(|m| m.alias() == **alias))
        .map(|s| s.to_string())
        .or_else(|| models.first().map(|m| m.alias().to_string()))
        .expect("No models available in the catalog");

    let model = manager.catalog().get_model(&model_alias).await?;
    if !model.is_cached().await? {
        println!("Downloading model '{}'...", model.alias());
        model
            .download(Some(|progress: f64| println!("  {progress:.1}%")))
            .await?;
    }

    println!("Loading model '{}'...", model.alias());
    model.load().await?;

    println!("Starting local OpenAI-compatible web service...");
    manager.start_web_service().await?;
    let base_url = format!(
        "{}/v1",
        manager
            .urls()?
            .first()
            .expect("web service did not return a URL")
            .trim_end_matches('/')
    );
    println!("Using base URL: {base_url}");

    let result = run_responses_flow(&base_url, model.id()).await;

    manager.stop_web_service().await.ok();
    model.unload().await.ok();

    result
}

async fn run_responses_flow(base_url: &str, model_id: &str) -> SampleResult<()> {
    let http = reqwest::Client::new();

    println!("\n--- Non-streaming response ---");
    let response = post_response_json(
        &http,
        base_url,
        json!({
            "model": model_id,
            "input": "What is 2 + 2? Respond with just the answer.",
            "temperature": 0.0
        }),
    )
    .await?;
    println!("Assistant: {}", output_text(&response));

    println!("\n--- Streaming response ---");
    print!("Assistant: ");
    io::stdout().flush().ok();
    let streaming_response = http
        .post(format!("{base_url}/responses"))
        .json(&json!({
            "model": model_id,
            "input": "Count from 1 to 3.",
            "temperature": 0.0,
            "stream": true
        }))
        .send()
        .await?;
    let streamed = read_responses_sse(streaming_response).await?;
    println!("\nSaw {} text delta event(s).", streamed.delta_count);
    if streamed.delta_count == 0 || !streamed.completed {
        return Err("stream did not include both text delta and completion events".into());
    }

    println!("\n--- Function calling response ---");
    let weather_tool = get_weather_tool();
    let tool_response = post_response_json(
        &http,
        base_url,
        json!({
            "model": model_id,
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
    println!("Model requested tool call: {name} ({call_id})");

    let final_response = post_response_json(
        &http,
        base_url,
        json!({
            "model": model_id,
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
    println!("Assistant: {}", output_text(&final_response));

    Ok(())
}

async fn post_response_json(
    http: &reqwest::Client,
    base_url: &str,
    body: Value,
) -> SampleResult<Value> {
    let response = http
        .post(format!("{base_url}/responses"))
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

async fn read_responses_sse(mut response: reqwest::Response) -> SampleResult<StreamSummary> {
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
            Some("response.output_text.delta") => {
                summary.delta_count += 1;
                if let Some(delta) = event.get("delta").and_then(Value::as_str) {
                    print!("{delta}");
                    io::stdout().flush().ok();
                }
            }
            Some("response.completed") => summary.completed = true,
            _ => {}
        }
    }

    false
}
