import { FlErrorCode, isFoundryLocalError } from "./detail/errors.js";
import {
  type NativeAudioSession,
  type NativeChatSession,
  type NativeEmbeddingsSession,
  type NativeSession,
  getAddon,
} from "./detail/native.js";
import type { Item } from "./items.js";
// Public `Session` / `ChatSession` classes for the v2 SDK.
//
// Surface:
//   * `Session` is an abstract base — not directly constructible. Users
//     construct a modality-specific subclass (`ChatSession` today;
//     `AudioSession` / `EmbeddingsSession` later). The base carries the
//     shared inference plumbing (processRequest / processStreamingRequest
//     / setOptions / dispose).
//   * `new ChatSession(model)` — construct directly from a loaded `Model`.
//     The native ctor calls `flSession_Create` synchronously.
//   * `session.processRequest(request) -> Promise<Response>` (non-streaming)
//   * `session.processStreamingRequest(request, { signal? }) ->
//     AsyncIterable<Item>`
//     `AbortSignal` cancellation maps the native `OperationCancelled`
//     rejection to a Web/Node-standard `AbortError`.
//   * `ChatSession` adds turn tracking and tool definitions.
//   * `setOptions(...)`, `dispose()`, `Symbol.dispose`.
import { Model, unwrapNativeModel } from "./model.js";
import { type Request, type RequestOptions, unwrapNativeRequest } from "./request.js";
import type { Response } from "./response.js";

/** Options accepted by streaming Session APIs. */
export interface StreamOptions {
  /**
   * Optional cancellation signal. When the signal aborts, the underlying
   * `Request` is cancelled and the async iterator rejects with an `Error`
   * whose `name === "AbortError"` (mirroring the Web/Node standard).
   */
  readonly signal?: AbortSignal;
}

function modelToNativeChatSession(model: Model): NativeChatSession {
  if (!(model instanceof Model)) {
    throw new TypeError("ChatSession: expected a Model as the first argument");
  }
  const nativeModel = unwrapNativeModel(model);
  return new (getAddon().ChatSession)(nativeModel);
}

function modelToNativeEmbeddingsSession(model: Model): NativeEmbeddingsSession {
  if (!(model instanceof Model)) {
    throw new TypeError("EmbeddingsSession: expected a Model as the first argument");
  }
  // Validate task in JS BEFORE constructing the native session. Matches the
  // C# (`ValidateTask`) and Python (`__init__` precheck) conventions: a
  // wrong-task model surfaces a `TypeError` rather than a native
  // `FoundryLocalError`, and the check works whether or not the model has
  // been loaded yet (the native ctor would fail later either way).
  const task = model.getInfo().task;
  if (task !== "embeddings") {
    throw new TypeError(`EmbeddingsSession requires a model with task 'embeddings', but got '${task ?? "(unset)"}'.`);
  }
  const nativeModel = unwrapNativeModel(model);
  return new (getAddon().EmbeddingsSession)(nativeModel);
}

function modelToNativeAudioSession(model: Model): NativeAudioSession {
  if (!(model instanceof Model)) {
    throw new TypeError("AudioSession: expected a Model as the first argument");
  }
  // JS-side task validation, same rationale as `modelToNativeEmbeddingsSession`.
  const task = model.getInfo().task;
  if (task !== "automatic-speech-recognition") {
    throw new TypeError(
      `AudioSession requires a model with task 'automatic-speech-recognition', but got '${task ?? "(unset)"}'.`,
    );
  }
  const nativeModel = unwrapNativeModel(model);
  return new (getAddon().AudioSession)(nativeModel);
}

/**
 * Drive a native streaming session and yield each item to the consumer.
 * Handles backpressure (the JS-side queue grows; the native TSFN backpressure
 * caps producer-side queueing), abort signal wiring, error mapping, and
 * deterministic cleanup on early break.
 */
async function* streamItems(
  native: NativeSession,
  request: Request,
  signal: AbortSignal | undefined,
): AsyncIterable<Item> {
  if (signal?.aborted) {
    throw makeAbortError("Stream aborted before start");
  }

  const nativeReq = unwrapNativeRequest(request);
  const queue: Item[] = [];
  let waiter: (() => void) | null = null;
  let done = false;
  let nativeError: unknown = null;

  const onAbort = (): void => {
    try {
      request.cancel();
    } catch {
      // Cancel is best-effort; the request may already be complete.
    }
  };
  if (signal !== undefined) {
    signal.addEventListener("abort", onAbort, { once: true });
  }

  const wake = (): void => {
    if (waiter !== null) {
      const w = waiter;
      waiter = null;
      w();
    }
  };

  const onItem = (item: unknown): void => {
    queue.push(item as Item);
    wake();
  };

  const nativePromise = native.processStreamingRequest(nativeReq, onItem).then(
    () => {
      done = true;
      wake();
    },
    (err: unknown) => {
      nativeError = err;
      done = true;
      wake();
    },
  );

  try {
    while (true) {
      const next = queue.shift();
      if (next !== undefined) {
        yield next;
        continue;
      }
      if (done) {
        if (nativeError !== null) {
          // Map native cancellation triggered by the AbortSignal to AbortError.
          if (
            signal?.aborted === true &&
            isFoundryLocalError(nativeError) &&
            nativeError.code === FlErrorCode.OperationCancelled
          ) {
            (nativeError as { name: string }).name = "AbortError";
          }
          throw nativeError;
        }
        return;
      }
      await new Promise<void>((resolve) => {
        waiter = resolve;
      });
    }
  } finally {
    if (signal !== undefined) {
      signal.removeEventListener("abort", onAbort);
    }
    if (!done) {
      // Consumer broke out early — cancel the request and let the native
      // promise settle so we don't leak an unhandled rejection.
      try {
        request.cancel();
      } catch {
        // ignore
      }
      await nativePromise;
    }
  }
}

