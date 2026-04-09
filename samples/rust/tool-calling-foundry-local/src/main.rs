// <complete_code>
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// <imports>
use std::io::{self, Write};

use serde_json::{json, Value};
use tokio_stream::StreamExt;

use foundry_local_sdk::{
    ChatCompletionRequestMessage, ChatCompletionRequestSystemMessage,
    ChatCompletionRequestToolMessage, ChatCompletionRequestUserMessage, ChatCompletionTools,
    ChatToolChoice, FinishReason, FoundryLocalConfig, FoundryLocalManager,
};
// </imports>

// By using an alias, the most suitable model variant will be downloaded
// to your end-user's device.
const ALIAS: &str = "qwen2.5-0.5b";

// <tool_implementations>
/// A simple tool that multiplies two numbers.
fn multiply_numbers(first: f64, second: f64) -> f64 {
    first * second
}

/// Dispatch a tool call by name and parsed arguments.
fn invoke_tool(name: &str, args: &Value) -> String {
    match name {
        "multiply_numbers" => {
            let first = args.get("first").and_then(|v| v.as_f64()).unwrap_or(0.0);
            let second = args.get("second").and_then(|v| v.as_f64()).unwrap_or(0.0);
            let result = multiply_numbers(first, second);
            result.to_string()
        }
        _ => format!("Unknown tool: {name}"),
    }
}
// </tool_implementations>

/// Accumulated state from a streaming response that contains tool calls.
#[derive(Default)]
struct ToolCallState {
    tool_calls: Vec<Value>,
    current_tool_id: String,
    current_tool_name: String,
    current_tool_args: String,
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("Tool Calling with Foundry Local");
    println!("===============================\n");

    // ── 1. Initialise the manager ────────────────────────────────────────
    // <init>
    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("foundry_local_samples"))?;
    // </init>

    // Download and register all execution providers.
    manager.download_and_register_eps(None).await?;

    // ── 2. Load a model──────────────────────────────────────────────────
    // <model_setup>
    let model = manager.catalog().get_model(ALIAS).await?;
    println!("Model: {} (id: {})", model.alias(), model.id());

    if !model.is_cached().await? {
        println!("Downloading model...");
        model
            .download(Some(|progress: f64| {
                print!("\r  {progress:.1}%");
                io::stdout().flush().ok();
            }))
            .await?;
        println!();
    }

    println!("Loading model...");
    model.load().await?;
    println!("✓ Model loaded\n");
    // </model_setup>

    // ── 3. Create a chat clientwith tool_choice = required ──────────────
    let client = model.create_chat_client()
        .max_tokens(512)
        .tool_choice(ChatToolChoice::Required);

    // <tool_definitions>
    // Define the multiply_numbers tool.
    let tools: Vec<ChatCompletionTools> = serde_json::from_value(json!([{
        "type": "function",
        "function": {
            "name": "multiply_numbers",
            "description": "A tool for multiplying two numbers.",
            "parameters": {
                "type": "object",
                "properties": {
                    "first": {
                        "type": "integer",
                        "description": "The first number in the operation"
                    },
                    "second": {
                        "type": "integer",
                        "description": "The second number in the operation"
                    }
                },
                "required": ["first", "second"]
            }
        }
    }]))?;
    // </tool_definitions>

    // <tool_loop>
    // Prepare the initial conversation.
    let mut messages: Vec<ChatCompletionRequestMessage> = vec![
        ChatCompletionRequestSystemMessage::from(
            "You are a helpful AI assistant. If necessary, you can use any provided tools to answer the question.",
        )
        .into(),
        ChatCompletionRequestUserMessage::from("What is the answer to 7 multiplied by 6?").into(),
    ];

    // ── 4. First streaming call – expect tool_calls ──────────────────────
    println!("Chat completion response:");

    let mut state = ToolCallState::default();
    let mut stream = client
        .complete_streaming_chat(&messages, Some(&tools))
        .await?;

    while let Some(chunk) = stream.next().await {
        let chunk = chunk?;
        if let Some(choice) = chunk.choices.first() {
            // Accumulate streamed content (if any).
            if let Some(ref content) = choice.delta.content {
                print!("{content}");
                io::stdout().flush().ok();
            }

            // Accumulate tool call fragments.
            if let Some(ref tool_calls) = choice.delta.tool_calls {
                for tc in tool_calls {
                    if let Some(ref id) = tc.id {
                        state.current_tool_id = id.clone();
                    }
                    if let Some(ref func) = tc.function {
                        if let Some(ref name) = func.name {
                            state.current_tool_name = name.clone();
                        }
                        if let Some(ref args) = func.arguments {
                            state.current_tool_args.push_str(args);
                        }
                    }
                }
            }

            // When the model signals finish_reason = ToolCalls, finalise.
            if choice.finish_reason == Some(FinishReason::ToolCalls) {
                let tc = json!({
                    "id": state.current_tool_id.clone(),
                    "type": "function",
                    "function": {
                        "name": state.current_tool_name.clone(),
                        "arguments": state.current_tool_args.clone(),
                    }
                });
                state.tool_calls.push(tc);
            }
        }
    }
    println!();

    // ── 5. Execute the tool(s)and append results ────────────────────────
    for tc in &state.tool_calls {
        let func = &tc["function"];
        let name = func["name"].as_str().unwrap_or_default();
        let args_str = func["arguments"].as_str().unwrap_or("{}");
        let args: Value = serde_json::from_str(args_str).unwrap_or(json!({}));

        println!("\nInvoking tool: {name} with arguments {args}");
        let result = invoke_tool(name, &args);
        println!("Tool response: {result}");

        // Append the assistant's tool_calls message and the tool result.
        let assistant_msg: ChatCompletionRequestMessage = serde_json::from_value(json!({
            "role": "assistant",
            "content": null,
            "tool_calls": [tc],
        }))?;
        messages.push(assistant_msg);
        messages.push(
            ChatCompletionRequestToolMessage {
                content: result.into(),
                tool_call_id: tc["id"].as_str().unwrap_or_default().to_string(),
            }
            .into(),
        );
    }

    // ── 6. Continue the conversation with auto tool_choice ───────────────
    println!("\nTool calls completed. Prompting model to continue conversation...\n");

    messages.push(
        ChatCompletionRequestSystemMessage::from(
            "Respond only with the answer generated by the tool.",
        )
        .into(),
    );

    let client = client.tool_choice(ChatToolChoice::Auto);

    print!("Chat completion response: ");
    let mut stream = client
        .complete_streaming_chat(&messages, Some(&tools))
        .await?;
    while let Some(chunk) = stream.next().await {
        let chunk = chunk?;
        if let Some(choice) = chunk.choices.first() {
            if let Some(ref content) = choice.delta.content {
                print!("{content}");
                io::stdout().flush().ok();
            }
        }
    }
    println!("\n");
    // </tool_loop>

    // ── 7. Clean up──────────────────────────────────────────────────────
    // <cleanup>
    println!("Unloading model...");
    model.unload().await?;
    println!("Done.");
    // </cleanup>

    Ok(())
}
// </complete_code>
