# Foundry Local JavaScript SDK (v2) — Plan

This document captures the architectural plan and decisions for the new
JavaScript / TypeScript SDK under `sdk_v2/js/`. The new SDK ports the legacy
`sdk/js/` package onto the C++ SDK's stable C ABI, while preserving the legacy
user-facing API surface.

The agent that owns implementation is [`JsCoder`](../../../.github/agents/JsCoder.agent.md).
Architecture decisions are owned by [`DearLeader`](../../../.github/agents/DearLeader.agent.md).
The canonical C ABI is [`foundry_local_c.h`](../../cpp/include/foundry_local/foundry_local_c.h)
and the C++ wrapper is [`foundry_local_cpp.h`](../../cpp/include/foundry_local/foundry_local_cpp.h).

---

## Goals

1. Provide a native Node.js binding for Foundry Local that uses the same C ABI
   the C# and Python SDKs use.
2. Preserve the **public TypeScript API** of `sdk/js/` so existing consumers
   recompile against `foundry-local-sdk@2.x` without source changes.
3. Eliminate the legacy `.NET`-side command-dispatch ABI and the JS-side
   `CoreInterop` shim.
4. Add a new typed surface (`Session` / `ChatSession` / `AudioSession` /
   `EmbeddingsSession`, `Request`, `Response`, `Item` hierarchy) that mirrors
   the C# and Python v2 SDKs.
5. Stay small. The legacy package was a Node-API C addon for a reason —
   `koffi` and `ffi-napi` cost tens of MB at install time, which is
   unacceptable for an embedded SDK.

## Non-goals