function makeAbortError(message: string): Error {
  const err = new Error(message);
  err.name = "AbortError";
  return err;
}

export abstract class Session {
  // `protected` (not `#private`) so subclasses can downcast for
  // modality-specific native methods without a second field/storage slot.
  protected readonly native: NativeSession;

  protected constructor(native: NativeSession) {
    this.native = native;
  }

  /**
   * Run inference for `request`. Resolves with a `Response` snapshot when
   * generation completes. The full output is materialised before resolution
   * — use {@link processStreamingRequest} to consume items incrementally.
   *
   * Rejects with a `FoundryLocalError` on native failure. Calling
   * `request.cancel()` from another async context causes this promise to
   * reject with `code === FlErrorCode.OperationCancelled`.
   */
  async processRequest(request: Request): Promise<Response> {
    const nativeReq = unwrapNativeRequest(request);
    return (await this.native.processRequest(nativeReq)) as Response;
  }

  /**
   * Run inference for `request`, yielding each `Item` produced by the model
   * as it streams. The returned `AsyncIterable` can be consumed with
   * `for await (const item of session.processStreamingRequest(req)) { ... }`.
   *
   * Cancellation: pass `{ signal }`; aborting the signal cancels the native
   * request and causes the iterator to throw an `Error` with
   * `name === "AbortError"`. Breaking out of the `for await` loop also
   * cancels the underlying request.
   *
   * Non-cancellation failures throw a `FoundryLocalError`.
   */
  processStreamingRequest(request: Request, options?: StreamOptions): AsyncIterable<Item> {
    return streamItems(this.native, request, options?.signal);
  }

  /** Apply session-level options that persist across `processRequest()` calls. */
  setOptions(options: RequestOptions): this {
    this.native.setOptions(options);
    return this;
  }

  /** True once `dispose()` has run. */
  get disposed(): boolean {
    return this.native.isDisposed();
  }

  /**
   * Release the underlying native session handle. Idempotent. After
   * disposal, every other instance method rejects (async) or throws (sync)
   * with `FoundryLocalError` / `code === FlErrorCode.InvalidUsage`.
   */
  dispose(): void {
    this.native.dispose();
  }

  [Symbol.dispose](): void {
    this.dispose();
  }
}

/** A tool definition exposed to {@link ChatSession.addToolDefinition}. */
export interface ToolDefinition {
  readonly name: string;
  readonly description: string;
  readonly jsonSchema: string;
}

export class ChatSession extends Session {
  /**
   * Construct a `ChatSession` bound to `model`. The model must be loaded
   * (call `await model.load()` first) or have been previously loaded. The
   * chat session additionally tracks conversation history and tool
   * definitions across `processRequest()` calls.
   *
   * Throws `TypeError` if `model` is not a `Model` instance, and a
   * `FoundryLocalError` if the native session cannot be created.
   */
  constructor(model: Model) {
    super(modelToNativeChatSession(model));
  }

  // Typed accessor for chat-only methods on the native handle. The base
  // stores the same handle as a `NativeSession`; the cast is safe because
  // `ChatSession` always constructs a `NativeChatSession`.
  get #nativeChat(): NativeChatSession {
    return this.native as NativeChatSession;
  }

  /**
   * Register a tool definition available to the model for the rest of the
   * session. Mirrors `foundry_local::ChatSession::AddToolDefinition`.
   */
  addToolDefinition(definition: ToolDefinition): this {
    this.#nativeChat.addToolDefinition(definition);
    return this;
  }

  /** Number of completed turns. */
  get turnCount(): number {
    return this.#nativeChat.turnCount();
  }

  /**
   * Rewind the last `count` turns: removes their input messages and
   * assistant replies from history and resets the generator.
   */
  undoTurns(count: number): void {
    this.#nativeChat.undoTurns(count);
  }
}

/**
 * Inference session for text-embedding models.
 *
 * Embeddings sessions are stateless and one-shot. Use the inherited
 * {@link Session.processRequest} method with `TextItem` inputs; the
 * response contains one `TensorItem` per input holding the embedding
 * vector.
 *
 * Streaming is not supported for embeddings — calling
 * {@link Session.processStreamingRequest} will fail at the native
 * boundary because the native `EmbeddingsSession` ObjectWrap does not
 * register a `processStreamingRequest` method.
 *
 * Task validation runs in the JS layer before the native ctor: passing a
 * non-embeddings model throws `TypeError` regardless of whether the
 * model has been loaded.
 */
export class EmbeddingsSession extends Session {
  constructor(model: Model) {
    super(modelToNativeEmbeddingsSession(model));
  }
}

/**
 * Inference session for automatic-speech-recognition models.
 *
 * Accepts `AudioItem` input (from URI or in-memory PCM bytes) and produces
 * `TextItem` output containing the transcription. For live streaming
 * input, push successive `Item.bytes(chunk)` PCM frames onto an
 * `ItemQueue`, add the queue to the request, and call `markFinished()`
 * when the input stream ends.
 *
 * Task validation runs in the JS layer before the native ctor: passing a
 * non-ASR model throws `TypeError` regardless of whether the model has
 * been loaded.
 */
export class AudioSession extends Session {
  constructor(model: Model) {
    super(modelToNativeAudioSession(model));
  }
}
