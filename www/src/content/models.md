---
title: Model Hub
description: Explore the AI models available for local deployment with Foundry Local
---

# Model Hub

Foundry Local provides access to a wide range of optimized AI models that can run entirely on your local hardware. These models deliver powerful AI capabilities without sending your data to the cloud.

## Available Models

Below is a catalog of the most popular models available in Foundry Local. All models are automatically optimized for your specific hardware configuration.

### Large Language Models (LLMs)

| Model Name               | Parameters | Size (Quantized) | Description                                                                 |
| ------------------------ | ---------- | ---------------- | --------------------------------------------------------------------------- |
| **gpt-4-mini-local**     | 8B         | 4-12 GB          | Lightweight but powerful GPT model optimized for general knowledge tasks    |
| **llama-3-8b-local**     | 8B         | 4-12 GB          | Open-source model from Meta with strong reasoning capabilities              |
| **mistral-7b-local**     | 7B         | 4-10 GB          | High-quality open-source model with excellent coding abilities              |
| **codellama-7b-local**   | 7B         | 4-10 GB          | Specialized for code generation and software development tasks              |
| **phi-3-mini-local**     | 3.8B       | 2-6 GB           | Microsoft's compact yet capable model for resource-constrained environments |
| **tinyllama-1.1b-local** | 1.1B       | 0.6-2 GB         | Ultra-lightweight model for devices with minimal resources                  |

### Text Embeddings

| Model Name                       | Dimensions | Size       | Description                                                       |
| -------------------------------- | ---------- | ---------- | ----------------------------------------------------------------- |
| **text-embedding-3-small-local** | 1536       | 0.3-1 GB   | Embeddings model for semantic search and text similarity          |
| **text-embedding-3-large-local** | 3072       | 0.8-2 GB   | Higher-dimensional embeddings for more precise similarity metrics |
| **bge-small-local**              | 384        | 0.1-0.4 GB | Compact embeddings model optimized for speed                      |

### Multimodal Models

| Model Name             | Capabilities     | Size    | Description                                             |
| ---------------------- | ---------------- | ------- | ------------------------------------------------------- |
| **gpt-4-vision-local** | Text+Image       | 8-20 GB | Process and reason about images and text together       |
| **clip-local**         | Image Embeddings | 1-3 GB  | Generate embeddings from images for similarity matching |
| **whisper-local**      | Speech-to-Text   | 1-3 GB  | Transcribe and translate audio to text                  |

## Hardware Requirements

Model performance depends on your hardware configuration. Below are general guidelines:

### Minimum System Requirements

- **CPU-only**: Most models will run on modern CPUs but with reduced performance
- **RAM**: 16GB minimum (8GB for smallest models only)
- **Storage**: 10GB for installation + 2-20GB per model
- **OS**: Windows 10/11, macOS 12+, or Linux (Ubuntu 20.04+, Debian 11+)

### Recommended Configuration

- **GPU**: NVIDIA GPU with at least 8GB VRAM
- **RAM**: 32GB or more
- **CPU**: 8+ cores
- **Storage**: SSD with 100GB+ free space
- **CUDA**: Version 11.7 or newer

## Quantization Options

Each model can be run at different quantization levels to balance performance and resource usage:

| Quantization | Memory Usage | Performance | Use Case                                     |
| ------------ | ------------ | ----------- | -------------------------------------------- |
| **float32**  | Highest      | Best        | When maximum accuracy is required            |
| **float16**  | High         | Very Good   | Standard for most GPU deployments            |
| **int8**     | Medium       | Good        | Balanced performance on constrained hardware |
| **int4**     | Low          | Acceptable  | Resource-constrained environments            |

## Model Management

Foundry Local provides tools to manage your model library:

```bash
# List all available models
foundry-local models list

# Download a specific model
foundry-local models download gpt-4-mini-local

# List downloaded models
foundry-local models list --downloaded

# Remove a model to free up space
foundry-local models remove llama-3-8b-local

# Update models to latest versions
foundry-local models update
```

## Custom Model Integration

You can integrate your own fine-tuned models or models from Hugging Face:

```yaml
# custom-model.yaml
name: my-custom-bert
source: huggingface
repo: your-username/your-fine-tuned-model
format: gguf
quantization: q4_k_m
context_length: 2048
```

Import using:

```bash
foundry-local models import custom-model.yaml
```

## Model Performance Metrics

Performance will vary based on your specific hardware. These benchmarks are from a system with an NVIDIA RTX 3090:

| Model                | Tokens/Second | Max Context | Memory Usage |
| -------------------- | ------------- | ----------- | ------------ |
| gpt-4-mini-local     | 30-40         | 8192        | 8GB          |
| llama-3-8b-local     | 25-35         | 8192        | 9GB          |
| mistral-7b-local     | 30-40         | 8192        | 8GB          |
| phi-3-mini-local     | 50-70         | 4096        | 5GB          |
| tinyllama-1.1b-local | 100-150       | 2048        | 2GB          |

## Model Updates

Models receive regular updates with improved capabilities:

- Security patches
- Performance optimizations
- New features and capabilities
- Training data refreshes

Enable automatic updates in your configuration or check for updates manually:

```bash
foundry-local models check-updates
```

## Learn More

- [Model Documentation](/docs/models/documentation)
- [Performance Optimization](/docs/advanced-usage/performance)
- [Fine-tuning Guide](/docs/advanced-usage/fine-tuning)
- [Compatibility with Azure AI](/docs/azure-compatibility)
