// <complete_code>
// <imports>
use foundry_local_sdk::{
    ChatCompletionRequestAssistantMessage, ChatCompletionRequestMessage,
    ChatCompletionRequestSystemMessage, ChatCompletionRequestUserMessage,
    FoundryLocalConfig, FoundryLocalManager,
};
use std::io::{BufRead, Write};
use tokio_stream::StreamExt;
// </imports>

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    // <init>
    // Initialize the Foundry Local SDK
    let manager = FoundryLocalManager::create(FoundryLocalConfig::new("chat-assistant"))?;

    // Select and load a model from the catalog
    let model = manager.catalog().get_model("phi-3.5-mini").await?;

    model
        .download(Some(|progress: f32| {
            print!("\rDownloading model: {:.2}%", progress);
            std::io::stdout().flush().unwrap();
        }))
        .await?;
    println!();

    model.load().await?;
    println!("Model loaded and ready.");

    // Create a chat client
    let client = model.create_chat_client().temperature(0.7).max_tokens(512);
    // </init>

    // <system_prompt>
    // Start the conversation with a system prompt
    let mut messages: Vec<ChatCompletionRequestMessage> = vec![
        ChatCompletionRequestSystemMessage::new(
            "You are a helpful, friendly assistant. Keep your responses \
             concise and conversational. If you don't know something, say so.",
        )
        .into(),
    ];
    // </system_prompt>

    println!("\nChat assistant ready! Type 'quit' to exit.\n");

    let stdin = std::io::stdin();
    // <conversation_loop>
    loop {
        print!("You: ");
        std::io::stdout().flush()?;

        let mut input = String::new();
        stdin.lock().read_line(&mut input)?;
        let input = input.trim();

        if input.eq_ignore_ascii_case("quit") || input.eq_ignore_ascii_case("exit") {
            break;
        }

        // Add the user's message to conversation history
        messages.push(ChatCompletionRequestUserMessage::new(input).into());

        // <streaming>
        // Stream the response token by token
        print!("Assistant: ");
        std::io::stdout().flush()?;
        let mut full_response = String::new();
        let mut stream = client.complete_streaming_chat(&messages, None).await?;
        while let Some(chunk) = stream.next().await {
            let chunk = chunk?;
            if let Some(content) = &chunk.choices[0].message.content {
                print!("{}", content);
                std::io::stdout().flush()?;
                full_response.push_str(content);
            }
        }
        println!("\n");
        // </streaming>

        // Add the complete response to conversation history
        messages.push(ChatCompletionRequestAssistantMessage::new(full_response).into());
    }
    // </conversation_loop>

    // Clean up - unload the model
    model.unload().await?;
    println!("Model unloaded. Goodbye!");

    Ok(())
}
// </complete_code>
