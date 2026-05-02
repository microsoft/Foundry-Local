# Foundry Local C++ SDK

The Foundry Local C++ SDK provides a C++17 interface for running AI models locally on your machine. Discover, download, load, and run inference — all without cloud dependencies.

## Features

- **Local-first AI** — Run models entirely on your machine with no cloud calls
- **Model catalog** — Browse and discover available models; check what's cached or loaded
- **Automatic model management** — Download, load, unload, and remove models from cache
- **Chat completions** — OpenAI-compatible chat API with both non-streaming and streaming responses
- **Audio transcription** — Transcribe audio files locally with streaming support
- **Tool calling** — Function/tool calling with multi-turn conversation support
- **Multi-variant models** — Models can have multiple variants (e.g., different quantizations) with automatic selection of the best cached variant
- **Embedded web service** — Start a local HTTP server for OpenAI-compatible API access
- **Configurable inference** — Control temperature, max tokens, top-k, top-p, frequency penalty, random seed, and more
- **Custom logging** — Implement the `ILogger` interface to route SDK log output to your application's logging system

## Prerequisites

- **C++17** compiler (MSVC or clang-cl on Windows)
- **CMake** 3.20+
- **vcpkg** (used for dependency management)
- **Windows 10+** (the SDK currently requires Windows APIs)

## Installation

