// Model tests for the v2 JS SDK. Cache-only fixture, same pattern as
// `catalog.test.ts`. Focuses on the read-only `Model` surface: `getInfo`,
// `isCached`, `isLoaded`, `getPath`, and lifetime invariants.
import { afterAll, beforeAll, describe, expect, it } from "vitest";

import type { Catalog } from "../src/catalog.js";
import { Model } from "../src/model.js";

import {
  type CacheOnlyManagerFixture,
  haveNativePrereqs,
  nativePrereqsDiagnostic,
  setupCacheOnlyManager,
  teardownCacheOnlyManager,
} from "./_fixtures/cacheOnlyManager.js";
import { type LoadedModelsStub, startLoadedModelsStub } from "./_fixtures/loadedModelsStub.js";

const describeIfBuilt = haveNativePrereqs ? describe : describe.skip;

if (!haveNativePrereqs) {
  // eslint-disable-next-line no-console
  console.warn(`[Model tests] SKIPPED — ${nativePrereqsDiagnostic}`);
}

describeIfBuilt("Model (cache-only)", () => {
  let fixture: CacheOnlyManagerFixture;
  let stub: LoadedModelsStub;
  let catalog: Catalog;
  let model: Model;

  beforeAll(async () => {
    stub = await startLoadedModelsStub();
    fixture = setupCacheOnlyManager({
      appName: "foundry-local-js-sdk-v2-model-tests",
      serviceEndpoint: stub.url,
    });
    catalog = fixture.manager.catalog;
    model = (await catalog.getModel("phi-4-mini-instruct")) as Model;
  });

  afterAll(async () => {
    teardownCacheOnlyManager(fixture);
    await stub.close();
  });

  it("info returns a plain object with all required fields populated", () => {
    const info = model.info;
    expect(info.id).toBe("phi-4-mini-instruct-generic-cpu:2");
    expect(info.name).toBe("phi-4-mini-instruct-generic-cpu");
    expect(info.version).toBe(2);
    expect(info.alias).toBe("phi-4-mini-instruct");
    expect(info.uri).toMatch(/azureml:\/\//);
    expect(["CPU", "GPU", "NPU", "Invalid"]).toContain(info.deviceType);
    expect(typeof info.createdAtUnix).toBe("number");
    expect(typeof info.isTestModel).toBe("boolean");
  });

  it("info populates optional fields that were present in the cache JSON", () => {
    const info = model.info;
    expect(info.task).toBe("chat-completion");
    expect(info.publisher).toBe("Microsoft");
    expect(info.displayName).toBe("Phi-4 Mini Instruct");
    expect(info.modelType).toBe("ONNX");
  });

  it("info preserves v1 compatibility fields with sane defaults", () => {
    const info = model.info;
    expect(info.providerType).toBe("AzureFoundry");
    expect(typeof info.cached).toBe("boolean");
    expect(info.runtime).toBeDefined();
    expect(info.runtime?.deviceType).toBe(info.deviceType);
    expect(typeof info.runtime?.executionProvider).toBe("string");
    expect(info.modelSettings ?? null).toBeNull();
    expect(info.promptTemplate ?? null).toBeNull();
  });

  it("info is a stable snapshot (same object identity across reads)", () => {
    // V1 surface caches the snapshot eagerly in the wrapper ctor.
    expect(model.info).toBe(model.info);
  });

  it("isCached returns a boolean", () => {
    expect(typeof model.isCached).toBe("boolean");
  });

  it("isLoaded routes to the external service and returns false when the model is not loaded", async () => {
    // The fixture points at a live stub service that reports no loaded models, so this exercises
    // the external load-state routing end-to-end and resolves to false.
    expect(await model.isLoaded()).toBe(false);
  });

  it("path returns a string", () => {
    expect(typeof model.path).toBe("string");
  });

  it("id and alias match info", () => {
    expect(model.id).toBe(model.info.id);
    expect(model.alias).toBe(model.info.alias);
  });

  it("the Model survives after the local Catalog reference is dropped", async () => {
    let local: Catalog | undefined = fixture.manager.catalog;
    const m = (await local.getModel("phi-4-mini-instruct")) as Model;
    expect(m).toBeInstanceOf(Model);
    local = undefined;
    if (typeof globalThis.gc === "function") {
      globalThis.gc();
    }
    expect(m.info.alias).toBe("phi-4-mini-instruct");
  });
});
