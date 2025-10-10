---
title: Features
description: Explore the powerful capabilities of Foundry Local for local AI model deployment
---

# Features

Foundry Local brings the enterprise capabilities of Microsoft's Azure AI Foundry to your local environment, with features designed for developers, data scientists, and organizations requiring secure AI workflows.

## Local Model Execution

Run powerful AI models entirely on your own hardware without sending data to external services.

### Supported Capabilities

- Run large language models (LLMs) like GPT variants locally
- Support for multimodal models (text, images)
- Fine-tune models on your own data
- API-compatible with Azure AI models
- Automatic hardware optimization
- Format-preserving inference results

```python
# Example model execution
response = client.chat.completions.create(
    model="mistral-7b-local",
    messages=[
        {"role": "system", "content": "You are a helpful assistant."},
        {"role": "user", "content": "Write a Python function to calculate prime numbers."}
    ],
    temperature=0.7
)
```

## Hardware Optimization

Foundry Local automatically optimizes models for your specific hardware configuration.

### Optimization Features

- Automatic quantization (4-bit, 8-bit options)
- CPU, GPU, and multi-GPU support
- Memory usage optimization
- Batch processing capabilities
- Adaptive throughput management
- Thermal monitoring and throttling protection

## API Compatibility

Maintain full compatibility with Azure AI APIs for seamless transition between local and cloud environments.

### API Features

- Drop-in replacement for Azure AI REST APIs
- Python SDK compatibility
- OpenAI-compatible endpoints
- WebSocket support for streaming responses
- Authentication and rate limiting options
- Comprehensive API documentation

```typescript
// Example REST API call
const response = await fetch('http://localhost:8080/v1/chat/completions', {
	method: 'POST',
	headers: { 'Content-Type': 'application/json' },
	body: JSON.stringify({
		model: 'llama-3-8b-local',
		messages: [{ role: 'user', content: 'Explain the importance of local AI inference' }]
	})
});
```

## Security & Privacy

Enterprise-grade security features ensure your data and models remain protected.

### Security Features

- No data leaves your environment
- Model integrity verification
- Encrypted model storage
- Access control and authentication
- Audit logging capabilities
- Compliance documentation

## Model Management

Comprehensive tools to manage your local model repository.

### Management Features

- Browse and download compatible models
- Model versioning and rollback
- Usage statistics and benchmarks
- Custom model importing
- Disk usage optimization
- Update notifications

## Extensibility

Extend Foundry Local with plugins and custom integrations.

### Extensibility Options

- Plugin architecture
- Custom model adapters
- Pre/post processing hooks
- Integration with vector databases
- Metrics collection and export
- Custom UI extensions

```python
# Example plugin registration
from foundry_local.plugins import register_plugin

@register_plugin
class CustomModelAdapter:
    """Custom adapter for proprietary models"""

    def __init__(self, config):
        self.config = config

    def transform_input(self, prompt):
        # Custom preprocessing
        return modified_prompt
```

All features are designed with enterprise requirements in mind, providing a secure, efficient, and compatible environment for local AI development and deployment.
