# Foundry Local — Sample Quick Start Guide

This guide covers how to run the **Python SDK** and **C++ SDK** samples from this branch of the Foundry Local repository. Two scenarios are covered for each SDK:

- **Text chat** — pure text-to-text inference (e.g., Q&A, jokes)
- **Vision** — image input via the Responses API

## Clone the Repository

```bash
git clone --branch Wayne-Ch/external-delivery-2026-05 https://github.com/microsoft/Foundry-Local.git
cd Foundry-Local
```

This branch is a frozen snapshot intended for sharing. It will not change after delivery.

---

## Python SDK — Text Chat Sample (Responses API)

### Prerequisites

- Python 3.10+
- pip

### Setup & Run

```bash
cd samples/python/web-server-responses
python -m venv .venv
.\.venv\Scripts\activate
pip install -r requirements.txt
python src\app.py qwen3.5-0.8b
```

On macOS/Linux, activate with `source .venv/bin/activate`.

### What it does

- Starts the Foundry Local web service
- Sends a text prompt via the Responses API
- Streams the model's response token-by-token

---

## Python SDK — Vision Sample (Responses API)

### Prerequisites

- Python 3.10+
- pip

### Setup & Run

```bash
cd samples/python/web-server-responses-vision
python -m venv .venv
.\.venv\Scripts\activate
pip install -r requirements.txt
python src\app.py qwen3.5-0.8b
```

### What it does

- Starts the Foundry Local web service
- Loads and resizes the default test image (`src/test_image.jpg`)
- Sends a vision request via the Responses API
- Streams the model's description of the image

---

## C++ SDK — Text Chat Sample (`CppSdkSample`)

This is the simplest C++ entry point — it uses the C++ SDK directly (no web service, no Responses API). It demonstrates catalog browsing, non-streaming chat, streaming chat, and tool calling.

### Prerequisites

- Visual Studio 2022 (MSVC, CMake, Ninja)
- vcpkg (`VCPKG_ROOT` environment variable set)
- NuGet CLI (`winget install Microsoft.NuGet`)

### Setup & Run

Open an **x64 Native Tools Command Prompt for VS 2022**, then:

```bash
cd sdk/cpp

# Configure (auto-downloads vcpkg deps + NuGet runtime DLLs)
cmake --preset x64-debug

# Build the text chat sample
cmake --build --preset x64-debug --target CppSdkSample

# Run
.\out\build\x64-debug\CppSdkSample.exe qwen3.5-0.8b
```

### What it does

- Discovers and downloads execution providers
- Browses the model catalog
- Runs non-streaming chat ("What is the capital of Croatia?")
- Runs streaming chat (streams a paragraph token-by-token)
- Demonstrates tool calling (model invokes a `multiply_numbers` function)

---

## C++ SDK — Vision Sample (Responses API)

### Prerequisites

Same as the C++ Text Chat sample above.

### Setup & Run

In the same x64 Native Tools Command Prompt:

```bash
cd sdk/cpp

# Configure (skip if already done from the text chat sample)
cmake --preset x64-debug

# Build the vision sample
cmake --build --preset x64-debug --target WebServerResponsesVision

# Run
.\out\build\x64-debug\WebServerResponsesVision.exe qwen3.5-0.8b
```

### What it does

- Discovers and downloads execution providers (e.g., WebGPU, CUDA)
- Starts the Foundry Local web service
- Loads, resizes, and base64-encodes the default test image (`test_image.jpg`)
- Sends a vision request via the Responses API using cURL
- Streams the model's response token-by-token

### Troubleshooting

| Issue | Fix |
|---|---|
| `nuget.exe not found` | `winget install Microsoft.NuGet` |
| NuGet/DLL copy errors | Delete `out` folder and reconfigure: `rmdir /s /q out && cmake --preset x64-debug` |
| `VCPKG_ROOT` not set | `set VCPKG_ROOT=C:\vcpkg` (or wherever vcpkg is installed) |
| Not in x64 developer prompt | Open "x64 Native Tools Command Prompt for VS 2022" from Start Menu |

---

## Notes

- All samples download the model on first run (skipped if already cached)
- Vision samples use a default test image; text samples use a built-in prompt
- The C++ vision sample mirrors the Python vision sample's structure and flow

## Finding the Best Region for Model Downloads

To find the optimal region/cluster for downloading models, run:

```bash
curl -s -D - -o /dev/null -X POST "https://api.catalog.azureml.ms/asset-gallery/v1.0/models" \
   -H "Content-Type: application/json" \
   -d '{"filters":[], "pageSize":1}' | grep -i "azureml-served-by-cluster"
```

On Windows (PowerShell):

```powershell
$response = Invoke-WebRequest -Uri "https://api.catalog.azureml.ms/asset-gallery/v1.0/models" `
    -Method POST -ContentType "application/json" -Body '{"filters":[], "pageSize":1}'
$response.Headers["azureml-served-by-cluster"]
```

This returns the cluster name serving your region, which can help diagnose slow model downloads.

### Setting the Model Registry Region

Once you know your optimal region, configure the SDK to use it:

**Python sample** — update `Configuration` in `src/app.py`:

```python
config = Configuration(
    app_name="foundry_local_samples",
    additional_settings={"ModelRegistryRegion": "eastus"},
)
```

**C++ sample** — update `Configuration` in `main.cpp`:

```cpp
foundry_local::Configuration config("foundry_local_samples");
config.additional_settings = std::unordered_map<std::string, std::string>{
    {"ModelRegistryRegion", "eastus"}
};
```

Replace `"eastus"` with the region closest to the cluster returned by the command above.
