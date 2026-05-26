// Types shared across the surface. New code should prefer the typed `Item` / `Session` surfaces.

export interface ModelInfo {
  readonly id: string;
  readonly name: string;
  readonly version: number;
  readonly alias: string;
  readonly uri: string;
  readonly deviceType: "CPU" | "GPU" | "NPU" | "Invalid";
  readonly executionProvider?: string;
  readonly displayName?: string;
  readonly modelType?: string;
  readonly publisher?: string;
  readonly license?: string;
  readonly licenseDescription?: string;
  readonly task?: string;
  readonly modelProvider?: string;
  readonly minFLVersion?: string;
  readonly parentUri?: string;
  readonly supportsToolCalling?: boolean;
  readonly fileSizeMb?: number;
  readonly maxOutputTokens?: number;
  readonly createdAtUnix: number;
  readonly isTestModel: boolean;
  readonly contextLength?: number;
  readonly inputModalities?: string;
  readonly outputModalities?: string;
  readonly capabilities?: string;
}

/** Information about a discoverable execution provider. */
export interface EpInfo {
  readonly name: string;
  readonly isRegistered: boolean;
}

/** Result of a `downloadAndRegisterEps()` call. */
export interface EpDownloadResult {
  readonly success: boolean;
  readonly status: string;
  readonly registeredEps: readonly string[];
  readonly failedEps: readonly string[];
}

/** OpenAI-compatible response format hint. */
export interface ResponseFormat {
  type: "text" | "json_object" | "json_schema" | "lark_grammar";
  jsonSchema?: string;
  larkGrammar?: string;
}

/** OpenAI-compatible tool choice descriptor. */
export interface ToolChoice {
  type: "none" | "auto" | "required" | "function";
  name?: string;
}
