# Live Audio Transcription Demo

Real-time microphone-to-text using Foundry Local SDK, Core, and onnxruntime-genai.

## Architecture

```
Microphone (NAudio, 16kHz/16-bit/mono)
    |
    v
Foundry Local SDK (C#)
    | AppendAsync(pcmBytes)
    v
Foundry Local Core (NativeAOT DLL)
    | AppendAudioChunk -> CommitTranscription
    v
onnxruntime-genai (StreamingProcessor + Generator)
    | RNNT encoder + decoder
    v
Live transcription text
```

## Prerequisites

1. **Windows x64** with a microphone
2. **.NET 9.0 SDK** installed
3. **Nemotron ASR model** downloaded locally
4. **Native DLLs** (4 files — see Setup below)

## Setup (Step by Step)

### Step 1: Get the native DLLs

You need 4 DLLs placed in this project folder:

| DLL | Source |
|-----|--------|
| `Microsoft.AI.Foundry.Local.Core.dll` | Built from neutron-server (`dotnet publish` with NativeAOT) |
| `onnxruntime-genai.dll` | Built from onnxruntime-genai (Nenad's StreamingProcessor branch) |
| `onnxruntime.dll` | Comes with the Core publish output |
| `onnxruntime_providers_shared.dll` | Comes with the Core publish output |

**Option A: From CI artifacts**
- Download the Core DLL from the neutron-server CI pipeline artifacts
- Download the GenAI native DLLs from the onnxruntime-genai pipeline artifacts

**Option B: From a teammate**
- Ask for the 4 DLLs from someone who has already built them

Copy all 4 DLLs to this folder (`samples/cs/LiveAudioTranscription/`).

### Step 2: Get the Nemotron model

The model should be in a folder with this structure:
```
models/
  nemotron/
    genai_config.json
    encoder.onnx
    decoder.onnx
    joint.onnx
    tokenizer.json
    vocab.txt
```

### Step 3: Build

```powershell
cd samples/cs/LiveAudioTranscription
dotnet build -c Debug
```

### Step 4: Copy native DLLs to output (if not auto-copied)

```powershell
Copy-Item onnxruntime-genai.dll bin\Debug\net9.0\win-x64\ -Force
Copy-Item onnxruntime.dll bin\Debug\net9.0\win-x64\ -Force
Copy-Item onnxruntime_providers_shared.dll bin\Debug\net9.0\win-x64\ -Force
Copy-Item Microsoft.AI.Foundry.Local.Core.dll bin\Debug\net9.0\win-x64\ -Force
```

### Step 5: Run

```powershell
# Default model cache location
dotnet run -c Debug --no-build

# Or specify model cache directory
dotnet run -c Debug --no-build -- C:\path\to\models
```

### Step 6: Speak!

- The app will show `LIVE TRANSCRIPTION ACTIVE`
- Speak into your microphone
- Text appears in **cyan** as you speak
- Press **ENTER** to stop

## Expected Output

```
===========================================================
   Foundry Local -- Live Audio Transcription Demo
===========================================================

[1/5] Initializing Foundry Local SDK...
       SDK initialized.
[2/5] Loading nemotron model...
       Found model: nemotron
       Model loaded.
[3/5] Creating live transcription session...
       Session started (SDK -> Core -> GenAI pipeline active).
[4/5] Setting up microphone...

===========================================================
  LIVE TRANSCRIPTION ACTIVE
  Speak into your microphone.
  Transcription appears in real-time (cyan text).
  Press ENTER to stop recording.
===========================================================

Hello this is a demo of live audio transcription running entirely on device
  [FINAL] Hello this is a demo of live audio transcription running entirely on device

  Recording: 15.2s | 152 chunks | 475 KB

[5/5] Stopping session...

===========================================================
  Demo complete!
  Pipeline: Mic -> NAudio -> SDK -> Core -> GenAI -> Text
===========================================================
```

## Troubleshooting

| Error | Fix |
|-------|-----|
| `Core DLL not found` | Copy `Microsoft.AI.Foundry.Local.Core.dll` to project folder |
| `nemotron not found in catalog` | Check `ModelCacheDir` points to folder containing `nemotron/` with `genai_config.json` |
| `OgaStreamingProcessor not found` | The `onnxruntime-genai.dll` is old — rebuild from Nenad's branch or get from CI |
| `No microphone` | Ensure a mic is connected and set as default recording device |
| `num_mels unknown` | Fix `genai_config.json` — ASR params must be at model level, not nested under `speech` |
