# Foundry Local JavaScript samples

These seven samples use the v2 `foundry-local-sdk` APIs from `sdk_v2/js`. They run on Node.js 20 or later and
use the SDK directly from this repository through a local `file:` dependency.

| Sample | What it demonstrates |
| --- | --- |
| [hello-world](hello-world/) | Discover execution providers, select/download/load a model, and run native `ChatSession` inference. |
| [realtime-transcription](realtime-transcription/) | Stream raw PCM audio through `ItemQueue` to a native `AudioSession`. |
| [embeddings](embeddings/) | Generate native text embeddings with `EmbeddingsSession`. |
| [tool-calling](tool-calling/) | Register a native chat tool, execute a returned `ToolCallItem`, and submit a `ToolResultItem`. |
| [web-server](web-server/) | Start the built-in server, discover its URL, and call its OpenAI-compatible endpoint. |
| [vision](vision/) | Send text and a local image to a native `ChatSession`. |
| [byom](byom/) | Register existing model assets in the local catalog and run native chat. |

> [!IMPORTANT]
> These samples track the latest JavaScript SDK on `main` through a local `file:` dependency and require Foundry Local
> to be built before they can run. Installing
> [`foundry-local-sdk` from npm](https://www.npmjs.com/package/foundry-local-sdk) is easier, but these samples are not
> built or tested against a specific published version. See the
> [get-started documentation](https://learn.microsoft.com/en-us/azure/foundry-local/get-started) for versioned setup
> guidance and additional examples.

## Prerequisites

- Node.js 20 or later
- Native Foundry Local prerequisites for your platform
- Network access for execution-provider and model downloads when they are not already cached

Build the native SDK and JavaScript binding once before running a sample:

```bash
cd sdk_v2/cpp
python build.py --configure --build --config RelWithDebInfo
cd ../js
npm install
npm run build
```

Then install and run any sample:

```bash
cd samples/js/hello-world
npm install
npm start
```

Samples that accept an optional model alias document it in their own README. The tool-calling sample intentionally
uses `qwen2.5-0.5b`, whose current catalog variants publish the metadata required by structured tool calling. The
non-web samples invoke the native in-process session APIs; only `web-server` starts the built-in HTTP service.
