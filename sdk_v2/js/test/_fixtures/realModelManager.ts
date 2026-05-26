// Real-model fixture for v2 SDK integration tests that need an actual loaded
// inference session (model lifecycle, chat session, eventual streaming).
//
// Gated by the TEST_MODEL_CACHE_DIR environment variable, mirroring the C++
// SDK's SharedTestEnv pattern (see cpp-testing.instructions.md). When the
// env var is not set, tests must skip via `describe.skipIf(!haveTestModelCache)`
// rather than fail.
//
// CI policy: if running under CI (TF_BUILD or CI env var) AND the selected
// model is not already on-disk in the cache, tests skip instead of triggering
// a multi-gigabyte download. Local devs implicitly opt into downloads simply
// by setting TEST_MODEL_CACHE_DIR.
import { existsSync, statSync } from "node:fs";

import type { Catalog } from "../../src/catalog.js";
import { Manager } from "../../src/manager.js";
import type { Model } from "../../src/model.js";

const envCache = process.env.TEST_MODEL_CACHE_DIR;
const cacheDirExists =
  envCache !== undefined && envCache.length > 0 && existsSync(envCache) && statSync(envCache).isDirectory();

/**
 * True iff `TEST_MODEL_CACHE_DIR` is set and points at a real directory.
 * Tests should gate on this via `describe.skipIf(!haveTestModelCache)`.
 */
export const haveTestModelCache: boolean = cacheDirExists;

export const testModelCacheDiagnostic = haveTestModelCache
  ? `[v2 SDK real-model tests] using cache dir ${envCache}`
  : "[v2 SDK real-model tests] SKIPPED — TEST_MODEL_CACHE_DIR is not set or does not exist";

const isCi: boolean = process.env.CI !== undefined || process.env.TF_BUILD !== undefined;

export interface RealModelManagerOptions {
  /** Override the appName. Defaults to a fixed test id. */
  readonly appName?: string;
  /**
   * Catalog task to filter on when picking a fallback model. Defaults to
   * "chat-completion".
   */
  readonly task?: string;
  /**
   * Preferred model alias / name. If supplied AND the model is in the
   * catalog, it wins over the smallest-by-task fallback. Defaults to
   * "qwen2.5-0.5b-instruct-generic-cpu" — the smallest chat model we ship.
   */
  readonly namePreference?: string;
}

export interface RealModelManagerFixture {
  readonly manager: Manager;
  readonly catalog: Catalog;
  readonly model: Model;
}

/**
 * Build a Manager pointed at `TEST_MODEL_CACHE_DIR`, pick a small chat
 * model, ensure it is on disk + loaded, and return the fixture. The caller
 * passes the returned fixture to {@link teardownRealModelManager}.
 *
 * Throws (rather than skips) if {@link haveTestModelCache} is false — callers
 * must gate the `describe` block themselves.
 */
export async function setupRealModelManager(opts: RealModelManagerOptions = {}): Promise<RealModelManagerFixture> {
  if (!haveTestModelCache || envCache === undefined) {
    throw new Error(
      "setupRealModelManager called without TEST_MODEL_CACHE_DIR — gate the describe with `skipIf(!haveTestModelCache)`",
    );
  }

  const manager = new Manager({
    appName: opts.appName ?? "foundry-local-js-sdk-v2-real-tests",
    modelCacheDir: envCache,
  });
  const catalog = manager.getCatalog();
  const namePref = opts.namePreference ?? "qwen2.5-0.5b-instruct-generic-cpu";

  // Preference 1: exact name / alias hit.
  let model: Model | undefined = catalog.getModel(namePref);

  // Preference 1b: catalog entries often carry a `-N` version suffix
  // (e.g. `nemotron-speech-streaming-en-0.6b-generic-cpu-3`). If the exact
  // hit missed, try matching by prefix before falling back to the task
  // filter — caller-specified names should win over "smallest by task".
  if (model === undefined && opts.namePreference !== undefined) {
    const prefixed = catalog.getModels().find((m) => m.getInfo().name.startsWith(namePref));
    if (prefixed !== undefined) {
      model = prefixed;
    }
  }

  // Preference 2: smallest model matching the task filter.
  if (model === undefined) {
    const task = opts.task ?? "chat-completion";
    const all = catalog.getModels();
    const matching = all.filter((m) => {
      const info = m.getInfo();
      return info.task === task && info.deviceType === "CPU";
    });
    if (matching.length === 0) {
      manager.dispose();
      throw new Error(
        `No catalog model matches task='${task}' deviceType='CPU' (and preference '${namePref}' missing)`,
      );
    }
    matching.sort(
      (a, b) =>
        (a.getInfo().filesizeMb ?? Number.POSITIVE_INFINITY) - (b.getInfo().filesizeMb ?? Number.POSITIVE_INFINITY),
    );
    model = matching[0];
  }
  if (model === undefined) {
    manager.dispose();
    throw new Error("Unreachable: model selection failed");
  }

  // CI gate: refuse to trigger a real download.
  if (isCi && !model.isCached()) {
    manager.dispose();
    throw new SkipFixture(
      `[CI] selected model '${model.getInfo().name}' is not in the cache; skipping to avoid a download.`,
    );
  }

  await model.load();
  return { manager, catalog, model };
}

export function teardownRealModelManager(fixture: RealModelManagerFixture | undefined): void {
  if (fixture === undefined) return;
  if (!fixture.manager.disposed) {
    fixture.manager.dispose();
  }
}

/**
 * Sentinel error type that tests recognise as "we couldn't run this fixture
 * for an environmental reason — skip the suite". The caller catches this
 * in a beforeAll and flips a module-level flag to gate `describe.skipIf`.
 */
export class SkipFixture extends Error {
  override name = "SkipFixture";
}
