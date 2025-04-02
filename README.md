# AI Foundry Local (Private Preview)

Welcome to the AI Foundry Local private preview! This tool enables you to run powerful AI models directly on your device and seamlessly integrate them into your applications. Experience high-performance, on-device inference with complete data privacy.

## What is AI Foundry Local?

AI Foundry Local brings the power of Azure AI Foundry to your local device. It allows you to:

- Run large language models (LLMs) directly on your hardware
- Keep all data processing on-device for enhanced privacy and security
- Integrate models with your applications through an OpenAI-compatible API
- Optimize performance using ONNX Runtime and hardware acceleration

## Quickstart

**ℹ️ INFO:** For a detailed installation guide, please refer to the [documentation section](./docs/README.md).

1. **Install Foundry Local**

   *Windows:*

   1. Download AI Foundry Local for your platform (Windows - x64/ARM) from the [releases page](https://github.com/microsoft/Foundry-Local/releases).
   2. Install the package by following the on-screen instructions.
   3. After installation, access the tool via command line with `foundry`.
      
   *MacOS / Linux:*
   1. Download AI Foundry Local for your platform (MacOS, Linux - x64/ARM) from the [releases page](https://github.com/microsoft/Foundry-Local/releases).
   2. Run the individual components in separate terminals (**💡 NOTE:** this workflow is not the most user friendly and is a work in progress.)
      1. Make the Neutron Server (`Inference.Service.Agent`), executable: `chmod +x Inference.Service.Agent`, then run it: `./Inference.Service.Agent`
      2. Make the Foundry Local Client (`foundry`) executable: `chmod +x foundry`, and optionally add it to your PATH. Then, run it: `./foundry` 
   3. After installation, access the tool via command line with `foundry` (or `./foundry` in the right directory if not added to path).

3. **Run your first model**

   ```bash
   foundry model run deepseek-r1-1.5b-cpu
   ```

   **💡 TIP:** The `foundry model run <model>` command will automatically download the model if it is not already cached on your local machine, and then start an interactive chat session with the model. You're encouraged to try out different models by replacing `deepseek-r1-1.5b-cpu` with the name of any other model available in the catalog, located with the `foundry model list` command.

4. **Connect your applications**

      AI Foundry Local provides an OpenAI-compatible API that you can call from any application:
      
      ```javascript
      // Simple JavaScript example
      const response = await fetch("http://localhost:5272/v1/chat/completions", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          model: "deepseek-r1-1.5b-cpu",
          messages: [{ role: "user", content: "What is AI Foundry Local?" }],
          temperature: 0.7,
          max_tokens: 100,
        }),
      });
      
      console.log(response.choices[0].message.content);
      ```

## Features & Use Cases

- **On-device inference** - Process sensitive data locally for privacy, reduced latency, and no cloud costs
- **OpenAI-compatible API** - Seamlessly integrate with applications using familiar SDKs
- **High performance** - Optimized execution with ONNX Runtime and hardware acceleration
- **Flexible deployment** - Ideal for edge computing scenarios with limited connectivity
- **Development friendly** - Perfect for prototyping AI features before production deployment
- **Model versatility** - Use pre-compiled models or [convert your own](./docs/how-to/compile-models-for-foundry-local.md).

## Reporting Issues

We're actively looking for feedback during this preview phase. Please report issues or suggest improvements in the [GitHub Issues](https://github.com/microsoft/Foundry-Local/issues) section.

Presently, we are aware of the following issues: 
* phi4-gpu model is underperforming in toks/sec  

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Additional Resources

- [Detailed documentation](./docs/README.md)
- [CLI reference](./docs/reference/reference-cli.md)
- [REST API reference](./docs/reference/reference-rest.md)
- [Security and privacy](./docs/reference/reference-security-privacy.md)
- [Troubleshooting guide](./docs/reference/reference-troubleshooting.md)
