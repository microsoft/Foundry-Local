// `Model` wraps the native model handle directly. There is no JS-level Model/Variant split — the native catalog
// already returns the resolved variant.
//
// `Model` instances are NEVER constructed directly by user code — they come from `Catalog` methods or from
// `model.variants`. The underlying native handle is pinned to its parent `FoundryLocalManager`; once the manager
// is disposed, any further calls reject with a `FoundryLocalError`.

import type { NativeModel } from "./detail/native.js";
import type { IModel } from "./imodel.js";
import { AudioClient } from "./openai/audioClient.js";
import { ChatClient } from "./openai/chatClient.js";
import { EmbeddingClient } from "./openai/embeddingClient.js";
import type { ModelInfo } from "./types.js";

const internalCtorKey = Symbol("Model.internal");

const nativeByModel = new WeakMap<Model, NativeModel>();

export class Model implements IModel {
  readonly #native: NativeModel;
  readonly #info: ModelInfo;

  /** @internal — wraps a native Model handle. Do not call from user code. */
  constructor(token: typeof internalCtorKey, native: NativeModel) {
    if (token !== internalCtorKey) {
      throw new TypeError("Model is internal — obtain instances via Catalog/FoundryLocalManager methods");
    }
    this.#native = native;
    // Cache the snapshot eagerly. The underlying native getInfo() copies every call, so we avoid repeating that
    // work for the property getters below. Native variant selection re-wraps with a fresh JS Model, so the
    // snapshot can never go stale on this instance.
    this.#info = native.getInfo();
    nativeByModel.set(this, native);
  }

  get id(): string {
    return this.#info.id;
  }

  get alias(): string {
    return this.#info.alias;
  }

  get info(): ModelInfo {
    return this.#info;
  }

  get isCached(): boolean {
    return this.#native.isCached();
  }

  /**
   * Async accessor. The underlying native check is synchronous, but the API is `Promise<boolean>` for parity
   * with the C# / Python SDKs whose "is loaded" check actually performs a backend round-trip.
   */
  async isLoaded(): Promise<boolean> {
    return this.#native.isLoaded();
  }

  get path(): string {
    return this.#native.getPath();
  }

  get variants(): IModel[] {
    const arr = this.#native.getVariants();
    return arr.map((n) => new Model(internalCtorKey, n));
  }

  get contextLength(): number | null {
    return this.#info.contextLength ?? null;
  }

  get inputModalities(): string | null {
    return this.#info.inputModalities ?? null;
  }

  get outputModalities(): string | null {
    return this.#info.outputModalities ?? null;
  }

  get capabilities(): string | null {
    return this.#info.capabilities ?? null;
  }

  get supportsToolCalling(): boolean | null {
    return this.#info.supportsToolCalling ?? null;
  }

  async load(): Promise<void> {
    await this.#native.load();
  }

  async unload(): Promise<void> {
    await this.#native.unload();
  }

  async download(progressCallback?: (progress: number) => void): Promise<void> {
    await this.#native.download(progressCallback);
  }

  removeFromCache(): void {
    this.#native.removeFromCache();
  }

  selectVariant(variant: IModel): void {
    if (!(variant instanceof Model)) {
      throw new TypeError("Model.selectVariant: expected a Model instance");
    }
    const nativeVariant = nativeByModel.get(variant);
    if (nativeVariant === undefined) {
      throw new TypeError("Model.selectVariant: expected a Model instance");
    }
    this.#native.selectVariant(nativeVariant);
  }

  createChatClient(): ChatClient {
    return new ChatClient(this);
  }

  createAudioClient(): AudioClient {
    return new AudioClient(this);
  }

  createEmbeddingClient(): EmbeddingClient {
    return new EmbeddingClient(this);
  }
}

/** @internal — used by `Catalog` to wrap native handles into JS `Model`s. */
export function wrapNativeModel(native: NativeModel): Model {
  return new Model(internalCtorKey, native);
}

/** @internal — used by `Catalog` / `Session` to recover the native handle. */
export function unwrapNativeModel(model: Model): NativeModel {
  const native = nativeByModel.get(model);
  if (native === undefined) {
    throw new TypeError("Argument is not a valid Model handle");
  }
  return native;
}
