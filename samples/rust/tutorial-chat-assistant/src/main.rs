// <complete_code>
// <imports>
use foundry_local_sdk::{
    ChatCompletionRequestMessage,
    ChatCompletionRequestSystemMessage, ChatCompletionRequestUserMessage,
    FoundryLocalConfig, FoundryLocalManager,
};
use std::io::{self, BufRead, Write};
use tokio_stream::StreamExt;
// </imports>

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    // <init>
    // Initialize the Foundry Local SDK
    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("chat-assistant"))?;

    // Select and load a model from the catalog
    let model = manager.catalog().get_model("phi-3.5-mini").await?;

    if !model.is_cached().await? {
        println!("Downloading model...");
        model
            .download(Some(|progress: &str| {
                print!("\r  {progress}");
                io::stdout().flush().ok();
            }))
            .await?;
        println!();
    }

    model.load().await?;
    println!("Model loaded and ready.");

    // Create a chat client
    let client = model.create_chat_client().temperature(0.7).max_tokens(512);
    // </init>

    // <system_prompt>
    // Start the conversation with a system prompt
    let mut messages: Vec<ChatCompletionRequestMessage> = vec![
        ChatCompletionRequestSystemMessage::from(
            "You are a helpful, friendly assistant. Keep your responses \
             concise and conversational. If you don't know something, say so.",
        )
        .into(),
    ];
    // </system_prompt>

    println!("\nChat assistant ready! Type 'quit' to exit.\n");

    let stdin = io::stdin();
    // <conversation_loop>
    loop {
        print!("You: ");
        io::stdout().flush()?;

        let mut input = String::new();
        stdin.lock().read_line(&mut input)?;
        let input = input.trim();

        if input.eq_ignore_ascii_case("quit") || input.eq_ignore_ascii_case("exit") {
            break;
        }

        // Add the user's message to conversation history
        messages.push(ChatCompletionRequestUserMessage::from(input).into());

        // <streaming>
        // Stream the response token by token
        print!("Assistant: ");
        io::stdout().flush()?;
        let mut full_response = String::new();
        let mut stream = client.complete_streaming_chat(&messages, None).await?;
        while let Some(chunk) = stream.next().await {
            let chunk = chunk?;
            if let Some(choice) = chunk.choices.first() {
                if let Some(ref content) = choice.delta.content {
                    print!("{content}");
                    io::stdout().flush()?;
                    full_response.push_str(content);
                }
            }
        }
        println!("\n");
        // </streaming>

        // Add the complete response to conversation history
        let assistant_msg: ChatCompletionRequestMessage = serde_json::from_value(
            serde_json::json!({"role": "assistant", "content": full_response}),
        )?;
        messages.push(assistant_msg);
    }
    // </conversation_loop>

    // Clean up - unload the model
    model.unload().await?;
    println!("Model unloaded. Goodbye!");

    Ok(())
}
// </complete_code>
