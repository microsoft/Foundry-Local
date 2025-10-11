---
title: Get Started
description: Learn how to set up and start using Foundry Local - your gateway to running Microsoft's AI models on your own hardware.
---

# Getting Started

Foundry Local brings the power of Microsoft's Azure AI models to your local environment, enabling secure, private AI development and inference without sending data to the cloud.

## Prerequisites

Before you begin, ensure you have:

- A compatible system with at least 16GB RAM
- NVIDIA GPU with CUDA support (recommended)
- Docker Desktop installed and running
- 10GB+ of free disk space
- Windows 10/11, macOS, or Linux

## Quick Start

1. Download and install Foundry Local:

   ```bash
   # Linux/macOS
   curl -sSL https://get.foundrylocal.ai | bash

   # Windows (PowerShell as Administrator)
   iwr -useb https://get.foundrylocal.ai/install.ps1 | iex
   ```

2. Verify installation:

   ```bash
   foundry-local --version
   ```

3. Start the Foundry Local service:

   ```bash
   foundry-local start
   ```

4. Open the web interface at `http://localhost:8080`

## Core Features

Foundry Local provides an Azure AI-compatible environment running entirely on your hardware:

- **Local Model Running**: Execute AI models directly on your machine with no data leaving your environment
- **Azure AI Compatibility**: Use the same APIs and SDKs as Azure AI Foundry
- **Model Management**: Easy download and management of compatible foundation models
- **Inference Optimization**: Automatic quantization and optimization for your specific hardware
- **Python & REST APIs**: Comprehensive programmatic access to all functionality

## Using Models

Access models through the Python SDK:

```python
from foundry_local import FoundryClient

# Initialize the client
client = FoundryClient()

# Run a completion with a local model
response = client.completions.create(
    model="gpt-4-mini-local",
    prompt="Explain quantum computing in simple terms",
    max_tokens=500
)

print(response.choices[0].text)
```

Or use the REST API:

```bash
curl http://localhost:8080/v1/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "gpt-4-mini-local",
    "prompt": "Explain quantum computing in simple terms",
    "max_tokens": 500
  }'
```

## Next Steps

- Explore the [Model Hub](/docs/models) to see available models
- Learn about [Configuration](/docs/configuration) options
- Check out [Advanced Usage](/docs/advanced-usage) for power users

## Need Help?

- Browse our [documentation](/docs)
- Report issues on [GitHub](https://github.com/microsoft/foundry-local/issues)
- Join our [Discord community](https://discord.gg/foundrylocal)
