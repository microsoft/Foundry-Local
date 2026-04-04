// <complete_code>
// <imports>
using Microsoft.AI.Foundry.Local;
using Microsoft.AI.Foundry.Local.OpenAI;
using Microsoft.Extensions.Logging;
// </imports>

// <init>
CancellationToken ct = CancellationToken.None;

var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
};

using var loggerFactory = LoggerFactory.Create(builder =>
{
    builder.SetMinimumLevel(Microsoft.Extensions.Logging.LogLevel.Information);
});
var logger = loggerFactory.CreateLogger<Program>();

// Initialize the singleton instance
await FoundryLocalManager.CreateAsync(config, logger);
var mgr = FoundryLocalManager.Instance;

// Select and load a model from the catalog
var catalog = await mgr.GetCatalogAsync();
var model = await catalog.GetModelAsync("qwen2.5-0.5b")
    ?? throw new Exception("Model not found");

await model.DownloadAsync(progress =>
{
    Console.Write($"\rDownloading model: {progress:F2}%");
    if (progress >= 100f) Console.WriteLine();
});

await model.LoadAsync();
Console.WriteLine("Model loaded and ready.");

// Get a chat client
var chatClient = await model.GetChatClientAsync();
// </init>

// <system_prompt>
// Start the conversation with a system prompt
var messages = new List<ChatMessage>
{
    new ChatMessage
    {
        Role = ChatMessageRole.System,
        Content = "You are a helpful, friendly assistant. Keep your responses " +
                  "concise and conversational. If you don't know something, say so."
    }
};
// </system_prompt>

Console.WriteLine("\nChat assistant ready! Type 'quit' to exit.\n");

// <conversation_loop>
while (true)
{
    Console.Write("You: ");
    var userInput = Console.ReadLine();
    if (string.IsNullOrWhiteSpace(userInput) ||
        userInput.Equals("quit", StringComparison.OrdinalIgnoreCase) ||
        userInput.Equals("exit", StringComparison.OrdinalIgnoreCase))
    {
        break;
    }

    // Add the user's message to conversation history
    messages.Add(new ChatMessage { Role = ChatMessageRole.User, Content = userInput });

    // <streaming>
    // Stream the response token by token
    Console.Write("Assistant: ");
    var fullResponse = string.Empty;
    var streamingResponse = chatClient.CompleteChatStreamingAsync(messages, ct);
    await foreach (var chunk in streamingResponse)
    {
        var content = chunk.Choices[0].Message.Content;
        if (!string.IsNullOrEmpty(content))
        {
            Console.Write(content);
            Console.Out.Flush();
            fullResponse += content;
        }
    }
    Console.WriteLine("\n");
    // </streaming>

    // Add the complete response to conversation history
    messages.Add(new ChatMessage { Role = ChatMessageRole.Assistant, Content = fullResponse });
}
// </conversation_loop>

// Clean up - unload the model
await model.UnloadAsync();
Console.WriteLine("Model unloaded. Goodbye!");
// </complete_code>
