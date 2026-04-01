<div align="center">
  <picture align="center">
    <source media="(prefers-color-scheme: dark)" srcset="media/icons/foundry_local_white.svg">
    <source media="(prefers-color-scheme: light)" srcset="media/icons/foundry_local_black.svg">
    <img alt="Foundry Local icon." src="media/icons/foundry_local_black.svg" height="100" style="max-width: 100%;">
  </picture>
<div id="user-content-toc">
  <ul align="center" style="list-style: none;">
    <summary>
      <h1>Foundry Local</h1><br>
     <h3><a href="https://aka.ms/foundry-local-installer">Download</a> | <a href="https://aka.ms/foundry-local-docs">Documentation</a> | <a href="https://aka.ms/foundry-local-discord">Discord</a></h3>
    </summary>
  </ul>
</div>

## Ship on-device AI inside your app

</div>

Foundry Local is an **end-to-end local AI solution** for building applications that run entirely on the user's device. It provides native SDKs (C#, JavaScript, Python, and Rust), a curated catalog of optimized models, and automatic hardware acceleration — all in a lightweight package (~20 MB).

User data never leaves the device, responses start immediately with zero network latency, and your app works offline. No per-token costs, no API keys, no backend infrastructure to maintain, and no Azure subscription required.

### Key Features

