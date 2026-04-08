# Foundry Local Playground — JavaScript

> **[JavaScript](.)** · [Python](../python/README.md) · [C#](../cs/README.md) · [Rust](../rust/README.md)

An interactive CLI sample that demonstrates the full Foundry Local JavaScript SDK — from hardware discovery to streaming inference.

## Prerequisites

- [Node.js](https://nodejs.org/) v20 or later

## Setup

```bash
cd js
npm install foundry-local-sdk-winml@1.0.0-rc5
npm pkg set type=module
```

> **Note:** The `package.json` pins `foundry-local-sdk-winml` to `1.0.0-rc5` for Windows with hardware acceleration. On macOS/Linux, switch to `foundry-local-sdk` and pin RC5 as well:
> ```bash
> npm install foundry-local-sdk@1.0.0-rc5
> npm pkg set type=module
> ```
> Then update the import in `app.js`:
> ```js
> import { FoundryLocalManager } from 'foundry-local-sdk';
> ```

## Run

```bash
node app.js
```

## What Happens

1. **Execution providers** are discovered and downloaded with live progress bars.
2. A **model catalog** table is displayed — enter a number to pick a model.
3. The model is **downloaded** (if needed) and **loaded** into memory.
4. Depending on the model type:
   - **Chat models** → interactive conversation with streaming token output in bordered boxes.
   - **Whisper models** → audio transcription — enter a `.wav`/`.mp3` file path and see the transcript stream in.
5. Type `/quit` to exit.

## File Overview

| File | Purpose |
|---|---|
| `app.js` | Main SDK flow — initialize, discover EPs, browse catalog, load model, run inference |
| `ui.js` | Terminal UI helpers — progress bars, box-drawing tables, streaming output boxes |

## Key SDK APIs Used

```js
import { FoundryLocalManager } from 'foundry-local-sdk';

const manager = FoundryLocalManager.create({ appName: '...' });

// Execution providers
manager.discoverEps();
await manager.downloadAndRegisterEps(names, progressCallback);

// Model catalog
const models = await manager.catalog.getModels();
const model  = await manager.catalog.getModel(alias);
model.selectVariant(variant);

// Download & load
await model.download(progressCallback);
await model.load();

// Chat inference (streaming)
const client = model.createChatClient();
for await (const chunk of client.completeStreamingChat(messages)) { ... }

// Audio transcription (streaming)
const audio = model.createAudioClient();
for await (const chunk of audio.transcribeStreaming(filePath)) { ... }

// Cleanup
await model.unload();
```

## Screenshot

```
────────────────────────────────────────────────────────────────────
  Execution Providers
────────────────────────────────────────────────────────────────────
  ┌──────────────────────┬───────────────────────────────────────┐
  │ EP Name              │ Status                                │
  ├──────────────────────┼───────────────────────────────────────┤
  │ QNNExecutionProvider │ ● registered                         │
  │ CUDAExecutionProvider│ █████████████████████████████░  96.3% │
  └──────────────────────┴───────────────────────────────────────┘
```
