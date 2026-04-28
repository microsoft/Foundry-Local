//! Responses API example demonstrating non-streaming and streaming usage,
//! tool calling, and multi-turn conversations.

use std::io::{self, Write};

use foundry_local_sdk::{
    FoundryLocalConfig, FoundryLocalError, FoundryLocalManager, FunctionToolDefinition,
    ResponseInput, ResponseItem, StreamingEvent,
};
use serde_json::json;
use tokio_stream::StreamExt;

type Result<T> = std::result::Result<T, FoundryLocalError>;

#[tokio::main]
async fn main() -> Result<()> {
    // ── 1. Initialise the manager ────────────────────────────────────────────
    let config = FoundryLocalConfig::new("foundry_local_responses_example");
    let manager = FoundryLocalManager::create(config)?;

    // ── 2. Start the web service ─────────────────────────────────────────────
    println!("Starting web service…");
    manager.start_web_service().await?;
    println!("Web service URLs: {:?}", manager.urls()?);

    // ── 3. Pick a model ──────────────────────────────────────────────────────
    let models = manager.catalog().get_models().await?;
    let model_alias = ["phi-4-mini", "phi-3.5-mini", "qwen2.5-0.5b"]
        .iter()
        .find(|alias| models.iter().any(|m| m.alias() == **alias))
        .map(|s| s.to_string())
        .or_else(|| models.first().map(|m| m.alias().to_string()))
        .expect("No models available in the catalog");

    println!("Using model: {model_alias}");
    let model = manager.catalog().get_model(&model_alias).await?;

    if !model.is_cached().await? {
        println!("Downloading model {model_alias}…");
        model.download(None::<fn(f64)>).await?;
    }
    model.load().await?;
    println!("Model loaded.");

    // ── 4. Create the Responses client ───────────────────────────────────────
    let mut client = manager.get_responses_client(Some(&model.info().id))?;
    client.settings.store = Some(true);

    // ── 5. Non-streaming request ─────────────────────────────────────────────
    println!("\n─── Non-streaming ───────────────────────────────────────────────");
    let response = client
        .create(
            ResponseInput::Text("What is the capital of France? Reply in one word.".into()),
            None,
        )
        .await?;

    println!("Status : {}", response.status);
    println!("Answer : {}", response.output_text());
    if let Some(usage) = &response.usage {
        println!(
            "Tokens : {} in / {} out",
            usage.input_tokens, usage.output_tokens
        );
    }

    // ── 6. Streaming request ─────────────────────────────────────────────────
    println!("\n─── Streaming ───────────────────────────────────────────────────");
    print!("Story : ");
    io::stdout().flush().ok();

    let mut stream = client
        .create_streaming(
            ResponseInput::Text(
                "Tell me a two-sentence story about a robot that loves ice cream.".into(),
            ),
            None,
        )
        .await?;

    let mut full_text = String::new();
    while let Some(event) = stream.next().await {
        match event? {
            StreamingEvent::OutputTextDelta { delta, .. } => {
                print!("{delta}");
                io::stdout().flush().ok();
                full_text.push_str(&delta);
            }
            StreamingEvent::ResponseCompleted { response, .. } => {
                if let Some(usage) = response.usage.as_ref() {
                    println!("\n[completed, {} output tokens]", usage.output_tokens);
                } else {
                    println!("\n[completed]");
                }
            }
            _ => {}
        }
    }

    // ── 7. Multi-turn: follow-up using previous_response_id ─────────────────
    println!("\n─── Multi-turn ──────────────────────────────────────────────────");
    let first = client
        .create(
            ResponseInput::Text("My favourite number is 42. Remember this.".into()),
            None,
        )
        .await?;
    println!("Turn 1: {}", first.output_text());

    let follow_up_opts = foundry_local_sdk::ResponseCreateRequest {
        model: model.info().id.clone(),
        input: ResponseInput::Text("What is my favourite number?".into()),
        previous_response_id: Some(first.id.clone()),
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

    let second = client
        .create(
            ResponseInput::Text("What is my favourite number?".into()),
            Some(follow_up_opts),
        )
        .await?;
    println!("Turn 2: {}", second.output_text());

    // ── 8. Tool calling ──────────────────────────────────────────────────────
    println!("\n─── Tool calling ────────────────────────────────────────────────");
    let add_tool = FunctionToolDefinition {
        tool_type: "function".into(),
        name: "add".into(),
        description: Some("Add two integers and return the sum.".into()),
        parameters: Some(json!({
            "type": "object",
            "properties": {
                "a": { "type": "integer", "description": "First addend" },
                "b": { "type": "integer", "description": "Second addend" }
            },
            "required": ["a", "b"]
        })),
        strict: None,
    };

    let tool_opts = foundry_local_sdk::ResponseCreateRequest {
        model: model.info().id.clone(),
        input: ResponseInput::Text("What is 123 + 456? Use the add tool.".into()),
        tools: Some(vec![add_tool]),
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

    let tool_response = client
        .create(
            ResponseInput::Text("What is 123 + 456? Use the add tool.".into()),
            Some(tool_opts),
        )
        .await?;

    if let Some(ResponseItem::FunctionCall {
        call_id,
        name,
        arguments,
        ..
    }) = tool_response
        .output
        .iter()
        .find(|i| matches!(i, ResponseItem::FunctionCall { .. }))
    {
        println!("Model called tool: {name}({arguments})");
        let args: serde_json::Value = serde_json::from_str(arguments)?;
        let a = args["a"].as_i64().unwrap_or(0);
        let b = args["b"].as_i64().unwrap_or(0);
        let sum = a + b;

        let result_input = ResponseInput::Items(vec![ResponseItem::FunctionCallOutput {
            id: None,
            call_id: call_id.clone(),
            output: sum.to_string(),
            status: None,
        }]);

        let final_opts = foundry_local_sdk::ResponseCreateRequest {
            model: model.info().id.clone(),
            input: result_input.clone(),
            previous_response_id: Some(tool_response.id.clone()),
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

        let final_response = client.create(result_input, Some(final_opts)).await?;
        println!("Tool result: {}", final_response.output_text());
    } else {
        println!("No tool call in response (model may not support tool calling)");
    }

    // ── 9. Clean up ──────────────────────────────────────────────────────────
    model.unload().await?;
    manager.stop_web_service().await?;
    println!("\nDone.");
    Ok(())
}
