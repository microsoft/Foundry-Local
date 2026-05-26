// Public read-only catalog handle for the v2 SDK.
//
// `Catalog` instances are NEVER constructed directly by user code — they are
// returned from `Manager.getCatalog()`. The underlying native catalog pointer
// is owned by the parent `Manager`; the JS wrapper holds a reference to keep
// the Manager alive while the Catalog is reachable.
//
// All catalog operations are synchronous — the underlying C ABI calls are
// either vector copies or in-memory lookups, none of which perform I/O.
// Real-work async lives on `Model` (download / load) and `Session` (inference).
//
// Design reference: mirrors `sdk_v2/python/src/foundry_local_sdk/catalog.py`
// (`Catalog`) and `sdk_v2/cs/src/Catalog.cs` / `ICatalog.cs`.
import type { NativeCatalog, NativeModel } from "./detail/native.js";
import { type Model, unwrapNativeModel, wrapNativeModel } from "./model.js";

const internalCtorKey = Symbol("Catalog.internal");

/**
 * Read-only handle to the model catalog. Obtain via `Manager.getCatalog()`.
 *
 * The `Catalog` is bound to its parent `Manager` — disposing the `Manager`
 * makes all subsequent `Catalog` calls fail with a `FoundryLocalError`.
 *
 * Catalog scope is read-only model discovery. Mutating operations
 * (`download`, `load`, `unload`, `removeFromCache`) live on `Model`.
 */
export class Catalog {
  readonly #native: NativeCatalog;

  /** @internal — wraps a native catalog handle. Do not call from user code. */
  constructor(token: typeof internalCtorKey, native: NativeCatalog) {
    if (token !== internalCtorKey) {
      throw new TypeError("Catalog is internal — obtain instances via Manager.getCatalog()");
    }
    this.#native = native;
  }

  /** Catalog name (e.g. `"AzureFoundryCatalog"`). */
  getName(): string {
    return this.#native.getName();
  }

  /** All models in the catalog. */
  getModels(): Model[] {
    return wrapAll(this.#native.getModels());
  }

  /** Models currently present in the local cache. */
  getCachedModels(): Model[] {
    return wrapAll(this.#native.getCachedModels());
  }

  /** Models currently loaded into memory. */
  getLoadedModels(): Model[] {
    return wrapAll(this.#native.getLoadedModels());
  }

  /**
   * Look up a model by alias. Returns `undefined` when the alias is not
   * present in the catalog — never throws on "not found".
   */
  getModel(alias: string): Model | undefined {
    const n = this.#native.getModel(alias);
    return n === undefined ? undefined : wrapNativeModel(n);
  }

  /**
   * Look up a specific model variant by its full model id (`"alias-foo:1"`).
   * Returns `undefined` when not found.
   */
  getModelVariant(modelId: string): Model | undefined {
    const n = this.#native.getModelVariant(modelId);
    return n === undefined ? undefined : wrapNativeModel(n);
  }

  /**
   * Resolve the latest catalog version for the same model name. Returns
   * `undefined` when no matching catalog entry exists (e.g. cache-only mode
   * with the model not present in the cache file).
   */
  getLatestVersion(model: Model): Model | undefined {
    const n = this.#native.getLatestVersion(unwrapNativeModel(model));
    return n === undefined ? undefined : wrapNativeModel(n);
  }
}

/** @internal — used by `Manager` to wrap a native catalog handle into a JS `Catalog`. */
export function wrapNativeCatalog(native: NativeCatalog): Catalog {
  return new Catalog(internalCtorKey, native);
}

function wrapAll(natives: readonly NativeModel[]): Model[] {
  return natives.map((n) => wrapNativeModel(n));
}
