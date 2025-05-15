# Get Started with Foundry Local

This guide provides detailed instructions on installing, configuring, and using Foundry Local to run AI models on your device.

## Prerequisites

- A PC with sufficient specifications to run AI models locally
  - Windows 10 or later
  - Greater than 8GB RAM
  - Greater than 10GB of free disk space for model caching (quantized Phi 3.2 models are ~3GB)
- Suggested hardware for optimal performance:
  - Windows 11
  - NVIDIA GPU (2000 series or newer) OR AMD GPU (6000 series or newer) OR Qualcomm Snapdragon X Elite, with 8GB or more of VRAM
  - Greater than 16GB RAM
  - Greater than 20GB of free disk space for model caching (the largest models are ~15GB)
- Administrator access to install software

## Installation

1. Download Foundry Local for your platform from the [releases page](https://github.com/microsoft/Foundry-Local/releases).
2. Install the package by following the on-screen prompts.
3. After installation, access the tool via command line with `foundry`.

## Running Your First Model

1. Open a command prompt or terminal window.
2. Run a model using the following command:

   ```bash
   foundry model run deepseek-r1-1.5b
   ```

   This command will:

   - Download the model to your local disk
   - Load the model into your device
   - Start a chat interface

**💡 TIP:** Replace `deepseek-r1-1.5b` with any model from the catalog. Use `foundry model list` to see available models.

## Explore Foundry Local CLI commands

The foundry CLI is structured into several categories:

- **Model**: Commands related to managing and running models
- **Service**: Commands for managing the Foundry Local service
- **Cache**: Commands for managing the local cache where models are stored

To see all available commands, use the help option:

```bash
foundry --help
```

**💡 TIP:** For a complete reference of all available CLI commands and their usage, see the [Foundry Local CLI Reference](./reference/reference-cli.md)

## Integrating with Applications

Foundry Local provides an OpenAI-compatible REST API at `http://localhost:PORT/v1`.

- Note that the port will be dynamically assigned, so check the logs for the correct port.

### REST API Example

```bash
curl http://localhost:5273/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "deepseek-r1-1.5b-cpu",
    "messages": [{"role": "user", "content": "What is the capital of France?"}],
    "temperature": 0.7,
    "max_tokens": 50
  }'
```

Read about all the samples we have for various languages and platforms in the [Integrate with Inference SDKs](./how-to/integrate-with-inference-sdks.md) section.

## Troubleshooting

### Common Issues and Solutions

| Issue                   | Possible Cause                          | Solution                                                                                  |
| ----------------------- | --------------------------------------- | ----------------------------------------------------------------------------------------- |
| Slow inference          | CPU-only model on large parameter count | Use GPU-optimized model variants when available                                           |
| Model download failures | Network connectivity issues             | Check your internet connection, try `foundry cache list` to verify cache state            |
| Service won't start     | Port conflicts or permission issues     | Try `foundry service restart` or post an issue providing logs with `foundry zip-logsrock` |

For more information, see the [troubleshooting guide](./reference/reference-troubleshooting.md).

## Next Steps

- [Learn more about Foundry Local](./what-is-foundry-local.md)
- [Integrate with inferencing SDKs](./how-to/integrate-with-inference-sdks.md)
- [Compile models for Foundry Local](./how-to/compile-models-for-foundry-local.md)
- [Build a chat application](./tutorials/chat-application-with-open-web-ui.md)
- [Use Langchain](./tutorials/use-langchain-with-foundry-local.md)
