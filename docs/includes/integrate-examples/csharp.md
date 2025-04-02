## Basic Integration

```csharp
// Install with: dotnet add package Azure.AI.OpenAI
using Azure.AI.OpenAI;
using Azure;

// Create a client
OpenAIClient client = new OpenAIClient(
    new Uri("http://localhost:5272/v1"),
    new AzureKeyCredential("not-needed-for-local")
);

// Chat completions
ChatCompletionsOptions options = new ChatCompletionsOptions()
{
    Messages =
    {
        new ChatMessage(ChatRole.User, "What is AI Foundry Local?")
    },
    DeploymentName = "Phi-4-mini-gpu-int4-rtn-block-32" // Use model name here
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
    OpenAIClient client = new OpenAIClient(
        new Uri("http://localhost:5272/v1"),
        new AzureKeyCredential("not-needed-for-local")
    );

    ChatCompletionsOptions options = new ChatCompletionsOptions()
    {
        Messages =
        {
            new ChatMessage(ChatRole.User, "Write a short story about AI")
        },
        DeploymentName = "Phi-4-mini-gpu-int4-rtn-block-32"
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
