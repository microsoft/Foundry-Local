# Foundry Local C++ SDK

A C++17 static library for running local AI models via [Foundry Local](https://learn.microsoft.com/windows/ai/foundry-local).
Provides OpenAI-compatible chat completion, audio transcription, and tool-calling APIs — all running on-device.

> **Windows-only** — requires MSVC or clang-cl (MSVC-compatible toolchain).

## Prerequisites

| Requirement | Notes |
|---|---|
| **Foundry Local / AI Toolkit** | Install via `winget install Microsoft.AIToolkit` or the VS Code AI Toolkit extension |
| **CMake >= 3.20** | Ships with Visual Studio 2022 |
| **Ninja** | Ships with Visual Studio 2022 |
| **vcpkg** | Set the `VCPKG_ROOT` environment variable to your vcpkg installation |
| **MSVC** (or clang-cl) | Visual Studio 2022 Build Tools or full IDE |

## Quick Start

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

### 4. Run the sample

Copy the provided onnxruntime.dll, onnxruntime-genai.dll, and Microsoft.AI.Foundry.Local.Core.dll to the output folder (e.g. `out/build/x64-debug`) before running:

```bash
.\out\build\x64-debug\CppSdkSample.exe
```

### 5. Run unit tests

```bash
ctest --preset x64-debug
```

Or run the test executable directly:

```bash
.\out\build\x64-debug\CppSdkTests.exe
```

## Release Build

```bash
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release
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

## Usage

### Minimal example

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

### Streaming chat

```cpp
chat.CompleteChatStreaming(messages, settings, [](const ChatCompletionCreateResponse& chunk) {
    if (!chunk.choices.empty() && chunk.choices[0].delta) {
        std::cout << chunk.choices[0].delta->content << std::flush;
    }
});
```

### Selecting a CPU variant

Models may have multiple variants (CPU, GPU, NPU). To explicitly select a CPU variant:

```cpp
auto* model = catalog.GetModel("phi-3.5-mini");
if (auto* concrete = dynamic_cast<Model*>(model)) {
    for (const auto& variant : concrete->GetAllModelVariants()) {
        if (variant.GetInfo().runtime &&
            variant.GetInfo().runtime->device_type == DeviceType::CPU) {
            concrete->SelectVariant(variant);
            break;
        }
    }
}
```

### Audio transcription

```cpp
OpenAIAudioClient audio(*model);
auto result = audio.TranscribeAudio(R"(C:\path\to\audio.wav)");
std::cout << result.text << "\n";
```

### Tool calling

See `sample/main.cpp` (Example 5) for a full tool-calling walkthrough.

## Troubleshooting

| Error | Cause | Fix |
|---|---|---|
| `DML provider requested, but GenAI has not been built with DML support` | GPU variant selected but ONNX Runtime GenAI lacks DML | Select a CPU variant or update Foundry Local |
| `OgaGenerator_TokenCount not found in onnxruntime-genai` | Version mismatch between Foundry Local components | Update Foundry Local: `winget upgrade Microsoft.AIToolkit` |
| `API version [N] is not available` | ONNX Runtime version too old for the Foundry Local service | Update Foundry Local: `winget upgrade Microsoft.AIToolkit` |

## License

Licensed under the MIT License.
