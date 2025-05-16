<div align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="media/icons/ai_studio_icon_white.svg">
    <source media="(prefers-color-scheme: light)" srcset="media/icons/ai_studio_icon_black.svg">
    <img alt="AI Foundry icon." src="media/icons/ai_studio_icon_black.svg" height="100" style="max-width: 100%;">
  </picture>

<h1>Foundry Local</h1>

</div>

## Run AI models on your local device

Foundry Local brings the power of Azure AI Foundry to your local device. It allows you to:

- Run large language models (LLMs) directly on your hardware
- Keep all data processing on-device for enhanced privacy and security
- Integrate models with your applications through an OpenAI-compatible API
- Optimize performance using ONNX Runtime and hardware acceleration

## Quickstart

**ℹ️ INFO:** For a detailed installation guide, please refer to the [documentation section](./docs/README.md).

1. **Install Foundry Local**

   1. Download Foundry Local for your platform (Windows - x64/ARM) from the [releases page](https://github.com/microsoft/Foundry-Local/releases).
   2. Install the package by following the on-screen instructions.
   3. After installation, access the tool via command line with `foundry`.

2. **Run your first model**

   ```bash
   foundry model run deepseek-r1-1.5b
   ```

   **💡 TIP:** The `foundry model run <model>` command will automatically download the model if it is not already cached on your local machine, and then start an interactive chat session with the model. It will download the variant that will perform best on your hardware You're encouraged to try out different models by replacing `deepseek-r1-1.5b` with the name of any other model available in the catalog, located with the `foundry model list` command.

3. **Connect your applications**
Foundry Local has an easy-to-use SDK to get you started with existing applications:
```bash
pip install foundry-local-sdk
```


Foundry Local provides an OpenAI-compatible API that you can call from any application:
```python
# Simple Python example using OpenAI API
from openai import OpenAI
from foundry_local import FoundryLocalManager
manager = FoundryLocalManager()
modelinfo = manager.load_model("DeepSeek-R1-Distill-Qwen-1.5B")

# Configure the client to use your local endpoint
client = OpenAI(
    base_url=manager.endpoint,
    api_key=manager.api_key  # API key isn't used but the client requires one
)

# Chat completion example
response = client.chat.completions.create(
    model=modelinfo.id,  # Use the id of your loaded model, found in 'foundry service ps'
    messages=[
        {"role": "system", "content": "You are a helpful assistant."},
        {"role": "user", "content": "What is the capital of France?"}
    ],
    max_tokens=1000
)

print(response.choices[0].message.content)
```

## Controlling Foundry from your App

Start Foundry models from your apps using the `FoundryManager`, available for multiple runtimes as source and
packages:

* [Python](./sdk/python/README.md)
* [Node & JavaScript](./sdk/js/README.md)
* [C# (Preview)](./sdk/cs/src/README.md) - [simple sample](./sdk/cs/samples/ConsoleClient)

## Features & Use Cases

- **On-device inference** - Process sensitive data locally for privacy, reduced latency, and no cloud costs
- **OpenAI-compatible API** - Seamlessly integrate with applications using familiar SDKs
- **High performance** - Optimized execution with ONNX Runtime and hardware acceleration
- **Flexible deployment** - Ideal for edge computing scenarios with limited connectivity
- **Development friendly** - Perfect for prototyping AI features before production deployment
- **Model versatility** - Use pre-compiled models or [convert your own](./docs/how-to/compile-models-for-foundry-local.md).

## Reporting Issues

We're actively looking for feedback during this preview phase. Please report issues or suggest improvements in the [GitHub Issues](https://github.com/microsoft/Foundry-Local/issues) section.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Additional Resources

- [Detailed documentation](./docs/README.md)
- [CLI reference](./docs/reference/reference-cli.md)
- [REST API reference](./docs/reference/reference-rest.md)
- [Security and privacy](./docs/reference/reference-security-privacy.md)
- [Troubleshooting guide](./docs/reference/reference-troubleshooting.md)
