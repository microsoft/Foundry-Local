# Foundry Local C++ SDK

The Foundry Local C++ SDK provides a C++17 static library for running AI models locally via [Foundry Local](https://www.foundrylocal.ai/). Discover, download, load, and run inference entirely on your own machine — no cloud required.

> **Windows-only** — requires MSVC or clang-cl (MSVC-compatible toolchain).

## Features

- **Model catalog** — browse and search all available models; filter by cached or loaded state
- **Lifecycle management** — download, load, unload, and remove models programmatically
- **Chat completions** — synchronous and streaming via OpenAI-compatible types
- **Audio transcription** — transcribe audio files with streaming support
- **Tool calling** — define tools and handle tool-call responses in chat completions
- **Download progress** — wire up a callback for real-time download percentage
- **Model variants** — select specific hardware/quantization variants per model alias
- **Optional web service** — start an OpenAI-compatible REST endpoint
- **Execution providers** — ensure EPs are downloaded and registered for hardware acceleration

## Prerequisites

| Requirement | Notes |
|---|---|
| **Foundry Local / AI Toolkit** | Install via `winget install Microsoft.AIToolkit` or the VS Code AI Toolkit extension |
| **CMake >= 3.20** | Ships with Visual Studio 2022 |
| **Ninja** | Ships with Visual Studio 2022 |
| **vcpkg** | Set the `VCPKG_ROOT` environment variable to your vcpkg installation |
| **MSVC** (or clang-cl) | Visual Studio 2022 Build Tools or full IDE |

## Building from Source

### 0. Open an x64 developer environment

All commands below must run in a shell that has the x64 MSVC toolchain on the PATH.
Choose **one** of the following:

| Method | How to open |
|---|---|
| **Developer Command Prompt** | Start Menu → *"x64 Native Tools Command Prompt for VS 2022"* |
| **Developer PowerShell** | Start Menu → *"Developer PowerShell for VS 2022"* |
| **Inside an existing cmd** | Run `"<VS_INSTALL>\VC\Auxiliary\Build\vcvars64.bat"` where `<VS_INSTALL>` is your Visual Studio installation path (e.g. `C:\Program Files\Microsoft Visual Studio\2022\Enterprise`) |
| **VS Code terminal** | Open the project folder in VS Code with the CMake Tools extension; it configures the environment automatically |

Verify by running `cl.exe` — the banner should say **x64**.

### 1. Clone & navigate

```bash
git clone <repo-url>
cd foundry-local-sdk/sdk/cpp
```

### 2. Configure (CMake + vcpkg)

```bash
cmake --preset x64-debug
```

This uses the `x64-debug` preset which:
- Uses the **Ninja** generator
- Resolves dependencies via **vcpkg** (`nlohmann-json`, `ms-gsl`, `wil`, `gtest`)
- Builds with the `x64-windows-static-md` triplet

### 3. Build

```bash
cmake --build --preset x64-debug
```

### Release build

```bash
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release
```

## Quick Start

```cpp
#include "foundry_local.h"
#include <iostream>

using namespace foundry_local;

int main() {
    // 1. Create the manager
    Manager::Create({"MyApp"});
    auto& manager = Manager::Instance();

    // 2. Get a model from the catalog
    auto& catalog = manager.GetCatalog();
    auto* model = catalog.GetModel("phi-3.5-mini");
    if (!model) return 1;

    // 3. Download (if needed) and load
    model->Download();
    model->Load();

    // 4. Chat
    OpenAIChatClient chat(*model);
    std::vector<ChatMessage> messages = {{"user", "Hello!"}};
    ChatSettings settings;
    settings.max_tokens = 128;

    auto response = chat.CompleteChat(messages, settings);
    if (!response.choices.empty() && response.choices[0].message) {
        std::cout << response.choices[0].message->content << "\n";
    }

    // 5. Cleanup
    model->Unload();
    Manager::Destroy();
}
```

## Usage

### Initialization

`Manager` is a singleton. Call `Create` once at startup:

```cpp
Manager::Create(Configuration{"MyApp"}, &myLogger);
```

Access it anywhere afterward via `Manager::Instance()`. Check `Manager::IsInitialized()` to verify creation.

Call `Manager::Destroy()` to perform deterministic cleanup when done.

### Catalog

The catalog lists all models known to the Foundry Local Core:

```cpp
auto& catalog = Manager::Instance().GetCatalog();

// List all available models
auto models = catalog.ListModels();
for (auto* m : models)
    std::cout << m->GetAlias() << " — " << m->GetId() << "\n";

// Get a specific model by alias
auto* model = catalog.GetModel("phi-3.5-mini");

// Get a specific variant by its unique model ID
auto* variant = catalog.GetModelVariant("phi-3.5-mini-generic-gpu-4");

// List models already downloaded to the local cache
auto cached = catalog.GetCachedModels();

// List models currently loaded in memory
auto loaded = catalog.GetLoadedModels();
```

### Model Lifecycle

Each model may have multiple variants (different quantizations, hardware targets). The SDK auto-selects the best variant, or you can pick one.

```cpp
// Check and select variants
if (auto* concrete = dynamic_cast<Model*>(model)) {
    for (const auto& v : concrete->GetAllModelVariants()) {
        std::cout << v.GetId() << " (cached: " << v.IsCached() << ")\n";
    }
    // Switch to a specific variant (e.g., CPU)
    for (const auto& variant : concrete->GetAllModelVariants()) {
        if (variant.GetInfo().runtime &&
            variant.GetInfo().runtime->device_type == DeviceType::CPU) {
            concrete->SelectVariant(variant);
            break;
        }
    }
}
```

Download, load, and unload:

```cpp
// Download with progress reporting
model->Download([](float progress) {
    std::cout << "Download: " << progress << "%\n";
});

// Load into memory
model->Load();

// Unload when done
model->Unload();

// Remove from local cache entirely
model->RemoveFromCache();
```

### Chat Completions

```cpp
OpenAIChatClient chat(*model);

std::vector<ChatMessage> messages = {
    {"system", "You are a helpful assistant."},
    {"user", "Explain async/await in C#."}
};
ChatSettings settings;

auto response = chat.CompleteChat(messages, settings);
if (!response.choices.empty() && response.choices[0].message) {
    std::cout << response.choices[0].message->content << "\n";
}
```

### Streaming

Use a callback for token-by-token output:

```cpp
chat.CompleteChatStreaming(messages, settings, [](const ChatCompletionCreateResponse& chunk) {
    if (!chunk.choices.empty() && chunk.choices[0].delta) {
        std::cout << chunk.choices[0].delta->content << std::flush;
    }
});
```

### Chat Settings

Tune generation parameters per request:

```cpp
ChatSettings settings;
settings.temperature = 0.7f;
settings.max_tokens = 256;
settings.top_p = 0.9f;
settings.frequency_penalty = 0.5f;
```

### Audio Transcription

```cpp
OpenAIAudioClient audio(*model);

// One-shot transcription
auto result = audio.TranscribeAudio(R"(C:\path\to\audio.wav)");
std::cout << result.text << "\n";

// Streaming transcription
audio.TranscribeAudioStreaming(R"(C:\path\to\audio.wav)", [](const AudioCreateTranscriptionResponse& chunk) {
    std::cout << chunk.text;
});
```

### Tool Calling

See `sample/main.cpp` (Example 5) for a full tool-calling walkthrough.

### Web Service

Start an OpenAI-compatible REST endpoint for use by external tools or processes:

```cpp
Configuration config{"MyApp"};
config.web = WebServiceConfig{ "http://127.0.0.1:5000" };
Manager::Create(std::move(config));

Manager::Instance().StartWebService();
auto urls = Manager::Instance().GetUrls();
for (const auto& url : urls)
    std::cout << "Listening on: " << url << "\n";

// ... use the service ...

Manager::Instance().StopWebService();
```

### Execution Providers

Ensure EPs are downloaded and registered for hardware acceleration:

```cpp
Manager::Instance().EnsureEpsDownloaded();
```

## Configuration

| Property | Type | Default | Description |
|---|---|---|---|
| `app_name` | `std::string` | (required) | Your application name |
| `app_data_dir` | `optional<path>` | `~/.{app_name}` | Application data directory |
| `model_cache_dir` | `optional<path>` | `{app_data_dir}/cache/models` | Where models are stored locally |
| `logs_dir` | `optional<path>` | `{app_data_dir}/logs` | Log output directory |
| `log_level` | `LogLevel` | `Warning` | Verbose, Debug, Information, Warning, Error, Fatal |
| `web` | `optional<WebServiceConfig>` | `nullopt` | Web service configuration (see below) |
| `additional_settings` | `optional<unordered_map>` | `nullopt` | Extra key-value settings passed to Core |

**WebServiceConfig**

| Property | Type | Default | Description |
|---|---|---|---|
| `urls` | `optional<string>` | `127.0.0.1:0` | Bind address; semicolon-separated for multiple |
| `external_url` | `optional<string>` | `nullopt` | URI for accessing the web service in a separate process |

## API Reference

Key types:

| Type | Description |
|---|---|
| `Manager` | Singleton entry point — create, catalog, web service |
| `Configuration` | Initialization settings |
| `Catalog` | Model catalog — list, search, filter |
| `IModel` | Model interface — identity, metadata, lifecycle |
| `Model` | Model with variant selection (implements `IModel`) |
| `ModelVariant` | A specific variant of a model (implements `IModel`) |
| `OpenAIChatClient` | Chat completions (sync + streaming) |
| `OpenAIAudioClient` | Audio transcription (sync + streaming) |
| `ChatSettings` | Chat generation parameters |
| `ModelInfo` | Full model metadata record |

## Tests

```bash
ctest --preset x64-debug
```

Or run the test executable directly:

```bash
.\out\build\x64-debug\CppSdkTests.exe
```

## Project Structure

```
sdk/cpp/
├── include/                  # Public headers
│   ├── foundry_local.h       # Umbrella header (include this)
│   ├── configuration.h       # Configuration struct
│   ├── foundry_local_manager.h  # Manager singleton
│   ├── catalog.h             # Model catalog
│   ├── model.h               # Model & ModelVariant
│   ├── logger.h              # ILogger interface
│   └── openai/
│       ├── openai_chat_client.h    # Chat completion client
│       ├── openai_audio_client.h   # Audio transcription client
│       └── openai_tool_types.h     # Tool calling types
├── src/                      # Private implementation
├── sample/
│   └── main.cpp              # Sample application
├── test/                     # Unit & E2E tests (GTest)
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json                # vcpkg dependencies
└── vcpkg-configuration.json
```

## Troubleshooting

| Error | Cause | Fix |
|---|---|---|
| `DML provider requested, but GenAI has not been built with DML support` | GPU variant selected but ONNX Runtime GenAI lacks DML | Select a CPU variant or update Foundry Local |
| `OgaGenerator_TokenCount not found in onnxruntime-genai` | Version mismatch between Foundry Local components | Update Foundry Local: `winget upgrade Microsoft.AIToolkit` |
| `API version [N] is not available` | ONNX Runtime version too old for the Foundry Local service | Update Foundry Local: `winget upgrade Microsoft.AIToolkit` |

## License

Licensed under the MIT License.
