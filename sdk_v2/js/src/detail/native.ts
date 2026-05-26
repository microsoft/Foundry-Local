// Loads the native addon from prebuilds/<platform>-<arch>/foundry_local_node.node.
// In published packages this directory is populated by CI; in dev it is
// populated by `npm run build:native` (output target) and the C++ shared lib
// is dropped alongside by `npm run copy-native:dev`.
import { existsSync } from "node:fs";
import { createRequire } from "node:module";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";

// JSON-stringifiable shape of the addon's `Manager` class. The native side
// uses `Napi::ObjectWrap<Manager>` and exports a JS constructor.
export interface NativeManagerCtor {
  new (options: { appName: string; modelCacheDir?: string; externalServiceUrl?: string }): NativeManager;
}

export interface NativeManager {
  getWebServiceEndpoints(): string[];
  getCatalog(): NativeCatalog;
  dispose(): void;
  isDisposed(): boolean;
}

// Raw snapshot returned by the native side. All optional fields are omitted
// (rather than serialised as `null`) when the wrapper reports them as unset.
// `deviceType` is already mapped to the public string union on the native side.
export interface NativeModelInfo {
  id: string;
  name: string;
  version: number;
  alias: string;
  uri: string;
  deviceType: "CPU" | "GPU" | "NPU" | "Invalid";
  executionProvider?: string;
  displayName?: string;
  modelType?: string;
  publisher?: string;
  license?: string;
  licenseDescription?: string;
  task?: string;
  modelProvider?: string;
  minFlVersion?: string;
  parentUri?: string;
  supportsToolCalling?: boolean;
  filesizeMb?: number;
  maxOutputTokens?: number;
  createdAtUnix: number;
  isTestModel: boolean;
  contextLength?: number;
  inputModalities?: string;
  outputModalities?: string;
  capabilities?: string;
}

export interface NativeModel {
  getInfo(): NativeModelInfo;
  isCached(): boolean;
  isLoaded(): boolean;
  getPath(): string;
  getVariants(): NativeModel[];
  load(): Promise<void>;
  unload(): Promise<void>;
  download(): Promise<void>;
}

export interface NativeCatalog {
  getName(): string;
  getModels(): NativeModel[];
  getCachedModels(): NativeModel[];
  getLoadedModels(): NativeModel[];
  getModel(alias: string): NativeModel | undefined;
  getModelVariant(modelId: string): NativeModel | undefined;
  getLatestVersion(model: NativeModel): NativeModel | undefined;
}

// ── Inference surface ───────────────────────────────────────────────────────

/**
 * Plain-object snapshot of an inference response. The native layer fully
 * materialises this on the JS thread before resolving the Promise — there is
 * no native handle behind it.
 */
export interface NativeResponse {
  output: ReadonlyArray<unknown>; // `Item[]` once narrowed via the public surface
  finishReason: "none" | "stop" | "length" | "toolCalls" | "error";
  usage: { promptTokens: number; completionTokens: number; totalTokens: number };
}

export interface NativeRequestCtor {
  new (): NativeRequest;
}

export interface NativeRequest {
  addItem(item: unknown): void;
  setOptions(options: Record<string, string | number | boolean | undefined>): void;
  cancel(): void;
  getItemCount(): number;
  getItem(index: number): unknown;
}

export interface NativeItemQueueCtor {
  new (): NativeItemQueue;
}

export interface NativeItemQueue {
  push(item: unknown): void;
  tryPop(): unknown;
  size(): number;
  markFinished(): void;
  finished(): boolean;
  dispose(): void;
}

export interface NativeSession {
  processRequest(request: NativeRequest): Promise<NativeResponse>;
  processStreamingRequest(request: NativeRequest, onItem: (item: unknown) => void): Promise<void>;
  setOptions(options: Record<string, string | number | boolean | undefined>): void;
  dispose(): void;
  isDisposed(): boolean;
}

export interface NativeChatSession extends NativeSession {
  addToolDefinition(definition: { name: string; description: string; jsonSchema: string }): void;
  turnCount(): number;
  undoTurns(count: number): void;
}

// Embeddings sessions expose no methods beyond the base. The type alias
// exists so the addon export can be typed distinctly from chat.
export type NativeEmbeddingsSession = NativeSession;

// Audio sessions also expose no methods beyond the base — all inference
// (one-shot and streaming) flows through the inherited surface. Audio IS
// streamable (live transcription), unlike embeddings, but that's a
// behavioural difference at the native registration site, not a TS-shape
// difference.
export type NativeAudioSession = NativeSession;

export interface NativeAddon {
  Manager: NativeManagerCtor;
  // `Catalog` / `Model` constructors are exported for `instanceof` checks
  // only; direct construction (`new addon.X()`) throws.
  // Typed as `new (...) => unknown` rather than `Function` to satisfy Biome —
  // we never call these constructors from TS, we only read them.
  Catalog: new (
    ...args: unknown[]
  ) => unknown;
  Model: new (...args: unknown[]) => unknown;
  // `Request` IS directly constructible from JS — it's a stateful builder.
  Request: NativeRequestCtor;
  // `ItemQueue` is directly constructible — the public TS `ItemQueue` class
  // wraps it. The only `Item` subtype with a real native handle on the JS
  // side; all the others are plain JS objects copied across the boundary.
  ItemQueue: NativeItemQueueCtor;
  // Only modality-specific session classes (`ChatSession` today;
  // `AudioSession` / `EmbeddingsSession` later) are directly constructible
  // from JS. They take a JS Model instance (the native ObjectWrap<Model>,
  // i.e. the value stored in the `nativeByModel` weak map) and produce a
  // native session handle. The abstract base `Session` is a TS-only class
  // and has no JS-constructible native counterpart.
  ChatSession: new (
    model: NativeModel,
  ) => NativeChatSession;
  EmbeddingsSession: new (model: NativeModel) => NativeEmbeddingsSession;
  AudioSession: new (model: NativeModel) => NativeAudioSession;
}

const here = fileURLToPath(new URL(".", import.meta.url));
// dist/detail/native.js -> dist/ -> package root
const pkgRoot = resolve(here, "..", "..");
const prebuildDir = resolve(pkgRoot, "prebuilds", `${process.platform}-${process.arch}`);
const addonPath = resolve(prebuildDir, "foundry_local_node.node");

function loadAddon(): NativeAddon {
  if (!existsSync(addonPath)) {
    throw new Error(
      `Native addon not found at ${addonPath}.\nBuild it locally with:\n  npm run copy-native:dev && npm run build:native\n(requires the C++ SDK to be built first via\n \`python sdk_v2/cpp/build.py --configure --build --config RelWithDebInfo\`).`,
    );
  }
  const require = createRequire(import.meta.url);
  return require(addonPath) as NativeAddon;
}

let cached: NativeAddon | undefined;

export function getAddon(): NativeAddon {
  if (cached === undefined) {
    cached = loadAddon();
  }
  return cached;
}
