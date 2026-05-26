// Public read-only model handle for the v2 SDK.
//
// `Model` instances are NEVER constructed directly by user code — they are
// manufactured by `Catalog` methods (`catalog.getModel(...)`,
// `catalog.getModels(...)`, etc.) and by `model.getVariants(...)`. The
// underlying native handle is pinned to its parent `Manager` for lifetime
// purposes; once the `Manager` is disposed, any further calls on a `Model`
// obtained through it will reject with a `FoundryLocalError`.
//
// Read-only accessors (`getInfo`, `isCached`, `isLoaded`, `getPath`,
// `getVariants`) are synchronous — the underlying C ABI calls are accessor
// copies, not I/O. Mutating operations (`download`, `load`, `unload`) are
// async and dispatch to libuv workers. `removeFromCache`, `selectVariant`
// and `getInputOutputInfo` are not yet exposed.
//
// Design reference: mirrors `sdk_v2/python/src/foundry_local_sdk/imodel.py`
// (`IModel` + `_ModelImpl`) and `sdk_v2/cs/src/IModel.cs` / `Detail/Native/Model.cs`.
import type { NativeModel } from "./detail/native.js";

/**
 * Snapshot of model catalog metadata. Returned by `Model.getInfo()`.
 *
 * Optional fields (`?:`) are omitted when the native side reports them as
 * unset, rather than serialised as `null` — this matches the C++ wrapper's
 * `std::optional<>` semantics and the Python SDK's `Optional[...]` shape.
 *
 * Snapshot rather than live view: the underlying `ModelInfo` in the native
 * layer is a non-owning view into a `flModelInfo*` whose lifetime is the
 * parent `Model`. Returning a JS view would create a use-after-free hazard
 * when the `Model` is disposed; instead `getInfo()` eagerly snapshots into
 * a plain object whose lifetime is the JS GC's responsibility.
 */
export interface ModelInfo {
  readonly id: string;
  readonly name: string;
  readonly version: number;
  readonly alias: string;
  readonly uri: string;
  /**
   * Device type for this model variant. `"Invalid"` is surfaced for
   * `FOUNDRY_LOCAL_DEVICE_NOTSET` (unspecified) — do NOT assume CPU as a
   * default. Mirrors `sdk_v2/cs/src/FoundryModelInfo.cs` `DeviceType` (which
   * also surfaces `Invalid`); the Python SDK uses `None` for the same case.
   */
  readonly deviceType: "CPU" | "GPU" | "NPU" | "Invalid";
  readonly executionProvider?: string;
  readonly displayName?: string;
  readonly modelType?: string;
  readonly publisher?: string;
  readonly license?: string;
  readonly licenseDescription?: string;
  readonly task?: string;
  readonly modelProvider?: string;
  readonly minFlVersion?: string;
  readonly parentUri?: string;
  readonly supportsToolCalling?: boolean;
  readonly filesizeMb?: number;
  readonly maxOutputTokens?: number;
  readonly createdAtUnix: number;
  readonly isTestModel: boolean;
  readonly contextLength?: number;
  readonly inputModalities?: string;
  readonly outputModalities?: string;
  readonly capabilities?: string;
}

/**
 * Structural read-only interface satisfied by `Model`. Kept primarily as a
 * seam for the legacy v1-compat shim which expects an `IModel` type to exist;
 * the v2 surface uses `Model` directly. Mirrors the `IModel` /
 * concrete-`Model` split in the C++ wrapper and the C# / Python SDKs.
 */
export interface IModel {
  getInfo(): ModelInfo;
  isCached(): boolean;
  isLoaded(): boolean;
  getPath(): string;
  getVariants(): Model[];
  load(): Promise<void>;
  unload(): Promise<void>;
  download(): Promise<void>;
}

const internalCtorKey = Symbol("Model.internal");

// Module-internal map from JS `Model` to its native handle. Lets `catalog.ts`
// recover the native handle without exposing a public accessor on `Model`.
const nativeByModel = new WeakMap<Model, NativeModel>();

/**
 * Read-only handle to a model in the catalog.
 *
 * Obtain instances via `Manager.getCatalog()` and `Catalog` methods.
 * `new Model(...)` from user code throws.
 */
export class Model implements IModel {
  readonly #native: NativeModel;

  /** @internal — wraps a native Model handle. Do not call from user code. */
  constructor(token: typeof internalCtorKey, native: NativeModel) {
    if (token !== internalCtorKey) {
      throw new TypeError("Model is internal — obtain instances via Catalog/Manager methods");
    }
    this.#native = native;
    nativeByModel.set(this, native);
  }

  /**
   * Snapshot of the model's catalog metadata. Cheap; safe to call repeatedly.
   * The returned object is a plain JS object with no native backing — it
   * remains valid after the `Manager` is disposed.
   */
  getInfo(): ModelInfo {
    return this.#native.getInfo();
  }

  /** True when the model is present in the local model cache. */
  isCached(): boolean {
    return this.#native.isCached();
  }

  /** True when the model is currently loaded in memory. */
  isLoaded(): boolean {
    return this.#native.isLoaded();
  }

  /** Local filesystem path to the cached model directory. */
  getPath(): string {
    return this.#native.getPath();
  }

  /**
   * Load the model into memory. Async — runs the underlying
   * `IModel::Load()` on a libuv worker so the event loop is not blocked.
   * Rejects with a `FoundryLocalError` on failure.
   */
  async load(): Promise<void> {
    await this.#native.load();
  }

  /**
   * Unload the model. Async — runs `IModel::Unload()` off the main thread.
   */
  async unload(): Promise<void> {
    await this.#native.unload();
  }

  /**
   * Ensure the model's weights are present in the local cache. Downloads
   * them from the configured backend if not already cached.
   *
   * No progress callback is exposed yet — that will wire up on top of the
   * streaming `ThreadSafeFunction` bridge.
   */
  async download(): Promise<void> {
    await this.#native.download();
  }

  /** Variants of the model (one per device / execution-provider combo). */
  getVariants(): Model[] {
    const arr = this.#native.getVariants();
    return arr.map((n) => new Model(internalCtorKey, n));
  }
}

/** @internal — used by `Catalog` to wrap native handles into JS `Model`s. */
export function wrapNativeModel(native: NativeModel): Model {
  return new Model(internalCtorKey, native);
}

/** @internal — used by `Catalog` to forward the underlying native handle. */
export function unwrapNativeModel(model: Model): NativeModel {
  const native = nativeByModel.get(model);
  if (native === undefined) {
    throw new TypeError("Catalog: argument is not a valid Model handle");
  }
  return native;
}
