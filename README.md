<div align="center">
  <picture align="center">
    <source media="(prefers-color-scheme: dark)" srcset="media/icons/ai_studio_icon_white.svg">
    <source media="(prefers-color-scheme: light)" srcset="media/icons/ai_studio_icon_black.svg">
    <img alt="AI Foundry icon." src="media/icons/ai_studio_icon_black.svg" height="100" style="max-width: 100%;">
  </picture>
<div id="user-content-toc">
  <ul align="center" style="list-style: none;">
    <summary>
      <h1>Foundry Local</h1><br>
     <h3><a href="https://aka.ms/foundry-local-installer">Download</a> | <a href="https://aka.ms/foundry-local-docs">Documentation</a> | <a href="https://aka.ms/foundry-local-discord">Discord</a></h3>
    </summary>
  </ul>
</div>
</div>

## 👋 Welcome to Foundry Local

Foundry Local brings the power of Azure AI Foundry to your local device **without requiring an Azure subscription**. It allows you to:

- Run Generative AI models directly on your local hardware - no sign-up required.
- Keep all data processing on-device for enhanced privacy and security
- Integrate models with your applications through an OpenAI-compatible API
- Optimize performance using ONNX Runtime and hardware acceleration

## 🚀 Quickstart

1. **Install Foundry Local:**
    - **Windows**: Open a terminal and run the following command:
        ```bash
        winget install Microsoft.FoundryLocal
        ```
        For any issues, refer to the [Installation section](https://github.com/microsoft/Foundry-Local#installing) below.
    - **MacOS**: Open a terminal and run the following command:
        ```bash
        brew tap microsoft/foundrylocal
        brew install foundrylocal
        ```
    Alternatively, you can download the installer from the [releases page](https://github.com/microsoft/Foundry-Local/releases) and follow the on-screen installation instructions.

2. **Run your first model**: Open a terminal and run the following command to run a model:

   ```bash
   foundry model run phi-3.5-mini
   ```

> [!NOTE]
> The `foundry model run <model>` command will automatically download the model if it's not already cached on your local machine, and then start an interactive chat session with the model.

Foundry Local will automatically select and download a model *variant* with the best performance for your hardware. For example:

- if you have an Nvidia CUDA GPU, it will download the CUDA-optimized model.
- if you have a Qualcomm NPU, it will download the NPU-optimized model.
- if you don't have a GPU or NPU, Foundry local will download the CPU-optimized model.

### 🔍 Explore available models

You can list all available models by running the following command:

```bash
foundry model ls
```

This will show you a list of all models that can be run locally, including their names, sizes, and other details.

## 🧑‍💻 Integrate with your applications using the SDK

Foundry Local has an easy-to-use SDK (Python, JavaScript) to get you started with existing applications:

### Python

The Python SDK is available as a package on PyPI. You can install it using pip:

```bash
pip install foundry-local-sdk
pip install openai
```

> [!TIP]
> We recommend using a virtual environment such as `conda` or `venv` to avoid conflicts with other packages.


Foundry Local provides an OpenAI-compatible API that you can call from any application:

```python
import openai
from foundry_local import FoundryLocalManager

# By using an alias, the most suitable model will be downloaded 
# to your end-user's device.
alias = "phi-3.5-mini"

# Create a FoundryLocalManager instance. This will start the Foundry 
# Local service if it is not already running and load the specified model.
manager = FoundryLocalManager(alias)

# The remaining code us es the OpenAI Python SDK to interact with the local model.

# Configure the client to use the local Foundry service
client = openai.OpenAI(
    base_url=manager.endpoint,
    api_key=manager.api_key  # API key is not required for local usage
)

# Set the model to use and generate a streaming response
stream = client.chat.completions.create(
    model=manager.get_model_info(alias).id,
    messages=[{"role": "user", "content": "What is the golden ratio?"}],
    stream=True
)

# Print the streaming response
for chunk in stream:
    if chunk.choices[0].delta.content is not None:
        print(chunk.choices[0].delta.content, end="", flush=True)
```

### JavaScript

The JavaScript SDK is available as a package on npm. You can install it using npm:

```bash
npm install foundry-local-sdk
npm install openai
```

```javascript
import { OpenAI } from "openai";
import { FoundryLocalManager } from "foundry-local-sdk";

// By using an alias, the most suitable model will be downloaded 
// to your end-user's device.
// TIP: You can find a list of available models by running the 
// following command in your terminal: `foundry model list`.
const alias = "phi-3.5-mini";

// Create a FoundryLocalManager instance. This will start the Foundry 
// Local service if it is not already running.
const foundryLocalManager = new FoundryLocalManager()

// Initialize the manager with a model. This will download the model 
// if it is not already present on the user's device.
const modelInfo = await foundryLocalManager.init(alias)
console.log("Model Info:", modelInfo)

const openai = new OpenAI({
  baseURL: foundryLocalManager.endpoint,
  apiKey: foundryLocalManager.apiKey,
});

async function streamCompletion() {
    const stream = await openai.chat.completions.create({
      model: modelInfo.id,
      messages: [{ role: "user", content: "What is the golden ratio?" }],
      stream: true,
    });
  
    for await (const chunk of stream) {
      if (chunk.choices[0]?.delta?.content) {
        process.stdout.write(chunk.choices[0].delta.content);
      }
    }
}
  
streamCompletion();
```

## Installing

### Windows

Install Foundry Local using `winget install Microsoft.FoundryLocal` in a Windows console (PowerShell, cmd, etc.)

You can also manually download and install the packages. On [the releases page](https://github.com/microsoft/Foundry-Local/releases)
select a release and expand the Artifacts list. Copy the artifact full URI (like `https://github.com/microsoft/Foundry-Local/releases/download/v0.3.9267/FoundryLocal-x64-0.3.9267.43123.msix`)
to use in the below PowerShell steps. (Replace `x64` with `arm64` as needed.)

```powershell
# Download the package and its dependency
$releaseUri = "https://github.com/microsoft/Foundry-Local/releases/download/v0.3.9267/FoundryLocal-x64-0.3.9267.43123.msix"
Invoke-WebRequest -Method Get -Uri $releaseUri -OutFile .\FoundryLocal.msix
$crtUri = "https://aka.ms/Microsoft.VCLibs.x64.14.00.Desktop.appx"
Invoke-WebRequest -Method Get -Uri $crtUri -OutFile .\VcLibs.appx

# Install the Foundry Local package
Add-AppxPackage .\FoundryLocal.msix -DependencyPath .\VcLibs.appx
```

If you're having problems installing Foundry, please [file an issue](https://github.com/microsoft/foundry-local/issues)
and include logs using one of these methods:

* For WinGet - use `winget install Microsoft.FoundryLocal --logs --verbose` - select the most-recently-dated log file
  and attach it to the issue.
* For `Add-AppxPackage` - immediately after it indicates an error, in an elevated PowerShell instance, use
  `Get-MsixLogs | Out-File MsixLogs.txt` and attach it to the issue.
* Use [Windows Feedback Hub](feedback-hub:) and create a Problem in the "Apps > All other apps" category. Use the
  "Add More Details > Recreate my problem" and re-run the failing commands to collect more data. Once your feedback
  is submitted, use the "Share" option to generate a link and put that into the filed issue.

> [!NOTE]
> Log files may contain information like user names, IP addresses, file paths, etc. Be sure to remove those
> before sharing here.

## Features & Use Cases

- **On-device inference** - Process sensitive data locally for privacy, reduced latency, and no cloud costs
- **OpenAI-compatible API** - Seamlessly integrate with applications using familiar SDKs
- **High performance** - Optimized execution with ONNX Runtime and hardware acceleration
- **Flexible deployment** - Ideal for edge computing scenarios with limited connectivity
- **Development friendly** - Perfect for prototyping AI features before production deployment
- **Model versatility** - Use pre-compiled models or [convert your own](./docs/how-to/compile-models-for-foundry-local.md).

## Reporting Issues

We're actively looking for feedback during this preview phase. Please report issues or suggest improvements in the [GitHub Issues](https://github.com/microsoft/Foundry-Local/issues) section.

## 🎓 Learn

- [Detailed documentation](./docs/README.md)
- [CLI reference](./docs/reference/reference-cli.md)
- [REST API reference](./docs/reference/reference-rest.md)
- [Security and privacy](./docs/reference/reference-security-privacy.md)
- [Troubleshooting guide](./docs/reference/reference-troubleshooting.md)

## ⚖️ License

Foundry Local is licensed under the Microsoft Software License Terms. For more details, read the [LICENSE](LICENSE) file.
