# Integrate Foundry Local with Inferencing SDKs

AI Foundry Local provides a REST API endpoint that makes it easy to integrate with various inferencing SDKs and programming languages. This guide shows you how to connect your applications to locally running AI models using popular SDKs.

## Prerequisites

- AI Foundry Local installed and running on your system
- A model loaded into the service (use `foundry model load <model-name>`)
- Basic knowledge of the programming language you want to use for integration
- Development environment for your chosen language

## Understanding the REST API

When AI Foundry Local is running, it exposes an OpenAI-compatible REST API endpoint at `http://localhost:5272/v1`. This endpoint supports standard API operations like:

- `/completions` - For text completion
- `/chat/completions` - For chat-based interactions
- `/models` - To list available models

## Language Examples

### Python

```python
from openai import OpenAI

# Configure the client to use your local endpoint
client = OpenAI(
    base_url="http://localhost:5272/v1",
    api_key="not-needed"  # API key isn't used but the client requires one
)

# Chat completion example
response = client.chat.completions.create(
    model="deepseek-r1-distill-qwen-1.5b-cpu-int4-rtn-block-32-acc-level-4",  # Use the id of your loaded model, found in 'foundry service ps'
    messages=[
        {"role": "system", "content": "You are a helpful assistant."},
        {"role": "user", "content": "What is the capital of France?"}
    ],
    max_tokens=1000
)

print(response.choices[0].message.content)
```
Check out the streaming example [here](../includes/integrate-examples/python.md).

### REST API

```bash
curl http://localhost:5272/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    model="deepseek-r1-distill-qwen-1.5b-cpu-int4-rtn-block-32-acc-level-4", 
    "messages": [
      {
        "role": "system",
        "content": "You are a helpful assistant."
      },
      {
        "role": "user",
        "content": "What is the capital of France?"
      }
    ],
    "max_tokens": 1000
  }'
```

Check out the streaming example [here](../includes/integrate-examples/rest.md).

### JavaScript

```javascript
import OpenAI from "openai";

// Configure the client to use your local endpoint
const openai = new OpenAI({
  baseURL: "http://localhost:5272/v1",
  apiKey: "not-needed", // API key isn't used but the client requires one
});

async function generateText() {
  const response = await openai.chat.completions.create({
    model: "deepseek-r1-distill-qwen-1.5b-cpu-int4-rtn-block-32-acc-level-4", // Use the id of your loaded model, found in 'foundry service ps'
    messages: [
      { role: "system", content: "You are a helpful assistant." },
      { role: "user", content: "What is the capital of France?" },
    ],
    max_tokens: 1000,
  });

  console.log(response.choices[0].message.content);
}

generateText();
```

Check out the streaming example [here](../includes/integrate-examples/javascript.md).

### C#

```csharp
using Azure.AI.OpenAI;
using Azure;

// Configure the client to use your local endpoint
OpenAIClient client = new OpenAIClient(
    new Uri("http://localhost:5272/v1"),
    new AzureKeyCredential("not-needed")  // API key isn't used but the client requires one
);

// Chat completion example
var chatCompletionsOptions = new ChatCompletionsOptions()
{
    Messages =
    {
        new ChatMessage(ChatRole.System, "You are a helpful assistant."),
        new ChatMessage(ChatRole.User, "What is the capital of France?")
    },
    MaxTokens = 1000
};

Response<ChatCompletions> response = await client.GetChatCompletionsAsync(
    "deepseek-r1-distill-qwen-1.5b-cpu-int4-rtn-block-32-acc-level-4", // Use the id of your loaded model, found in 'foundry service ps'
    chatCompletionsOptions
);

Console.WriteLine(response.Value.Choices[0].Message.Content);
```

Check out the streaming example [here](../includes/integrate-examples/csharp.md).

## Best Practices

1. **Error Handling**: Implement robust error handling to manage cases when the local service is unavailable or a model isn't loaded.
2. **Resource Management**: Be mindful of your local resources. Monitor CPU/RAM usage when making multiple concurrent requests.
3. **Fallback Strategy**: Consider implementing a fallback to cloud services for when local inference is insufficient.
4. **Model Preloading**: For production applications, ensure your model is preloaded before starting your application.

## Next steps

- [Compile Hugging Face models for Foundry Local](./compile-models-for-foundry-local.md)
- [Explore the AI Foundry Local CLI reference](../reference/reference-cli.md)
