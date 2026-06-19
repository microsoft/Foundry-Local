# 🚀 Foundry Local JavaScript Samples

These samples demonstrate how to use the Foundry Local JavaScript SDK (`foundry-local-sdk`) with Node.js.

The samples consume the SDK **directly from local source** (`../../sdk/js` via a `file:` dependency),
so they always track `main` rather than a published npm version.

## Prerequisites

- [Node.js](https://nodejs.org/) (v18 or later recommended)

## Samples

| Sample | Description |
|--------|-------------|
| [chat-completion](chat-completion/) | Run the same chat prompt two ways: native in-process inference (streaming and non-streaming) **and** the local OpenAI-compatible web server (`/v1/chat/completions`). |
| [embeddings](embeddings/) | Generate single and batch text embeddings using native inference. |
| [audio](audio/) | Live microphone streaming (Nemotron ASR) **and** file-based transcription (`--file <path>`, Whisper) in one app. |
| [responses-api](responses-api/) | Stream a vision (image understanding) response from the local web server using the Responses API. |

## Running a Sample

1. Clone the repository:

   ```bash
   git clone https://github.com/microsoft/Foundry-Local.git
   cd Foundry-Local/samples/js
   ```

1. Navigate to a sample and install dependencies:

   ```bash
   cd chat-completion
   npm install
   ```

1. Run the sample:

   ```bash
   npm start
   ```

> [!TIP]
> Each sample's `package.json` references the SDK via `"foundry-local-sdk": "file:../../../sdk/js"`,
> so `npm install` builds against the in-repo SDK. The SDK ships a prebuilt `dist/` and downloads its
> native runtime on install. If you've changed the SDK source, rebuild it first with
> `npm install && npm run build` (and `npm run build:native` to rebuild the native addon) inside
> `sdk/js`.
