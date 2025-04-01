---
title: Get started with AI Foundry Local
titleSuffix: AI Foundry Local
description: Learn how to install, configure, and run your first AI model with AI Foundry Local
manager: scottpolly
keywords: Azure AI services, cognitive, AI models, local inference
ms.service: azure-ai-foundry
ms.topic: quickstart
ms.date: 02/20/2025
ms.reviewer: samkemp
ms.author: samkemp
author: samuel100
ms.custom: build-2025
#customer intent: As a developer, I want to get started with AI Foundry Local so that I can run AI models locally.
---

# Get started with AI Foundry Local

This article shows you how to get started with AI Foundry Local to run AI models on your device. Follow these steps to install the tool, discover available models, and run your first local AI model.

## Prerequisites

- A PC with sufficient specifications to run AI models locally
  - Windows 10 or later
  - Greater than 8GB RAM
  - Greater than 3GB of free disk space for model caching (quantized Phi 3.2 models are ~3GB)
- Suggested hardware for optimal performance:
  - Windows 11
  - NVIDIA GPU (2000 series or newer) OR AMD GPU (6000 series or newer) OR Qualcomm Snapdragon X Elite, with 8GB or more of VRAM
  - Greater than 16GB RAM
  - Greater than 15GB of free disk space for model caching (the largest models are ~15GB)
- Administrator access to install software

## Quickstart in 2-steps

Follow these steps to get started with AI Foundry Local:

1. **Install Foundry Local**

   1. Download AI Foundry Local for your platform (Windows, MacOS, Linux - x64/ARM) from the repository's releases page.
   2. Install the package by following the on-screen prompts.

      **IMPORTANT: For MacOS/Linux users:** Run both components in separate terminals:

      - Neutron Server (`Inference.Service.Agent`) - Use `chmod +x Inference.Service.Agent` to make executable
      - Foundry Client (`foundry`) - Use `chmod +x foundry` to make executable, and add to your PATH

   3. After installation, access the tool via command line with `foundry`.

2. **Run your first model**
   1. Open a command prompt or terminal window.
   2. Run the DeepSeek-R1 model on the CPU using the following command:
      ```bash
      foundry model run deepseek-r1-1.5b-cpu
      ```

**💡 TIP:** The `foundry model run <model>` command will automatically download the model if it is not already cached on your local machine, and then start an interactive chat session with the model. You're encouraged to try out different models by replacing `deepseek-r1-1.5b-cpu` with the name of any other model available in the catalog, located with the `foundry model list` command.

## Explore Foundry Local CLI commands

The foundry CLI is structured into several categories:

- **Model**: Commands related to managing and running models
- **Service**: Commands for managing the AI Foundry Local service
- **Cache**: Commands for managing the local cache where models are stored

To see all available commands, use the help option:

```bash
foundry --help
```

**💡 TIP:** For a complete reference of all available CLI commands and their usage, see the [Foundry Local CLI Reference](./reference/reference-cli.md)

## Security and privacy considerations

AI Foundry Local is designed with privacy and security as core principles:

- **Local processing**: All data processed by AI Foundry Local remains on your device and is never sent to Microsoft or any external services.
- **No telemetry**: AI Foundry Local does not collect usage data or model inputs.
- **Air-gapped environments**: AI Foundry Local can be used in disconnected environments after initial model download.

### Security best practices

- Use AI Foundry Local in environments that align with your organization's security policies.
- For handling sensitive data, ensure your device meets your organization's security requirements.
- Consider disk encryption for devices where cached models might contain sensitive fine-tuning data.

### Licensing considerations

Models available through AI Foundry Local are subject to their original licenses:

- Open-source models maintain their original licenses (e.g., Apache 2.0, MIT).
- Commercial models may have specific usage restrictions or require separate licensing.
- Always review the licensing information for each model before deploying in production.

## Production deployment scope

AI Foundry Local is designed primarily for:

- Individual developer workstations
- Single-node deployment
- Local application development and testing

**⚠️ IMPORTANT:** AI Foundry Local is not currently intended for distributed, containerized, or multi-machine production deployment. For production-scale deployment needs, consider Azure AI Foundry for enterprise-grade availability and scale.

## Troubleshooting

### Common issues and solutions

| Issue                   | Possible Cause                          | Solution                                                                                  |
| ----------------------- | --------------------------------------- | ----------------------------------------------------------------------------------------- |
| Slow inference          | CPU-only model on large parameter count | Use GPU-optimized model variants when available                                           |
| Model download failures | Network connectivity issues             | Check your internet connection, try `foundry cache list` to verify cache state            |
| Service won't start     | Port conflicts or permission issues     | Try `foundry service restart` or post an issue providing logs with `foundry zip-logsrock` |

### Diagnosing performance issues

If you're experiencing slow inference:

1. Check that you're using GPU acceleration if available
2. Monitor memory usage during inference to detect bottlenecks
3. Consider a more quantized model variant (e.g., INT8 instead of FP16)
4. Experiment with batch sizes for non-interactive workloads

## Next steps

- [Learn how to integrate AI Foundry Local with your applications](./how-to/integrate-with-inference-sdks.md)
- [Explore the AI Foundry Local documentation](./index.yml)
