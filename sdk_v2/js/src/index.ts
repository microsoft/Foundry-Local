// Public entry for foundry-local-sdk v2.
//
// Surfaces still under construction (audio/embeddings sessions, legacy v1
// compatibility classes) are stubbed and throw at construction time so
// consumers building against the dev preview get a clear "not implemented"
// error.

export { Manager, type ManagerOptions } from "./manager.js";
export { Catalog } from "./catalog.js";
export { Model, type IModel, type ModelInfo } from "./model.js";
export { FlErrorCode, isFoundryLocalError, type FoundryLocalError } from "./detail/errors.js";

// Inference surface.
export { Request, type RequestOptions } from "./request.js";
export { ItemQueue } from "./item-queue.js";
export type { Response, FinishReason, TokenUsage } from "./response.js";
export { Session, ChatSession, EmbeddingsSession, AudioSession, type ToolDefinition } from "./session.js";
export {
  Item,
  type TextItem,
  type MessageItem,
  type BytesItem,
  type TensorItem,
  type ImageItem,
  type AudioItem,
  type ToolCallItem,
  type ToolResultItem,
  type MessageRole,
  type TextItemKind,
  type TensorDataType,
} from "./items.js";

const NOT_IMPL = "not implemented yet (see sdk_v2/js/docs/PortJsToSdkV2.md)";

// ── Legacy v1-compatible surface — not implemented yet ────────────────────

export class FoundryLocalManager {
  constructor() {
    throw new Error(`FoundryLocalManager: ${NOT_IMPL}`);
  }
}
export class ChatClient {
  constructor() {
    throw new Error(`ChatClient: ${NOT_IMPL}`);
  }
}
export class ResponsesClient {
  constructor() {
    throw new Error(`ResponsesClient: ${NOT_IMPL}`);
  }
}
export class AudioClient {
  constructor() {
    throw new Error(`AudioClient: ${NOT_IMPL}`);
  }
}
export class EmbeddingClient {
  constructor() {
    throw new Error(`EmbeddingClient: ${NOT_IMPL}`);
  }
}
export class LiveAudioTranscriptionSession {
  constructor() {
    throw new Error(`LiveAudioTranscriptionSession: ${NOT_IMPL}`);
  }
}
export class ModelLoadManager {
  constructor() {
    throw new Error(`ModelLoadManager: ${NOT_IMPL}`);
  }
}

export function getOutputText(): string {
  throw new Error(`getOutputText: ${NOT_IMPL}`);
}

// Legacy `Configuration` placeholder for the v1-compat shim.
export interface Configuration {
  readonly appName: string;
}
