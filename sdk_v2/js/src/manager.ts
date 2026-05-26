// Public `Manager` class for the v2 SDK. Mirrors the C# `FoundryLocalManager`
// in sdk_v2/cs/src/FoundryLocalManager.cs and the Python
// `FoundryLocalManager` in sdk_v2/python/src/foundry_local_sdk/manager.py.
//
// Surface: construction, getWebServiceEndpoints, getCatalog, dispose
// (+ Symbol.dispose). All Manager operations are synchronous — none of
// the underlying C ABI calls perform I/O. Real-work async (download, load,
// inference) lives on `Model` and `Session`.
import { type Catalog, wrapNativeCatalog } from "./catalog.js";
import { type NativeManager, getAddon } from "./detail/native.js";

export interface ManagerOptions {
  /** Application name, used to namespace SDK data on disk. Required. */
  readonly appName: string;
  /**
   * Override for the on-disk model cache directory. Maps to
   * `Configuration::SetModelCacheDir`. Useful for tests that want to point at
   * a temp directory pre-populated with `foundry.modelinfo.json`.
   */
  readonly modelCacheDir?: string;
  /**
   * Override for the external service URL the embedded service contacts.
   * Maps to `Configuration::SetExternalServiceUrl`. Combined with
   * `modelCacheDir` this enables fully offline cache-only operation.
   */
  readonly externalServiceUrl?: string;
}

export class Manager {
  readonly #native: NativeManager;

  constructor(options: ManagerOptions) {
    if (typeof options?.appName !== "string" || options.appName.length === 0) {
      throw new TypeError("Manager: options.appName must be a non-empty string");
    }
    if (options.modelCacheDir !== undefined && typeof options.modelCacheDir !== "string") {
      throw new TypeError("Manager: options.modelCacheDir must be a string when provided");
    }
    if (options.externalServiceUrl !== undefined && typeof options.externalServiceUrl !== "string") {
      throw new TypeError("Manager: options.externalServiceUrl must be a string when provided");
    }
    const addon = getAddon();
    const nativeOpts: { appName: string; modelCacheDir?: string; externalServiceUrl?: string } = {
      appName: options.appName,
    };
    if (options.modelCacheDir !== undefined) {
      nativeOpts.modelCacheDir = options.modelCacheDir;
    }
    if (options.externalServiceUrl !== undefined) {
      nativeOpts.externalServiceUrl = options.externalServiceUrl;
    }
    this.#native = new addon.Manager(nativeOpts);
  }

  /**
   * URLs the embedded web service is bound to. Returns an empty array when
   * the embedded web service is not running; callers can use that as an
   * is-running check. Mirrors `foundry_local::Manager::GetWebServiceEndpoints`.
   *
   * Synchronous — the underlying C ABI call is an in-memory vector copy.
   */
  getWebServiceEndpoints(): string[] {
    return this.#native.getWebServiceEndpoints();
  }

  /**
   * Returns the model catalog. The returned `Catalog` is bound to this
   * `Manager`'s lifetime — once `Manager.dispose()` runs, subsequent calls on
   * the `Catalog` will throw `FoundryLocalError`.
   */
  getCatalog(): Catalog {
    return wrapNativeCatalog(this.#native.getCatalog());
  }

  /** True if `dispose()` has been called on this `Manager`. */
  get disposed(): boolean {
    return this.#native.isDisposed();
  }

  /**
   * Releases the underlying native `foundry_local::Manager`. Idempotent:
   * calling `dispose()` more than once is a no-op. After disposal, every
   * other instance method (and any method on a `Catalog` or `Model` obtained
   * through this `Manager`) throws a `FoundryLocalError`.
   *
   * Also wired to `Symbol.dispose` so callers can use the TS 5.2 `using`
   * keyword: `using mgr = new Manager({ appName: "x" });`.
   */
  dispose(): void {
    this.#native.dispose();
  }

  [Symbol.dispose](): void {
    this.dispose();
  }
}
