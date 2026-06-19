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
const MODEL_ALIAS: &str = "qwen2.5-0.5b";

#[tokio::main]
async fn main() -> SampleResult<()> {
    println!("Responses Web Service");
    println!("=====================\n");

    let config = FoundryLocalConfig::new("foundry_local_responses_web_service_sample");
    let manager = FoundryLocalManager::create(config)?;

    manager
        .download_and_register_eps_with_progress(None, {
            let mut current_ep = String::new();
            move |ep_name: &str, percent: f64| {
                if ep_name != current_ep {
                    if !current_ep.is_empty() {
                        println!();
                    }
                    current_ep = ep_name.to_string();
                }
                print!("\r  {:<30}  {:5.1}%", ep_name, percent);
                io::stdout().flush().ok();
            }
        })
        .await?;
    println!();

    let model = manager.catalog().get_model(MODEL_ALIAS).await?;
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
            "temperature": 0.0,
            "max_output_tokens": 64,
            "store": false
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
            "max_output_tokens": 64,
            "store": false,
            "stream": true
        }))
        .header(reqwest::header::ACCEPT, "text/event-stream")
        .send()
        .await?;
    let streamed = read_responses_sse(streaming_response).await?;
    println!("\nSaw {} text delta event(s).", streamed.delta_count);
    if !streamed.created || streamed.delta_count == 0 || !streamed.completed {
        return Err(
            "stream did not include response.created, text delta, and completion events".into(),
        );
    }

    println!("\n--- Function calling response ---");
    let weather_tool = get_weather_tool();
    let tool_response = post_response_json(
        &http,
        base_url,
        json!({
            "model": model_id,
            "input": "Use the get_weather tool and then answer with the weather.",
            "tools": [weather_tool.clone()],
            "tool_choice": "required",
            "temperature": 0.0,
            "max_output_tokens": 64,
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
                "output": "{\"location\":\"Seattle\",\"weather\":\"72 degrees F and sunny\"}"
            }],
            "tools": [weather_tool],
            "temperature": 0.0,
            "max_output_tokens": 64,
            "store": false
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
    if let Some(text) = response.get("output_text").and_then(Value::as_str) {
        return text.to_string();
    }

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
        "description": "Get the current weather. This sample always returns Seattle weather.",
        "parameters": {
            "type": "object",
            "properties": {},
            "additionalProperties": false
        }
    })
}

#[derive(Default)]
struct StreamSummary {
    created: bool,
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

    if !buffer.trim().is_empty() {
        handle_sse_block(&buffer, &mut summary);
    }

    Ok(summary)
}

fn handle_sse_block(block: &str, summary: &mut StreamSummary) -> bool {
    let data = block
        .lines()
        .filter_map(|line| line.trim().strip_prefix("data:").map(str::trim_start))
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
            Some("response.created") => summary.created = true,
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
