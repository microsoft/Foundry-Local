# Foundry Local Architecture

Foundry Local is designed to enable efficient, secure, and scalable AI model inference directly on local devices. This article explains the key components of the Foundry Local architecture and how they interact to deliver AI capabilities.

The benefits of Foundry Local include:

- **Low Latency**: By running models locally, Foundry Local minimizes the time it takes to process requests and return results.
- **Data Privacy**: Sensitive data can be processed locally without sending it to the cloud, ensuring compliance with data protection regulations.
- **Flexibility**: Foundry Local supports a wide range of hardware configurations, allowing users to choose the best setup for their needs.
- **Scalability**: Foundry Local can be deployed on various devices, from personal computers to powerful servers, making it suitable for different use cases.
- **Cost-Effectiveness**: Running models locally can reduce costs associated with cloud computing, especially for high-volume applications.
- **Offline Capabilities**: Foundry Local can operate without an internet connection, making it ideal for remote or disconnected environments.
- **Integration with Existing Workflows**: Foundry Local can be easily integrated into existing development and deployment workflows, allowing for a smooth transition to local inference.

## Key Components

The key components of the Foundry Local architecture are articulated in the following diagram:

![Foundry Local Architecture Diagram](../media/architecture/foundry-local-arch.png)

### Foundry Local Service

The Foundry Local Service is an OpenAI compatible REST server that provides a standardized interface for interacting with the inference engine and model management. Developers can use this API to send requests, run models, and retrieve results programmatically.

- **Endpoint**: `http://localhost:PORT/v1`
  - Note: The port is dynamically assigned, so check the logs for the correct port.
- **Use Cases**:
  - Integrating Foundry Local with custom applications.
  - Running models via HTTP requests.

### ONNX Runtime

The ONNX runtime is a core component responsible for running AI models. It uses optimized ONNX models to perform inference efficiently on local hardware, such as CPUs, GPUs, or NPUs.

**Features**:

- Supports multiple hardware providers (for example: NVIDIA, AMD, Intel) and devices (for example: NPUs, CPUs, GPUs).
- Provides a unified interface for running models on different hardware platforms.
- Best-in-class performance.
- Supports quantized models for faster inference.

### Model Management

Foundry Local provides robust tools for managing AI models, ensuring that they're readily available for inference and easy to maintain. Model management is handled through the **Model Cache** and the **Command-Line Interface (CLI)**.

#### Model Cache

The model cache is a local storage system where AI models are downloaded and stored. It ensures that models are available for inference without requiring repeated downloads. The cache can be managed using the Foundry CLI or REST API.

- **Purpose**: Reduces latency by storing models locally.
- **Management Commands**:
  - `foundry cache list`: Lists all models stored in the local cache.
  - `foundry cache remove <model-name>`: Deletes a specific model from the cache.
  - `foundry cache cd <path>`: Changes the directory where models are stored.

#### Model Lifecycle

1. **Download**: Models are downloaded from the Azure AI Foundry model catalog to local disk.
2. **Load**: Models are loaded into the Foundry Local service (and therefore memory) for inference. You can set a TTL (time-to-live) for how long the model should remain in memory (the default is 10 minutes).
3. **Run**: Models are inferenced.
4. **Unload**: Models can be unloaded from the inference engine to free up resources.
5. **Delete**: Models can be deleted from the local cache to free up disk space.

#### Model Compilation using Olive

Before models can be used with Foundry Local, they must be compiled and optimized in the [ONNX](https://onnx.ai) format. Microsoft provides a selection of published models in the Azure AI Foundry Model Catalog that are already optimized for Foundry Local. However, you aren't limited to those models - by using [Olive](https://microsoft.github.io/Olive/). Olive is a powerful framework for preparing AI models for efficient inference. It converts models into the ONNX format, optimizes their graph structure, and applies techniques like quantization to improve performance on local hardware.

**💡 TIP**: To learn more about compiling models for Foundry Local, read [Compile Hugging Face models for Foundry Local](../how-to/compile-models-for-foundry-local.md).

### Hardware Abstraction Layer

The hardware abstraction layer ensures that Foundry Local can run on various devices by abstracting the underlying hardware. To optimize performance based on the available hardware, Foundry Local supports:

- **multiple _execution providers_**, such as NVIDIA CUDA, AMD, Qualcomm, Intel.
- **multiple _device types_**, such as CPU, GPU, NPU.

### Developer Experiences

The Foundry Local architecture is designed to provide a seamless developer experience, enabling easy integration and interaction with AI models.

Developers can choose from various interfaces to interact with the system, including:

#### Command-Line Interface (CLI)

The Foundry CLI is a powerful tool for managing models, the inference engine, and the local cache.

**Examples**:

- `foundry model list`: Lists all available models in the local cache.
- `foundry model run <model-name>`: Runs a model.
- `foundry service status`: Checks the status of the service.

**💡 TIP**: To learn more about the CLI commands, read [Foundry Local CLI Reference](../reference/reference-cli.md).

#### Inferencing SDK Integration

Foundry Local supports integration with various SDKs, such as the OpenAI SDK, enabling developers to use familiar programming interfaces to interact with the local inference engine.

- **Supported SDKs**: Python, JavaScript, C#, and more.

**💡 TIP**: To learn more about integrating with inferencing SDKs, read [Integrate Foundry Local with Inferencing SDKs](../how-to/integrate-with-inference-sdks.md).

#### AI Toolkit for Visual Studio Code

The AI Toolkit for Visual Studio Code provides a user-friendly interface for developers to interact with Foundry Local. It allows users to run models, manage the local cache, and visualize results directly within the IDE.

- **Features**:
  - Model management: Download, load, and run models from within the IDE.
  - Interactive console: Send requests and view responses in real-time.
  - Visualization tools: Graphical representation of model performance and results.

## Next Steps

- [Get started with Foundry Local](../get-started.md)
- [Integrate with Inference SDKs](../how-to/integrate-with-inference-sdks.md)
- [Foundry Local CLI Reference](../reference/reference-cli.md)
