// <complete_code>
// <imports>
use foundry_local_sdk::{
    ChatCompletionRequestMessage,
    ChatCompletionRequestSystemMessage,
    ChatCompletionRequestToolMessage,
    ChatCompletionRequestUserMessage,
    ChatCompletionMessageToolCalls,
    ChatCompletionTools, ChatToolChoice,
    FoundryLocalConfig, FoundryLocalManager,
};
use serde_json::{json, Value};
use std::io::{self, BufRead, Write};
// </imports>

// <tool_implementations>
// --- Tool implementations ---
fn execute_tool(
    name: &str,
    arguments: &Value,
) -> Value {
    match name {
        "get_weather" => {
            let location = arguments["location"]
                .as_str()
                .unwrap_or("unknown");
            let unit = arguments["unit"]
                .as_str()
                .unwrap_or("celsius");
            let temp = if unit == "celsius" { 22 } else { 72 };
            json!({
                "location": location,
                "temperature": temp,
                "unit": unit,
                "condition": "Sunny"
            })
        }
        "calculate" => {
            let expression = arguments["expression"]
                .as_str()
                .unwrap_or("");
            let is_valid = expression
                .chars()
                .all(|c| "0123456789+-*/(). ".contains(c));
            if !is_valid {
                return json!({"error": "Invalid expression"});
            }
            match eval_expression(expression) {
                Ok(result) => json!({
                    "expression": expression,
                    "result": result
                }),
                Err(e) => json!({"error": e}),
            }
        }
        _ => json!({"error": format!("Unknown function: {}", name)}),
    }
}

fn eval_expression(expr: &str) -> Result<f64, String> {
    let expr = expr.replace(' ', "");
    let chars: Vec<char> = expr.chars().collect();
    let mut pos = 0;
    let result = parse_add(&chars, &mut pos)?;
    if pos < chars.len() {
        return Err("Unexpected character".to_string());
    }
    Ok(result)
}

fn parse_add(
    chars: &[char],
    pos: &mut usize,
) -> Result<f64, String> {
    let mut result = parse_mul(chars, pos)?;
    while *pos < chars.len()
        && (chars[*pos] == '+' || chars[*pos] == '-')
    {
        let op = chars[*pos];
        *pos += 1;
        let right = parse_mul(chars, pos)?;
        result = if op == '+' {
            result + right
        } else {
            result - right
        };
    }
    Ok(result)
}

fn parse_mul(
    chars: &[char],
    pos: &mut usize,
) -> Result<f64, String> {
    let mut result = parse_atom(chars, pos)?;
    while *pos < chars.len()
        && (chars[*pos] == '*' || chars[*pos] == '/')
    {
        let op = chars[*pos];
        *pos += 1;
        let right = parse_atom(chars, pos)?;
        result = if op == '*' {
            result * right
        } else {
            result / right
        };
    }
    Ok(result)
}

fn parse_atom(
    chars: &[char],
    pos: &mut usize,
) -> Result<f64, String> {
    if *pos < chars.len() && chars[*pos] == '(' {
        *pos += 1;
        let result = parse_add(chars, pos)?;
        if *pos < chars.len() && chars[*pos] == ')' {
            *pos += 1;
        }
        return Ok(result);
    }
    let start = *pos;
    while *pos < chars.len()
        && (chars[*pos].is_ascii_digit() || chars[*pos] == '.')
    {
        *pos += 1;
    }
    if start == *pos {
        return Err("Expected number".to_string());
    }
    let num_str: String = chars[start..*pos].iter().collect();
    num_str.parse::<f64>().map_err(|e| e.to_string())
}
// </tool_implementations>

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    // <tool_definitions>
    // --- Tool definitions ---
    let tools: Vec<ChatCompletionTools> = serde_json::from_value(json!([
        {
            "type": "function",
            "function": {
                "name": "get_weather",
                "description":
                    "Get the current weather for a location",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "location": {
                            "type": "string",
                            "description":
                                "The city or location"
                        },
                        "unit": {
                            "type": "string",
                            "enum": ["celsius", "fahrenheit"],
                            "description": "Temperature unit"
                        }
                    },
                    "required": ["location"]
                }
            }
        },
        {
            "type": "function",
            "function": {
                "name": "calculate",
                "description": "Perform a math calculation",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "expression": {
                            "type": "string",
                            "description":
                                "The math expression to evaluate"
                        }
                    },
                    "required": ["expression"]
                }
            }
        }
    ]))?;
    // </tool_definitions>

    // <init>
    // Initialize the Foundry Local SDK
    let manager = FoundryLocalManager::create(
        FoundryLocalConfig::new("tool-calling-app"),
    )?;

    // Select and load a model
    let model = manager
        .catalog()
        .get_model("qwen2.5-0.5b")
        .await?;

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

    model.load().await?;
    println!("Model loaded and ready.");

    // Create a chat client
    let client = model
        .create_chat_client()
        .temperature(0.7)
        .max_tokens(512)
        .tool_choice(ChatToolChoice::Auto);

    // Conversation with a system prompt
    let mut messages: Vec<ChatCompletionRequestMessage> = vec![
        ChatCompletionRequestSystemMessage::from(
            "You are a helpful assistant with access to tools. \
             Use them when needed to answer questions accurately.",
        )
        .into(),
    ];
    // </init>

    // <tool_loop>
    println!(
        "\nTool-calling assistant ready! Type 'quit' to exit.\n"
    );

    let stdin = io::stdin();
    loop {
        print!("You: ");
        io::stdout().flush()?;

        let mut input = String::new();
        stdin.lock().read_line(&mut input)?;
        let input = input.trim();

        if input.eq_ignore_ascii_case("quit")
            || input.eq_ignore_ascii_case("exit")
        {
            break;
        }

        messages.push(
            ChatCompletionRequestUserMessage::from(input).into(),
        );

        let mut response = client
            .complete_chat(&messages, Some(&tools))
            .await?;

        // Process tool calls in a loop
        while response.choices[0].message.tool_calls.is_some() {
            let tool_calls = response.choices[0]
                .message
                .tool_calls
                .as_ref()
                .unwrap();

            // Append the assistant's tool_calls message via JSON
            let assistant_msg: ChatCompletionRequestMessage =
                serde_json::from_value(json!({
                    "role": "assistant",
                    "content": null,
                    "tool_calls": tool_calls,
                }))?;
            messages.push(assistant_msg);

            for tc_enum in tool_calls {
                let tool_call = match tc_enum {
                    ChatCompletionMessageToolCalls::Function(
                        tc,
                    ) => tc,
                    _ => continue,
                };
                let function_name =
                    &tool_call.function.name;
                let arguments: Value =
                    serde_json::from_str(
                        &tool_call.function.arguments,
                    )?;
                println!(
                    "  Tool call: {}({})",
                    function_name, arguments
                );

                let result =
                    execute_tool(function_name, &arguments);
                messages.push(
                    ChatCompletionRequestToolMessage {
                        content: result.to_string().into(),
                        tool_call_id: tool_call.id.clone(),
                    }
                    .into(),
                );
            }

            response = client
                .complete_chat(&messages, Some(&tools))
                .await?;
        }

        let answer = response.choices[0]
            .message
            .content
            .as_deref()
            .unwrap_or("");
        let assistant_msg: ChatCompletionRequestMessage =
            serde_json::from_value(json!({
                "role": "assistant",
                "content": answer,
            }))?;
        messages.push(assistant_msg);
        println!("Assistant: {}\n", answer);
    }

    // Clean up
    model.unload().await?;
    println!("Model unloaded. Goodbye!");
    // </tool_loop>

    Ok(())
}
// </complete_code>