- **Native SDKs** — Embed AI directly in your app with C#, JavaScript, Python, and Rust SDKs. No separate server process needed.
- **Chat AND Audio in one runtime** — Text generation and speech-to-text (Whisper) through a single SDK.
- **Curated model catalog** — Production-ready models (Phi, Qwen, DeepSeek, Mistral, Whisper) optimized for on-device use across consumer hardware.
- **Automatic hardware acceleration** — GPU and NPU when available, with seamless CPU fallback. Zero hardware detection code needed.
- **Smart model management** — Models download on first use, cache locally, and auto-select the best variant for the user's hardware.
- **OpenAI-compatible API** — Drop-in compatible with OpenAI SDKs for minimal code changes.
- **Small footprint** — Powered by [ONNX Runtime](https://onnxruntime.ai/), a high-performance inference engine with minimal disk and memory requirements.
- **Multi-platform** — Windows, macOS (Apple silicon), and Linux.

### Supported Tasks

| Task | Model Aliases | API |
|------|--------------|-----|
| Chat / Text Generation | `phi-3.5-mini`, `qwen2.5-0.5b`, `qwen2.5-coder-0.5b`, etc. | Chat Completions |
| Audio Transcription (Speech-to-Text) | `whisper-tiny` | Audio Transcription |

## 🚀 Quickstart

The fastest way to get started is with the SDK. Pick your language:

<details open>
<summary><strong>JavaScript</strong></summary>

1. Install the SDK:

    ```bash
    npm install foundry-local-sdk
    ```

2. Run your first chat completion:

    ```javascript
    import { FoundryLocalManager } from 'foundry-local-sdk';

    const manager = FoundryLocalManager.create({ appName: 'my-app' });

    // Download and load a model (auto-selects best variant for user's hardware)
    const model = await manager.catalog.getModel('qwen2.5-0.5b');
    await model.download((progress) => {
        process.stdout.write(`\rDownloading... ${progress.toFixed(2)}%`);
    });
    await model.load();

    // Create a chat client and get a completion
    const chatClient = model.createChatClient();
    const response = await chatClient.completeChat([
        { role: 'user', content: 'What is the golden ratio?' }
    ]);

    console.log(response.choices[0]?.message?.content);

    // Unload the model when done
    await model.unload();
    ```

> [!NOTE]
> On Windows, NPU models are not currently available for the JavaScript SDK. These will be enabled in a subsequent release.

</details>

<details>
<summary><strong>C#</strong></summary>

1. Install the SDK:

    ```bash
    # Windows (recommended for hardware acceleration)
    dotnet add package Microsoft.AI.Foundry.Local.WinML

    # macOS/Linux
    dotnet add package Microsoft.AI.Foundry.Local
    ```

2. Run your first chat completion:

    ```csharp
    using Microsoft.AI.Foundry.Local;

    var config = new Configuration { AppName = "my-app" };
    await FoundryLocalManager.CreateAsync(config);
    var mgr = FoundryLocalManager.Instance;

    // Download and load a model (auto-selects best variant for user's hardware)
    var catalog = await mgr.GetCatalogAsync();
    var model = await catalog.GetModelAsync("qwen2.5-0.5b");
    await model.DownloadAsync();
    await model.LoadAsync();

    // Create a chat client and get a streaming completion
    var chatClient = await model.GetChatClientAsync();
    var messages = new List<ChatMessage>
    {
        new() { Role = "user", Content = "What is the golden ratio?" }
    };

    await foreach (var chunk in chatClient.CompleteChatStreamingAsync(messages))
    {
        Console.Write(chunk.Choices[0].Message.Content);
    }

    // Unload the model when done
    await model.Unload();
    ```

</details>

<details>
<summary><strong>Python</strong></summary>

> **Note:** The Python SDK currently uses the Foundry Local CLI and the OpenAI-compatible REST API. A native in-process SDK (matching JS/C#) is coming soon.

1. Install the SDK:

    ```bash
    pip install foundry-local-sdk openai
    ```

2. Run your first chat completion:

    ```python
    import openai
    from foundry_local import FoundryLocalManager

    # Initialize manager (starts local service and loads model)
    manager = FoundryLocalManager("phi-3.5-mini")

    # Use the OpenAI SDK pointed at your local endpoint
    client = openai.OpenAI(base_url=manager.endpoint, api_key=manager.api_key)

    response = client.chat.completions.create(
        model=manager.get_model_info("phi-3.5-mini").id,
        messages=[{"role": "user", "content": "What is the golden ratio?"}]
    )

    print(response.choices[0].message.content)
    ```

</details>

> [!TIP]
> For the JavaScript and C# SDKs, you do **not** need the CLI installed. The Python SDK currently requires the CLI — a native in-process SDK is coming soon.

### Audio Transcription (Speech-to-Text)

The SDK also supports audio transcription via Whisper models:

```javascript
import { FoundryLocalManager } from 'foundry-local-sdk';

const manager = FoundryLocalManager.create({ appName: 'my-app' });

const whisperModel = await manager.catalog.getModel('whisper-tiny');
await whisperModel.download();
await whisperModel.load();

const audioClient = whisperModel.createAudioClient();
audioClient.settings.language = 'en';

// Transcribe an audio file
const result = await audioClient.transcribe('recording.wav');
console.log('Transcription:', result.text);

// Or stream in real-time
for await (const chunk of audioClient.transcribeStreaming('recording.wav')) {
    process.stdout.write(chunk.text);
}

await whisperModel.unload();
```

> [!TIP]
> A single `FoundryLocalManager` can manage both chat and audio models simultaneously. See the [chat-and-audio sample](samples/js/chat-and-audio-foundry-local/) for a complete example.

## 📦 Samples

Explore complete working examples in the [`samples/`](samples/) folder:

| Language | Samples | Highlights |
|----------|---------|------------|
| [**C#**](samples/cs/) | 12 | Native chat, audio transcription, tool calling, model management, web server, tutorials |
| [**JavaScript**](samples/js/) | 12 | Native chat, audio, Electron app, Copilot SDK, LangChain, tool calling, tutorials |
| [**Python**](samples/python/) | 9 | Chat completions, audio transcription, LangChain, tool calling, tutorials |
| [**Rust**](samples/rust/) | 8 | Native chat, audio transcription, tool calling, web server, tutorials |

## 🖥️ CLI

The Foundry Local CLI lets you explore models and experiment interactively.

**Install:**

```bash
# Windows
winget install Microsoft.FoundryLocal

# macOS
brew install microsoft/foundrylocal/foundrylocal
```

**Run a model:**

```bash
foundry model run qwen2.5-0.5b
```

**List available models:**

```bash
foundry model ls
```

> For the full CLI reference and advanced usage, see the [CLI documentation on Microsoft Learn](https://learn.microsoft.com/en-us/azure/foundry-local/reference/reference-cli).

## 📥 Installing

Foundry Local is available for Windows, macOS (Apple silicon), and Linux.

### Windows

```bash
winget install Microsoft.FoundryLocal
```

<details>
<summary>Manual installation</summary>

On [the releases page](https://github.com/microsoft/Foundry-Local/releases), select a release and expand the Artifacts list. Copy the artifact URI and use the following PowerShell steps:

```powershell
$releaseUri = "https://github.com/microsoft/Foundry-Local/releases/download/v0.3.9267/FoundryLocal-x64-0.3.9267.43123.msix"
Invoke-WebRequest -Method Get -Uri $releaseUri -OutFile .\FoundryLocal.msix
$crtUri = "https://aka.ms/Microsoft.VCLibs.x64.14.00.Desktop.appx"
Invoke-WebRequest -Method Get -Uri $crtUri -OutFile .\VcLibs.appx

Add-AppxPackage .\FoundryLocal.msix -DependencyPath .\VcLibs.appx
```

Replace `x64` with `arm64` as needed.

</details>

### macOS

```bash
brew install microsoft/foundrylocal/foundrylocal
```

<details>
<summary>Manual installation</summary>

1. Download the latest release from [the releases page](https://github.com/microsoft/Foundry-Local/releases).
2. Unzip the downloaded file.
3. Run the installer:

   ```bash
   ./install-foundry.command
   ```

</details>

### Upgrading

```bash
# Windows
winget upgrade --id Microsoft.FoundryLocal

# macOS (Homebrew)
brew upgrade foundrylocal
```

### Uninstalling

<details>
<summary>Uninstall instructions</summary>

**Windows:**

```bash
winget uninstall Microsoft.FoundryLocal
```

Or navigate to **Settings > Apps > Apps & features**, find "Foundry Local", and select **Uninstall**.

**macOS (Homebrew):**

```bash
brew rm foundrylocal
brew untap microsoft/foundrylocal
brew cleanup --scrub
```

**macOS (manual install):**

```bash
uninstall-foundry
```

</details>

> [!TIP]
> For installation troubleshooting, see the [troubleshooting guide](https://learn.microsoft.com/azure/ai-foundry/foundry-local/reference/reference-best-practice?view=foundry-classic) or [file an issue](https://github.com/microsoft/foundry-local/issues).

## Reporting Issues

We're actively looking for feedback during this preview phase. Please report issues or suggest improvements in the [GitHub Issues](https://github.com/microsoft/Foundry-Local/issues) section.

## 🎓 Learn More

- [Foundry Local Documentation](https://learn.microsoft.com/en-us/azure/foundry-local/) on Microsoft Learn
- [What is Foundry Local?](https://learn.microsoft.com/en-us/azure/foundry-local/what-is-foundry-local) — Architecture and concepts
- [Tutorials](https://learn.microsoft.com/en-us/azure/foundry-local/) — Chat assistant, document summarizer, tool calling, voice-to-text
- [Troubleshooting guide](https://learn.microsoft.com/azure/ai-foundry/foundry-local/reference/reference-best-practice?view=foundry-classic)
- [Foundry Local Lab](https://github.com/Microsoft-foundry/foundry-local-lab) — Hands-on exercises and step-by-step instructions

## ⚖️ License

Foundry Local is licensed under the Microsoft Software License Terms. For more details, read the [LICENSE](LICENSE) file.
