// Configuration shape for FoundryLocalManager.

export interface FoundryLocalConfig {
  /** **REQUIRED** Application name. Used to namespace SDK data on disk. */
  appName: string;

  /** Application data directory. */
  appDataDir?: string;

  /** Override the on-disk model cache directory. */
  modelCacheDir?: string;

  /** Directory for log files. */
  logsDir?: string;

  /** Logging level. */
  logLevel?: "trace" | "debug" | "info" | "warn" | "error" | "fatal";

  /** Web service bind URL. */
  webServiceUrls?: string;

  /** External service URL (when the web service runs in a separate process). */
  serviceEndpoint?: string;

  /**
   * Directory containing the native Foundry Local library (`foundry_local.dll` on Windows, `libfoundry_local.so`
   * on Linux, `libfoundry_local.dylib` on macOS). When set, the SDK pre-loads the library from this directory
   * before the addon resolves its dependencies. Must be applied before the first manager construction — once the
   * native addon is loaded, the path cannot be changed.
   */
  libraryPath?: string;

  /** Bag of additional settings passed through to the core. */
  additionalSettings?: { [key: string]: string };
}
