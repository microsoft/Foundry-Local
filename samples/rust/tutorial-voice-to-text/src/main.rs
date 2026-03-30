// <complete_code>
// <imports>
use foundry_local_sdk::{
    ChatCompletionRequestMessage,
    ChatCompletionRequestSystemMessage,
    ChatCompletionRequestUserMessage,
    FoundryLocalConfig, FoundryLocalManager,
};
use std::io::{self, Write};
// </imports>

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    // <init>
    // Initialize the Foundry Local SDK
    let manager = FoundryLocalManager::create(
        FoundryLocalConfig::new("note-taker"),
    )?;
    // </init>

    // <transcription>
    // Load the speech-to-text model
    let speech_model = manager
        .catalog()
        .get_model("whisper")
        .await?;

    if !speech_model.is_cached().await? {
        println!("Downloading speech model...");
        speech_model
            .download(Some(|progress: &str| {
                print!("\r  {progress}");
                io::stdout().flush().ok();
            }))
            .await?;
        println!();
    }

    speech_model.load().await?;
    println!("Speech model loaded.");

    // Transcribe the audio file
    let audio_client = speech_model.create_audio_client();
    let transcription = audio_client
        .transcribe("meeting-notes.wav")
        .await?;
    println!("\nTranscription:\n{}", transcription.text);

    // Unload the speech model to free memory
    speech_model.unload().await?;
    // </transcription>

    // <summarization>
    // Load the chat model for summarization
    let chat_model = manager
        .catalog()
        .get_model("phi-3.5-mini")
        .await?;

    if !chat_model.is_cached().await? {
        println!("Downloading chat model...");
        chat_model
            .download(Some(|progress: &str| {
                print!("\r  {progress}");
                io::stdout().flush().ok();
            }))
            .await?;
        println!();
    }

    chat_model.load().await?;
    println!("Chat model loaded.");

    // Summarize the transcription into organized notes
    let client = chat_model
        .create_chat_client()
        .temperature(0.7)
        .max_tokens(512);

    let messages: Vec<ChatCompletionRequestMessage> = vec![
        ChatCompletionRequestSystemMessage::from(
            "You are a note-taking assistant. Summarize \
             the following transcription into organized, \
             concise notes with bullet points.",
        )
        .into(),
        ChatCompletionRequestUserMessage::from(
            transcription.text.as_str(),
        )
        .into(),
    ];

    let response = client
        .complete_chat(&messages, None)
        .await?;
    let summary = response.choices[0]
        .message
        .content
        .as_deref()
        .unwrap_or("");
    println!("\nSummary:\n{}", summary);

    // Clean up
    chat_model.unload().await?;
    println!("\nDone. Models unloaded.");
    // </summarization>

    Ok(())
}
// </complete_code>
