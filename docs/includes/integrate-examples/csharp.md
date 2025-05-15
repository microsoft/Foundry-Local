## Basic Integration

```csharp
// Install with: dotnet add package Azure.AI.OpenAI
using Azure.AI.OpenAI;
using Azure;

// Create a client. Note the port is dynamically assigned, so check the logs for the correct port.
OpenAIClient client = new OpenAIClient(
    new Uri("http://localhost:5273/v1"),
    new AzureKeyCredential("not-needed-for-local")
);

// Chat completions
ChatCompletionsOptions options = new ChatCompletionsOptions()
{
    Messages =
    {
        new ChatMessage(ChatRole.User, "What is Foundry Local?")
    },
    DeploymentName = "Phi-4-mini-instruct-cuda-gpu" // Use model name here
};

Response<ChatCompletions> response = await client.GetChatCompletionsAsync(options);
string completion = response.Value.Choices[0].Message.Content;
Console.WriteLine(completion);
```

## Streaming Response

```csharp
// Install with: dotnet add package Azure.AI.OpenAI
using Azure.AI.OpenAI;
using Azure;
using System;
using System.Threading.Tasks;

async Task StreamCompletionsAsync()
{
// Note the port is dynamically assigned, so check the logs for the correct port.
    OpenAIClient client = new OpenAIClient(
        new Uri("http://localhost:5273/v1"),
        new AzureKeyCredential("not-needed-for-local")
    );

    ChatCompletionsOptions options = new ChatCompletionsOptions()
    {
        Messages =
        {
            new ChatMessage(ChatRole.User, "Write a short story about AI")
        },
        DeploymentName = "Phi-4-mini-instruct-cuda-gpu"
    };

    await foreach (StreamingChatCompletionsUpdate update in client.GetChatCompletionsStreaming(options))
    {
        if (update.ContentUpdate != null)
        {
            Console.Write(update.ContentUpdate);
        }
    }
}

// Call the async method
await StreamCompletionsAsync();
```
