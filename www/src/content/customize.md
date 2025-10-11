---
title: Configuration Guide
description: Learn how to customize Foundry Local to meet your specific technical requirements
---

# Configuration Guide

This guide explains how to configure Foundry Local to optimize performance, manage resources, and customize behavior for your specific environment and use cases.

## Global Configuration

The primary way to configure Foundry Local is through the `config.yaml` file located in the installation directory. This file follows a structured schema:

```yaml
version: 1.0.0
server:
  host: 0.0.0.0
  port: 8080
  max_request_size_mb: 100
  logging:
    level: info
    format: json

models:
  default_model: gpt-4-mini-local
  cache_dir: /path/to/model/cache
  download_concurrency: 2

hardware:
  gpu:
    enabled: true
    device_ids: [0, 1] # Use specific GPUs
    memory_limit_mb: 8192
  cpu:
    threads: 8

security:
  api_keys:
    enabled: false # Set to true to require API keys
    key_file: /path/to/api/keys.json
  cors:
    origins: ['http://localhost:3000']

storage:
  vector_db:
    type: chroma
    path: /path/to/vector/db
```

## Model Configuration

Individual model settings can be customized in the `models` directory, with one file per model:

```yaml
# models/llama-3-8b-local.yaml
name: llama-3-8b-local
source: huggingface
repo: meta-llama/Llama-3-8B
format: gguf
quantization: q4_k_m
context_length: 8192
batch_size: 32
system_prompt: 'You are a helpful AI assistant.'
parameters:
  temperature: 0.7
  top_p: 0.9
  repetition_penalty: 1.1
```

## API Endpoints

Configure which API endpoints are enabled and their specific behaviors:

```yaml
# endpoints.yaml
completions:
  enabled: true
  rate_limit: 60 # requests per minute

chat:
  enabled: true
  rate_limit: 60

embeddings:
  enabled: true
  rate_limit: 120

images:
  enabled: false # disable image generation
```

## Hardware Optimization

Foundry Local automatically detects your hardware capabilities, but you can fine-tune settings for optimal performance:

### GPU Settings

For NVIDIA GPU users:

```yaml
hardware:
  gpu:
    enabled: true
    precision: float16 # Options: float32, float16, bfloat16
    memory_efficient_attention: true
    device_map: auto # or specify manual mapping
    offload:
      cpu: false
      disk: false
```

### CPU-Only Setup

For systems without compatible GPUs:

```yaml
hardware:
  gpu:
    enabled: false
  cpu:
    threads: 16 # Adjust based on your CPU
    quantization: int8 # More aggressive quantization for CPU
    batch_size: 4 # Smaller batch size for CPU
```

## Memory Management

Configure how Foundry Local manages memory for different model sizes:

```yaml
memory:
  max_ram_usage_gb: 12
  swap_buffer_gb: 2
  offload_strategy: sequential # Options: sequential, balanced, performance
  cache_strategy:
    type: lru
    max_size_gb: 4
```

## Plugin System

Enable and configure extensions through the plugin system:

```yaml
plugins:
  directory: /path/to/plugins
  enabled:
    - vector_store_connector
    - custom_output_formatter
  disabled:
    - experimental_features
  settings:
    vector_store_connector:
      db_url: 'postgres://user:password@localhost/vectors'
```

## Environment Variables

Many configuration options can be overridden using environment variables:

```bash
# Override config file settings
export FOUNDRY_LOCAL_PORT=9000
export FOUNDRY_LOCAL_GPU_ENABLED=true
export FOUNDRY_LOCAL_DEFAULT_MODEL=mistral-7b-local
export FOUNDRY_LOCAL_LOG_LEVEL=debug
```

## Security Options

Configure authentication and authorization:

```yaml
security:
  authentication:
    method: api_key # Options: none, api_key, oauth
    api_keys:
      - name: development
        key: sk-devapikey123
        permissions: ['completions', 'chat', 'embeddings']
      - name: production
        key: sk-prodapikey456
        permissions: ['completions', 'chat']

  network:
    allow_external_connections: false
    allowed_ips: ['192.168.1.0/24']
    ssl:
      enabled: true
      cert_file: /path/to/cert.pem
      key_file: /path/to/key.pem
```

## Example Configurations

### High-Performance Setup

For systems with powerful GPUs:

```yaml
hardware:
  gpu:
    enabled: true
    precision: float16
    device_ids: [0, 1, 2, 3]
    memory_efficient_attention: true

models:
  preload: ['gpt-4-mini-local', 'llama-3-8b-local']
  parallel_loading: true

performance:
  batch_size: 64
  kv_cache_enabled: true
  compile_graphs: true
```

### Low-Resource Setup

For systems with limited resources:

```yaml
hardware:
  gpu:
    enabled: false
  cpu:
    threads: 4

models:
  default_model: tinyllama-1.1b-local
  quantization: int4

memory:
  max_ram_usage_gb: 4
  offload_strategy: disk
```

Remember to restart the Foundry Local service after making configuration changes for them to take effect.
