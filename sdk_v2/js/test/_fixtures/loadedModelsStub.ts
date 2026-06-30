// A minimal stand-in for the Foundry service's model-command surface — just
// enough for the v2 SDK's `ModelCommandRouter` (external mode) to talk to a
// real HTTP endpoint instead of a dead port. Lets cache-only tests exercise the
// *happy* path of the remote load-state queries (getLoadedModels / isLoaded)
// rather than only the unreachable-service failure.
//
// Only the endpoints the router actually calls are implemented (see
// `sdk_v2/cpp/src/model_command_router.cc`):
//   GET /models/loaded       -> JSON array of loaded model-id strings
//   GET /models/load/{id}    -> 200 (load acknowledged)
//   GET /models/unload/{id}  -> 200 (unload acknowledged)
// Any other path returns 404 so unexpected routing surfaces as a test failure
// instead of silently passing.
//
// The server runs in a worker thread (its own event loop). This is essential:
// the SDK's native `getLoadedModels`/`isLoaded` are *synchronous* and block the
// caller's thread for the duration of the HTTP request. An in-process server on
// the same event loop would deadlock — it could never accept the connection
// while the main thread is parked in native code.
import { fileURLToPath } from "node:url";
import { Worker } from "node:worker_threads";

const workerPath = fileURLToPath(new URL("./loadedModelsStubWorker.cjs", import.meta.url));

export interface LoadedModelsStub {
  /** Base URL to point the Manager at (no trailing slash). */
  readonly url: string;
  /** Replace the set of loaded model ids reported by `GET /models/loaded`. */
  setLoaded(ids: readonly string[]): void;
  /** Stop the server and join the worker. */
  close(): Promise<void>;
}

/**
 * Start the stub on an ephemeral loopback port. Resolves once it is listening.
 *
 * @param initialLoaded ids reported as loaded before any {@link LoadedModelsStub.setLoaded} call.
 */
export async function startLoadedModelsStub(initialLoaded: readonly string[] = []): Promise<LoadedModelsStub> {
  const worker = new Worker(workerPath, { workerData: { initialLoaded: [...initialLoaded] } });

  const port = await new Promise<number>((resolve, reject) => {
    worker.once("error", reject);
    worker.once("message", (msg: { type?: string; port?: number }) => {
      if (msg?.type === "listening" && typeof msg.port === "number") {
        resolve(msg.port);
        return;
      }

      reject(new Error(`unexpected message from stub worker: ${JSON.stringify(msg)}`));
    });
  });

  return {
    url: `http://127.0.0.1:${port}`,
    setLoaded(ids: readonly string[]): void {
      worker.postMessage({ type: "setLoaded", ids: [...ids] });
    },
    close(): Promise<void> {
      return new Promise<void>((resolve) => {
        worker.once("exit", () => resolve());
        worker.postMessage({ type: "close" });
      });
    },
  };
}