Download the latest source archive from [GitHub Releases](https://github.com/microsoft/Foundry-Local/releases), extract it, and build from source:

```bash
# Extract the archive
unzip foundry-local-cpp-sdk-<version>.zip
cd foundry-local-cpp-sdk

# Ensure VCPKG_ROOT is set to your vcpkg installation (needed for build dependencies)
cmake --preset x64-release
cmake --build --preset x64-release
```

To use the built SDK in another project, install it and point CMake at the install prefix:

```bash
cmake --install out/build/x64-release --prefix /path/to/install
# Then in your consuming project:
# cmake -DCMAKE_PREFIX_PATH=/path/to/install ..
```

### Build dependencies

The following are resolved automatically by vcpkg during the build:

| Package | Purpose |
|---------|---------|
| `nlohmann-json` | JSON parsing and serialization |
| `wil` | Windows Implementation Libraries (RAII handles, error helpers) |
| `ms-gsl` | Microsoft GSL (`gsl::not_null`, `gsl::span`) |
| `gtest` | Google Test (unit and E2E tests only) |

### Runtime dependencies (automatic)

The SDK dynamically loads `Microsoft.AI.Foundry.Local.Core.dll` and ONNX Runtime libraries at runtime. These are **downloaded automatically from NuGet** during CMake configuration — no manual setup needed.

When you build the sample or E2E tests, the native libraries are copied next to the executable automatically. For your own project, use the provided convenience function:

```cmake
# After find_package(CppSdk) or FetchContent:
include(cmake/FoundryLocalNativeDeps.cmake)  # or found via CMAKE_MODULE_PATH
foundry_local_download_native_deps()          # downloads at configure time
foundry_local_copy_native_deps(your_app)      # copies next to your binary at build time
```

The download is skipped if the binaries are already present. In CI, set `FOUNDRY_NATIVE_OVERRIDE_DIR` to use pipeline-built binaries instead of downloading from NuGet.

To disable automatic download (e.g. if you manage the Core DLL yourself):

```bash
cmake -DFOUNDRY_DOWNLOAD_NATIVE_DEPS=OFF ...
```

### Updating the vcpkg baseline

`vcpkg-configuration.json` contains a `baseline` field — this is a commit hash from the [microsoft/vcpkg](https://github.com/microsoft/vcpkg) repo that pins the exact versions of all build dependencies (nlohmann-json, wil, ms-gsl, gtest). This ensures reproducible builds across machines and CI.

To update the baseline to the latest vcpkg release:

```bash
# Get the latest commit hash
git ls-remote https://github.com/microsoft/vcpkg.git HEAD

# Update vcpkg-configuration.json with the new hash
# Then rebuild to verify nothing breaks
```

## Quick Start

```cpp
#include "foundry_local.h"
#include <iostream>

using namespace foundry_local;

int main() {
    // 1. Initialize the manager
    Manager::Create({"MyApp"});
    auto& manager = Manager::Instance();

    // 2. Get a model from the catalog
    auto& catalog = manager.GetCatalog();
    auto* model = catalog.GetModel("phi-3.5-mini");

    // 3. Download (if needed) and load the model
    model->Download([](float pct) {
        std::cout << "\rDownloading: " << pct << "%" << std::flush;
    });
    model->Load();

    // 4. Create a chat client and run inference
    OpenAIChatClient chat(*model);

    ChatSettings settings;
    settings.temperature = 0.7f;
    settings.max_tokens = 256;

    auto response = chat.CompleteChat(
        {{"user", "Why is the sky blue?"}},
        settings);

    std::cout << response.choices[0].message->content << std::endl;

    // 5. Clean up
    model->Unload();
    Manager::Destroy();
    return 0;
}
```

## Usage

### Initialization

Create the singleton `Manager` with a `Configuration`. Call `Destroy()` when done:

```cpp
#include "foundry_local.h"
using namespace foundry_local;

// Minimal — just an app name
Manager::Create({"MyApp"});

// With custom logger
class MyLogger : public ILogger {
public:
    void Log(LogLevel level, std::string_view message) noexcept override {
        std::cerr << "[" << LogLevelToString(level) << "] " << message << "\n";
    }
};

MyLogger logger;
Manager::Create({"MyApp"}, &logger);

// Access the singleton anywhere
auto& manager = Manager::Instance();

// Check if initialized
if (Manager::IsInitialized()) { /* ... */ }

// Deterministic cleanup
Manager::Destroy();
```

### Configuration

The SDK is configured via `Configuration` when creating the manager:

```cpp
Configuration config("MyApp");
config.log_level = LogLevel::Information;
config.model_cache_dir = "/path/to/cache";
config.web = WebServiceConfig{.urls = "http://127.0.0.1:5000"};
config.additional_settings = {{"Bootstrap", "false"}};

Manager::Create(std::move(config));
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `app_name` | `std::string` | **(required)** | Your application name |
| `app_data_dir` | `optional<path>` | `~/.{app_name}` | Application data directory |
| `model_cache_dir` | `optional<path>` | `{app_data}/cache/models` | Where models are stored locally |
| `logs_dir` | `optional<path>` | `{app_data}/logs` | Log output directory |
| `log_level` | `LogLevel` | `Warning` | `Verbose`, `Debug`, `Information`, `Warning`, `Error`, `Fatal` |
| `web` | `optional<WebServiceConfig>` | `nullopt` | Web service configuration |
| `additional_settings` | `optional<unordered_map>` | `nullopt` | Extra key-value settings passed to Core |

### Browsing the Model Catalog

The `Catalog` lets you discover what models are available, which are already cached locally, and which are currently loaded in memory.

```cpp
auto& catalog = manager.GetCatalog();

// List all available models
auto models = catalog.ListModels();
for (const auto* model : models) {
    std::cout << model->GetAlias() << " (id: " << model->GetId() << ")"
              << " cached=" << (model->IsCached() ? "yes" : "no")
              << " loaded=" << (model->IsLoaded() ? "yes" : "no") << "\n";
}

// Look up a specific model by alias
auto* model = catalog.GetModel("phi-3.5-mini");

// Look up a specific variant by its unique model ID
auto* variant = catalog.GetModelVariant("phi-3.5-mini-generic-gpu-4");

// See what's already downloaded
auto cached = catalog.GetCachedModels();

// See what's currently loaded in memory
auto loaded = catalog.GetLoadedModels();
```

### Model Lifecycle

Each model may have multiple variants (different quantizations, hardware targets). The SDK auto-selects the best available variant, preferring cached versions.

```cpp
auto* model = catalog.GetModel("phi-3.5-mini");

// Inspect available variants
auto* concreteModel = dynamic_cast<Model*>(model);
for (const auto& variant : concreteModel->GetAllModelVariants()) {
    const auto& info = variant.GetInfo();
    std::cout << "  " << info.name << " v" << info.version
              << " cached=" << (variant.IsCached() ? "yes" : "no") << "\n";
}

// Select a specific variant
concreteModel->SelectVariant(concreteModel->GetAllModelVariants()[0]);
```

Download, load, and unload:

```cpp
// Download with progress reporting
model->Download([](float pct) {
    std::cout << "\r" << pct << "%" << std::flush;
});

// Load into memory
model->Load();

// Unload when done
model->Unload();

// Remove from local cache entirely
model->RemoveFromCache();
```

### Chat Completions

The `OpenAIChatClient` follows the OpenAI Chat Completion API structure.

```cpp
OpenAIChatClient chat(*model);

ChatSettings settings;
settings.temperature = 0.7f;
settings.max_tokens = 256;

auto response = chat.CompleteChat(
    {{"system", "You are a helpful assistant."},
     {"user", "Explain quantum computing in simple terms."}},
    settings);

std::cout << response.choices[0].message->content << "\n";
```

### Streaming Responses

For real-time token-by-token output, use streaming:

```cpp
std::cout << "Assistant: ";
chat.CompleteChatStreaming(
    {{"user", "Write a short poem about programming."}},
    settings,
    [](const ChatCompletionCreateResponse& chunk) {
        if (!chunk.choices.empty() && chunk.choices[0].delta &&
            !chunk.choices[0].delta->content.empty()) {
            std::cout << chunk.choices[0].delta->content << std::flush;
        }
    });
std::cout << "\n";
```

### Tool Calling

Define functions the model can call and handle the multi-turn conversation:

```cpp
// 1. Define tools
std::vector<ToolDefinition> tools = {
    {"function",
     FunctionDefinition{
         "multiply_numbers",
         "Multiply two integers and return the result.",
         PropertyDefinition{
             "object", std::nullopt,
             std::unordered_map<std::string, PropertyDefinition>{
                 {"first", PropertyDefinition{"integer", "The first number"}},
                 {"second", PropertyDefinition{"integer", "The second number"}}},
             std::vector<std::string>{"first", "second"}}}}};

// 2. Send request with tools
std::vector<ChatMessage> messages = {
    {"system", "You are a helpful AI assistant. Use the provided tools when appropriate."},
    {"user", "What is 7 multiplied by 6?"}};

ChatSettings settings;
settings.tool_choice = ToolChoiceKind::Required;

auto response = chat.CompleteChat(messages, tools, settings);

// 3. Check if the model wants to call a tool
const auto& choice = response.choices[0];
if (choice.finish_reason == FinishReason::ToolCalls &&
    choice.message && !choice.message->tool_calls.empty()) {
    const auto& tc = choice.message->tool_calls[0];

    // 4. Execute the tool locally (your application logic)
    std::string result = "42";

    // 5. Feed the tool result back
    messages.push_back({"assistant", "", std::nullopt, choice.message->tool_calls});
    messages.push_back({"tool", result, tc.id});

    settings.tool_choice = ToolChoiceKind::Auto;
    auto followUp = chat.CompleteChat(messages, tools, settings);
    std::cout << followUp.choices[0].message->content << "\n";
}
```

### Audio Transcription

Transcribe audio files locally using the `OpenAIAudioClient`:

```cpp
auto* model = catalog.GetModel("whisper-small");
model->Load();

OpenAIAudioClient audio(*model);

// Non-streaming transcription
auto result = audio.TranscribeAudio("recording.wav");
std::cout << "Transcription: " << result.text << "\n";

// Streaming transcription
audio.TranscribeAudioStreaming("recording.wav",
    [](const AudioCreateTranscriptionResponse& chunk) {
        std::cout << chunk.text << std::flush;
    });
```

### Embedded Web Service

Start a local HTTP server that exposes an OpenAI-compatible REST API:

```cpp
// Configure the web service URL in Configuration
Configuration config("MyApp");
config.web = WebServiceConfig{.urls = "http://127.0.0.1:5000"};
Manager::Create(std::move(config));

auto& manager = Manager::Instance();
manager.StartWebService();

auto urls = manager.GetUrls();
std::cout << "Service running at: " << urls[0] << "\n";

// Any OpenAI-compatible client can now connect to the endpoint.
// ...

manager.StopWebService();
```

### Chat Settings Reference

| Field | Type | Description |
|-------|------|-------------|
| `frequency_penalty` | `optional<float>` | Frequency penalty |
| `max_tokens` | `optional<int>` | Maximum number of tokens to generate |
| `n` | `optional<int>` | Number of completions to generate |
| `temperature` | `optional<float>` | Sampling temperature (0.0–2.0; higher = more random) |
| `presence_penalty` | `optional<float>` | Presence penalty |
| `random_seed` | `optional<int>` | Random seed for reproducible results |
| `top_k` | `optional<int>` | Top-k sampling parameter |
| `top_p` | `optional<float>` | Nucleus sampling probability (0.0–1.0) |
| `tool_choice` | `optional<ToolChoiceKind>` | Tool selection strategy (`Auto`, `None`, `Required`) |

## API Reference

### Core Classes

| Class | Header | Description |
|-------|--------|-------------|
| `Manager` | `foundry_local_manager.h` | Singleton entry point — initialization, catalog access, web service |
| `Configuration` | `configuration.h` | Initialization settings (app name, cache dir, log level, web service) |
| `Catalog` | `catalog.h` | Model discovery — listing, lookup by alias/ID, cached/loaded queries |
| `IModel` | `model.h` | Abstract interface for models — identity, metadata, lifecycle |
| `Model` | `model.h` | Alias-level model with variant selection (implements `IModel`) |
| `ModelVariant` | `model.h` | Specific model variant with full metadata (implements `IModel`) |
| `ILogger` | `logger.h` | Logging interface — implement to receive SDK log output |

### OpenAI Clients

| Class | Header | Description |
|-------|--------|-------------|
| `OpenAIChatClient` | `openai/openai_chat_client.h` | Chat completions (non-streaming and streaming) with tool calling |
| `OpenAIAudioClient` | `openai/openai_audio_client.h` | Audio transcription (non-streaming and streaming) |

### Types

| Type | Header | Description |
|------|--------|-------------|
| `ChatMessage` | `openai/openai_chat_client.h` | A message in a chat conversation (role + content + optional tool calls) |
| `ChatSettings` | `openai/openai_chat_client.h` | Generation settings (temperature, max_tokens, etc.) |
| `ChatCompletionCreateResponse` | `openai/openai_chat_client.h` | Response from a chat completion request |
| `ToolDefinition` | `openai/openai_tool_types.h` | Describes a tool the model can call |
| `ToolCall` | `openai/openai_tool_types.h` | A tool call returned by the model |
| `AudioCreateTranscriptionResponse` | `openai/openai_audio_client.h` | Response from an audio transcription request |
| `ModelInfo` | `model.h` | Full metadata for a model variant |

## Project Structure

The SDK uses the standard C++ `include/` vs `src/` separation:

```
sdk/cpp/
├── include/                    # Public headers (shipped to consumers)
│   ├── foundry_local.h         # Umbrella header
│   ├── configuration.h
│   ├── catalog.h
│   ├── model.h
│   ├── foundry_local_manager.h
│   ├── foundry_local_exception.h
│   ├── logger.h
│   ├── log_level.h
│   └── openai/
│       ├── openai_chat_client.h
│       ├── openai_audio_client.h
│       └── openai_tool_types.h
├── src/                        # Internal implementation (not shipped)
│   ├── core.h                  # Native DLL loader (LoadLibraryW)
│   ├── flcore_native.h         # C ABI struct definitions
│   ├── core_helpers.h          # Call wrappers
│   ├── core_interop_request.h  # JSON request builder
│   ├── parser.h                # JSON parsing utilities
│   └── *.cpp                   # Implementation files
├── sample/                     # Sample application
│   └── main.cpp
├── test/                       # Unit and E2E tests (GoogleTest)
│   ├── *_test.cpp
│   ├── e2e_test.cpp
│   ├── mock_core.h
│   └── testdata/
├── cmake/                      # CMake package config template
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
└── vcpkg-configuration.json
```

## Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| Windows x64 | ✅ | Full support (MSVC, clang-cl) |
| Windows ARM64 | 🔜 | Planned |
| Linux x64 | 🔜 | Planned (requires cross-platform refactor) |
| macOS ARM64 | 🔜 | Planned (requires cross-platform refactor) |

## Building and Running Tests

```bash
# Configure with tests enabled (default)
cmake --preset x64-debug

# Build everything (library, sample, tests)
cmake --build --preset x64-debug

# Run unit tests (no Core DLL needed)
ctest --preset x64-debug

# Run E2E tests (requires Core DLL next to test binary)
cd out/build/x64-debug
ctest --output-on-failure
```

E2E tests require the Foundry Local Core DLL to be placed alongside the test binary. Tests that require model downloads are `DISABLED_` by default; run them locally with `--gtest_also_run_disabled_tests`.

## License

Microsoft Software License Terms — see [LICENSE](../../LICENSE) for details.