- Browser support. This SDK loads a native binary; it is Node-only.
- Polyfilling sync wrappers on top of async I/O. Sync entry points are real
  native sync calls (see [Sync vs. async](#sync-vs-async)).
- A new HTTP transport. The OpenAI Responses / Chat Completions client remains
  pure TypeScript talking HTTP to the embedded web service.

---

## Architecture

Five composing layers, top-down:

```
┌───────────────────────────────────────────────────────────────────┐
│  5. Legacy public surface (preserved)                             │
│     FoundryLocalManager, Catalog, ChatClient, ResponsesClient,    │
│     AudioClient, EmbeddingClient, LiveAudioTranscriptionSession,  │
│     ModelLoadManager, IModel, Model, ModelVariant, Configuration  │
├───────────────────────────────────────────────────────────────────┤
│  4. New v2 public surface                                         │
│     Session, ChatSession, AudioSession, EmbeddingsSession,        │
│     Request, Response, Item hierarchy, Items namespace            │
├───────────────────────────────────────────────────────────────────┤
│  3. TypeScript detail layer                                       │
│     Typed handle classes that own native pointers, AsyncIterable  │
│     stream adapters, AbortSignal plumbing, error mapping          │
├───────────────────────────────────────────────────────────────────┤
│  2. node-addon-api C++ addon                                      │
│     Napi::ObjectWrap<T> over std::unique_ptr<foundry_local::X>,   │
│     ThreadSafeFunction streaming bridge, AsyncWorker async paths  │
├───────────────────────────────────────────────────────────────────┤
│  1. foundry_local C++ wrapper (foundry_local_cpp.h)               │
│     RAII handles, exception-based error handling, typed accessors │
│  0. foundry_local C ABI (foundry_local_c.h)                       │
│     Versioned vtable, opaque handles, status returns              │
└───────────────────────────────────────────────────────────────────┘
```

### Why layer on the C++ wrapper instead of going straight to the C ABI

- The wrapper already solves RAII, error → exception mapping, and typed
  content accessors. Bypassing it forces every addon source file to redo
  that work, and it diverges from how the C# and Python SDKs are organised.
- The wrapper has no third-party dependencies, no ABI surface of its own,
  and is header-only. The addon links against the same `foundry_local`
  native library every other SDK loads.
- If the addon needs something the wrapper does not expose, the answer is
  to extend the wrapper (via `@ApiExpert`), not to drop down to the C ABI.

### Native addon language: C++17 + node-addon-api

- `node-addon-api` is the Node-maintained C++ wrapper over N-API. It is
  header-only, version-stable across Node majors, and adds roughly
  50–150 KB to the addon binary. Compared to the legacy raw-C addon, it
  removes ~3–4× the boilerplate around handle wrapping, type conversion,
  and async work.
- The legacy `foundry_local_napi.c` is **not** a structural template — it
  binds to a different (now-dead) ABI. Specific bug-fixes worth carrying
  forward are called out inline in JsCoder's agent file.
- C++17 is the minimum standard. `NAPI_VERSION=8`,
  `NAPI_DISABLE_CPP_EXCEPTIONS=0` (we use C++ exceptions internally and
  translate at the JS boundary).

---

## Sync vs. async

Node's main thread runs JavaScript on a single event loop. A sync JS call
cannot `await` an async operation — doing so would deadlock. The legacy SDK
ran `executeCommand` as a real native sync call, blocking the JS thread until
the .NET side returned. The new SDK does the same.

| Entry point | Native binding strategy |
|---|---|
| Async (default) | Wrap the underlying `foundry_local::*` C++ call in a `Napi::AsyncWorker`, dispatch on libuv worker pool, resolve a Promise on completion. |
| Sync (legacy parity) | Call the **same** underlying C++ method inline on the JS thread. Blocks until complete. Exposed only for entries that exist in `sdk/js/` today. |

Both entries reuse the same C++ function; the addon decides where it runs.
There is no JS-layer `Atomics.wait`, `deasync`, or worker-thread trampoline.

---

## Streaming

- New `Session` family exposes `AsyncIterable<Item>`. Each native streaming
  callback push lands on a `Napi::ThreadSafeFunction` acquired in the
  session's constructor and released in its destructor.
- Legacy clients (`ChatClient`, `LiveAudioTranscriptionSession`) keep their
  callback / EventEmitter shapes verbatim — the iterable is bridged to a
  callback at the TS legacy-compat layer.
- Cancellation: each async API accepts an `AbortSignal`. The signal is
  bound to a `std::atomic<bool>` captured by the C++ streaming callback,
  which returns non-zero to the C ABI when fired. `Request::Cancel()` is
  the wrapper-level primitive.

---

## API surface

### New (v2) — primary

Mirrors the C# and Python v2 SDKs:

- `Session`, `ChatSession`, `AudioSession`, `EmbeddingsSession`
- `Request`, `Response`
- `Item` base, plus `Items.{TextItem, MessageItem, ImageItem, AudioItem, BytesItem, TensorItem, ToolCallItem, ToolResultItem, ItemQueueItem}`
- `Manager` (new typed handle), `Catalog`, `Model`, `ModelInfo`, `ModelList`
- `Configuration` builder, `KeyValuePairs`
- Strong typing for `ToolDefinition`, `SearchOptions`, `FinishReason`, `Usage`

### Legacy (preserved) — back-compat shims

Reimplemented on top of the new layer; same exported names, signatures, and
runtime semantics as `sdk/js/`:

- `FoundryLocalManager`, `Catalog`, `ChatClient`, `ResponsesClient`,
  `AudioClient`, `EmbeddingClient`, `LiveAudioTranscriptionSession`,
  `ModelLoadManager`, `IModel`, `Model`, `Configuration`, `ModelVariant`,
  `getOutputText`

### Removed

- `CoreInterop` (was `@internal` — never part of the public contract).
- The `executeCommand` / `executeCommandAsync` / `executeCommandWithBinary`
  / `executeCommandStreaming` plumbing in the addon. Replaced by typed
  N-API entries per operation.

---

## Package & distribution

- Single npm package: `foundry-local-sdk@2.0.0`. Supersedes the legacy
  `1.x` line under the same name. Hard cut at the major version.
- Node 20+. ESM-only (no CommonJS dual build).
- Native addon: built with `node-gyp` against `binding.gyp`.
- **All prebuilds are bundled in the published npm tarball.** The
  central CI pipeline (`.pipelines/sdk_v2/`) already builds the C++ SDK
  and the addon for every (platform × arch). Before `npm publish`, CI
  drops each `(.node addon + foundry_local.{dll,so,dylib})` pair into
  `prebuilds/<platform>-<arch>/` inside the package directory. The
  tarball published to the npm registry contains all variants. At
  install time, `npm install` just unpacks the tarball — there is **no
  postinstall download step**, no separate artifact host, and no
  network access beyond the normal npm fetch. At runtime, the JS
  loader picks the matching `prebuilds/<process.platform>-<process.arch>/`
  subdirectory. If a consumer is on an unsupported platform, the addon
  load fails with a clear error; there is no automatic source-build
  fallback in the published package. Source builds are a **dev-only**
  path (`node-gyp rebuild` + `script/copy-native.mjs`), not an
  install-time fallback.
- **Dev / source builds load the native from the canonical C++ build
  dir.** Per
  [cpp-build.instructions.md](../../../.github/instructions/cpp-build.instructions.md),
  `python sdk_v2/cpp/build.py --configure --build --config RelWithDebInfo`
  is the contract; the addon is built locally and a dev-time helper copies
  `sdk_v2/cpp/build/<Windows|Linux|macOS>/<Config>/bin/<Config>/[lib]foundry_local.{dll,so,dylib}`
  into `sdk_v2/js/prebuilds/<platform>-<arch>/` next to the `.node`
  addon. At runtime the addon does a fixed sibling-file
  `LoadLibrary`/`dlopen` — no path discovery, no `DllLoader`-equivalent.
  The C# `Detail/DllLoader.cs` is not a reference here — it solves a
  NuGet runtime-assets layout problem that doesn't generalize to npm.
- **ONNX Runtime / ORT-GenAI discovery is inherited from the legacy
  setup.** `foundry_local` depends on `onnxruntime` and
  `onnxruntime-genai`; locating those at runtime is the user's
  responsibility today (via `Configuration.libraryPath`, env vars, and
  the Linux `RTLD_DEEPBIND` pre-load workaround for `libcrypto` /
  `libssl` from the legacy `foundry_local_napi.c`). That contract carries
  over verbatim — same env vars, same `libraryPath` field, same
  `RTLD_DEEPBIND` shim in the new addon's module-init code. See
  [ort-loading-contract.instructions.md](../../../.github/instructions/ort-loading-contract.instructions.md).

---

## Repository layout

```
sdk_v2/js/
├── package.json
├── tsconfig.json
├── tsconfig.build.json
├── biome.json                  # lint + format (single tool)
├── vitest.config.ts            # test runner + coverage
├── binding.gyp                 # node-gyp build for the C++ addon
├── docs/
│   └── plan.md                 # this file
├── native/
│   └── src/                    # C++ addon (node-addon-api)
│       ├── addon.cc            # NAPI_MODULE entry, exports
│       ├── handles/            # Napi::ObjectWrap<T> classes
│       ├── items/              # Item subclass bindings
│       ├── manager.cc
│       ├── catalog.cc
│       ├── request.cc
│       ├── response.cc
│       ├── session.cc
│       ├── streaming.cc        # ThreadSafeFunction bridge
│       └── errors.cc           # foundry_local::Error → Napi::Error
├── src/
│   ├── index.ts                # public exports (v2 + legacy)
│   ├── detail/                 # typed handle wrappers, internals
│   ├── items/                  # Item TS classes
│   ├── session.ts
│   ├── chatSession.ts
│   ├── audioSession.ts
│   ├── embeddingsSession.ts
│   ├── request.ts
│   ├── response.ts
│   ├── manager.ts
│   ├── catalog.ts
│   ├── model.ts
│   ├── configuration.ts
│   ├── types.ts
│   ├── openai/                 # HTTP-only clients (chat, responses, audio, embeddings)
│   └── legacy/                 # preserved v1 surface re-implemented on v2
└── script/
    ├── copy-native.mjs         # dev: copy foundry_local + ORT/GenAI siblings into prebuilds/<plat>-<arch>/
    └── pack-prebuilds.mjs      # CI: stage foundry_local only into prebuilds/<plat>-<arch>/ before npm publish
```

---

## Test strategy

Follows the **testing-trophy** model:

- **Foundation:** TypeScript strict mode + Biome catch most surface bugs
  without runtime cost.
- **Unit (thin):** Pure TS helpers (URL parsing, content encoders) covered
  with Vitest. No native calls.
- **Integration (heaviest layer):** Drive the real addon against a real
  Foundry Local native library and the small test models in
  `sdk_v2/testdata/`. Mocks only at true external boundaries (network for
  the HTTP `ResponsesClient` tests). No mocking of internal collaborators.
- **End-to-end (small):** A handful of scenarios from the legacy test
  suite that exercise full client flows (`FoundryLocalManager` →
  `ChatClient.completeChat` → text out).

Test stack pinned:

- **Runner / assertions:** `vitest` + `@vitest/coverage-v8`
- **Lint + format:** `biome` (single tool — no ESLint, no Prettier)
- **No** Mocha, Chai-standalone, Jest, ts-node, tsx, or
  `@types/node`-driven `assert` helpers.

---

## Gap analysis vs. the C++ wrapper

Performed up-front against [`foundry_local_cpp.h`](../../cpp/include/foundry_local/foundry_local_cpp.h)
and the reference C# / Python SDKs. Result:

- **Zero** C ABI gaps.
- **Zero** wrapper gaps requiring API additions.
- The "is web service running?" capability is satisfied by the existing
  `Manager::GetWebServiceEndpoints()` returning an empty vector when the
  service is not running (header comment updated to document this).
- `LiveAudioTranscriptionSession`'s push-PCM-while-streaming-results
  pattern is built on existing primitives (`AudioSession` +
  `ItemQueue` of audio items + streaming callback emitting result items),
  matching the C# and Python implementations.
- Legacy `Configuration.libraryPath` is a JS-side native-loader concern,
  not a wrapper gap.

---

## Execution sequence

No separate up-front analyst pass is needed. The legacy JS SDK and the
legacy C# SDK both spoke the same .NET command-dispatch protocol, and
the C# v2 SDK in `sdk_v2/cs/src/` is the worked example of how each of
those commands is re-expressed against `foundry_local_c.h` /
`foundry_local_cpp.h`. The Python v2 SDK in
`sdk_v2/python/src/foundry_local_sdk/` is a second reference. `@JsCoder`
reads the answer key directly from those two SDKs as it implements each
layer.

**Implementation is phased.** Each phase is reviewed by `@DearLeader`
before the next is dispatched. No phase invocation should attempt to
land more than one phase of work; agent invocations are one-shot and
small reviewable diffs are the goal.

### Phase 1 — Scaffolding + Manager vertical slice

Owner: `@JsCoder`.

Goal: prove the toolchain end-to-end with the smallest possible surface
area, before any sessions, streaming, or items work begins.

1. Validate `foundry_local_cpp.h` compiles in a `node-addon-api`
   translation unit under MSVC `/EHsc` with C++17 (`NAPI_VERSION=8`,
   `NAPI_DISABLE_CPP_EXCEPTIONS=0`). If it requires C++20 or pulls in
   headers that don't play well with node-addon-api, stop and report
   back — do not work around it.
2. Package scaffolding:
   - `package.json` (name `foundry-local-sdk`, version `2.0.0-dev.0`,
     `engines.node` >=20, `type: module`, scripts for `build:native`,
     `build:ts`, `build`, `test`, `lint`, `format`,
     `copy-native:dev`).
   - `tsconfig.json` + `tsconfig.build.json` (strict, ESM, Node20 lib).
   - `vitest.config.ts`, `biome.json`, `.gitignore`, `binding.gyp`.
   - `src/index.ts` with the full v2 + legacy export shape stubbed
     (types only; methods throw `not implemented` where applicable).
3. Native addon skeleton (`native/src/`):
   - `addon.cc` with NAPI module init.
   - `errors.cc` — `foundry_local::Error` → `Napi::Error` mapping.
   - `manager.cc` — `Napi::ObjectWrap<Manager>` over
     `std::unique_ptr<foundry_local::Manager>`, with sync + async
     entries for **only**: ctor, `getCacheLocation`, `getCatalog`,
     `getWebServiceEndpoints`. (Catalog returned as a typed handle if
     `getCatalog` is exposed in Phase 1, or deferred to Phase 2 — JsCoder
     decides based on minimal-vertical-slice cost.)
4. TS detail + public layer for `Manager` only:
   - `src/detail/native.ts` — addon loader, error normalization.
   - `src/manager.ts` — public `Manager` class with both sync and async
     methods, mirroring the C# v2 `Manager` shape.
   - Re-export from `src/index.ts`.
5. Dev-build wiring:
   - `script/copy-native.mjs` — copies
     `sdk_v2/cpp/build/<Platform>/<Config>/bin/<Config>/foundry_local.{dll,so,dylib}`
     into `prebuilds/<platform>-<arch>/`. Config defaults to
     `RelWithDebInfo`. Errors clearly if the source isn't present and
     points at `python sdk_v2/cpp/build.py --configure --build`.
   - `node-gyp` configured to drop the `.node` addon next to the copied
     native lib.
6. Tests (Vitest, integration only — no mocks of the native):
   - `Manager` construction, `getCacheLocation`, `getWebServiceEndpoints`
     (asserting empty when service is not running, per the documented
     contract).
   - Run only when a built native is present; skip with a clear message
     otherwise.

**Out of scope for Phase 1:** sessions of any kind, `Request`/`Response`,
items, streaming, `ChatClient` and other legacy HTTP clients, the legacy
`FoundryLocalManager` compat shim, CI prebuild-packing integration.

### Phase 2 — Sessions, Request/Response, Items, streaming bridge

Owner: `@JsCoder`. Dispatched only after Phase 1 review.

Adds the v2 typed session surface (`Session`, `ChatSession`,
`EmbeddingsSession`, `AudioSession`), the `Request` / `Response` types,
the `Item` hierarchy, and the `Napi::ThreadSafeFunction` streaming
bridge. Test models from `sdk_v2/testdata/` come online here.

### Phase 3 — Legacy compatibility surface

Owner: `@JsCoder`.

Re-implements `FoundryLocalManager`, `ChatClient`, `ResponsesClient`,
`AudioClient`, `EmbeddingClient`, `LiveAudioTranscriptionSession`,
`ModelLoadManager`, `IModel`/`Model`/`ModelVariant`,
`Configuration`, `getOutputText` on top of the v2 layer. Behavioural
parity with `sdk/js/`.

### Phase 4 — CI prebuild packaging

Owner: `@JsCoder`.

`script/pack-prebuilds.mjs` plus a `.pipelines/sdk_v2/` job that, for
each (platform × arch), builds the addon, drops `(.node, foundry_local
shared lib)` into `prebuilds/<platform>-<arch>/`, then `npm pack`s a
single tarball containing all variants. No `postinstall` script — `npm
install` just unpacks the tarball.

### On-demand support

- **`@PortCSharpToCpp`** — consulted only for specific
  C#-pattern-to-C++-idiom questions that arise during implementation
  (e.g. how a particular `IAsyncEnumerable` pattern maps to the
  wrapper's streaming callback).
- **`@Tester`** — cross-SDK behavioural-parity audit against the C# and
  Python v2 test suites once Phase 3 lands.

---

## Open issues

None blocking. Followups to consider after the initial drop lands:

- `.github/instructions/js-build.instructions.md` (applyTo
  `sdk_v2/js/**`) — pin npm scripts, prebuild paths, "never edit
  `prebuilds/` by hand".
- `.github/instructions/js-napi-addon.instructions.md` (applyTo
  `sdk_v2/js/native/**`) — TSFN-from-worker-threads rules, buffer-pinning
  contract for any agent touching the addon.
- A sibling `@JsReviewer` agent that mirrors `@Reviewer` for the JS / TS
  surface, if review volume justifies it.
