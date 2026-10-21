use std::io::{self, Write};

use foundry_local_sdk::{
    ChatSession, FoundryLocalConfig, FoundryLocalManager, Item, Request, RequestOptions,
    SearchOptions, TextKind, ToolChoice, ToolDefinition,
};
use serde_json::{json, Value};
use tokio_stream::StreamExt;

fn multiply(arguments: &str) -> String {
    let arguments: Value = serde_json::from_str(arguments).unwrap_or_else(|_| json!({}));
    let first = arguments
        .get("first")
        .and_then(Value::as_f64)
        .unwrap_or_default();
    let second = arguments
        .get("second")
        .and_then(Value::as_f64)
        .unwrap_or_default();
    (first * second).to_string()
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("foundry_local_samples"))?;
    let model = manager.catalog().get_model("qwen2.5-0.5b").await?;
    if model.supports_tool_calling() != Some(true) {
        return Err("The selected qwen2.5-0.5b variant does not support tool calling".into());
    }
    println!(
        "Using tool-capable model: {} ({})",
        model.alias(),
        model.id()
    );

    if !model.is_cached().await? {
        model.download(None::<fn(f64)>).await?;
    }
    model.load().await?;

    let session = ChatSession::new(&model).await?;
    session
        .add_tool_definition(
            ToolDefinition::new(
                "multiply",
                json!({
                    "type": "object",
                    "properties": {
                        "first": { "type": "number" },
                        "second": { "type": "number" }
                    },
                    "required": ["first", "second"]
                })
                .to_string(),
            )
            .with_description("Multiply two numbers."),
        )
        .await?;

    let request = Request::from_items(vec![Item::user_message(vec![Item::text(
        "Use the multiply tool to calculate 7 times 6.",
    )])])
    .with_options(RequestOptions {
        search: SearchOptions {
            max_output_tokens: Some(256),
            ..Default::default()
        },
        tool_choice: Some(ToolChoice::Required),
        ..Default::default()
    });

    let mut tool_results = Vec::new();
    let mut stream = session.process_streaming_request(request);
    while let Some(item) = stream.next().await {
        let item = item?;
        if let Some(call) = item.as_tool_call() {
            println!("Tool call: {}({})", call.name, call.arguments);
            let output = match call.name.as_str() {
                "multiply" => multiply(&call.arguments),
                name => format!("Unknown tool: {name}"),
            };
            println!("Tool result: {output}");
            tool_results.push(Item::tool_result(call.call_id.clone(), output));
        }
    }

    let follow_up = Request::from_items(tool_results).with_options(RequestOptions {
        tool_choice: Some(ToolChoice::None),
        ..Default::default()
    });
    print!("Assistant: ");
    let mut stream = session.process_streaming_request(follow_up);
    while let Some(item) = stream.next().await {
        if let Item::Text {
            text,
            kind: TextKind::Default,
        } = item?
        {
            print!("{text}");
            io::stdout().flush().ok();
        }
    }
    println!();

    drop(session);
    model.unload().await?;
    manager.shutdown()?;
    Ok(())
}
