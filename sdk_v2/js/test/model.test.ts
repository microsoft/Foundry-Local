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

const describeIfBuilt = haveNativePrereqs ? describe : describe.skip;

if (!haveNativePrereqs) {
  // eslint-disable-next-line no-console
  console.warn(`[Model tests] SKIPPED — ${nativePrereqsDiagnostic}`);
}

describeIfBuilt("Model (cache-only)", () => {
  let fixture: CacheOnlyManagerFixture;
  let catalog: Catalog;
  let model: Model;

  beforeAll(() => {
    fixture = setupCacheOnlyManager({ appName: "foundry-local-js-sdk-v2-model-tests" });
    catalog = fixture.manager.getCatalog();
    const m = catalog.getModel("phi-4-mini-instruct");
    if (m === undefined) {
      throw new Error("fixture setup failed — alias not resolved");
    }
    model = m;
  });

  afterAll(() => {
    teardownCacheOnlyManager(fixture);
  });

  it("getInfo returns a plain object with all required fields populated", () => {
    const info = model.getInfo();
    expect(info.id).toBe("phi-4-mini-instruct-generic-cpu:2");
    expect(info.name).toBe("phi-4-mini-instruct-generic-cpu");
    expect(info.version).toBe(2);
    expect(info.alias).toBe("phi-4-mini-instruct");
    expect(info.uri).toMatch(/azureml:\/\//);
    // deviceType is a string union — generic-cpu fixture has no explicit
    // device, so it surfaces as "Invalid" (NOTSET) or "CPU" depending on the
    // wrapper's defaulting. Assert it's one of the allowed values.
    expect(["CPU", "GPU", "NPU", "Invalid"]).toContain(info.deviceType);
    expect(typeof info.createdAtUnix).toBe("number");
    expect(typeof info.isTestModel).toBe("boolean");
  });

  it("getInfo populates optional fields that were present in the cache JSON", () => {
    const info = model.getInfo();
    expect(info.task).toBe("chat-completion");
    expect(info.publisher).toBe("Microsoft");
    expect(info.displayName).toBe("Phi-4 Mini Instruct");
    expect(info.modelType).toBe("ONNX");
  });

  it("getInfo returns a fresh snapshot each call (plain object, not a live view)", () => {
    const a = model.getInfo();
    const b = model.getInfo();
    expect(a).not.toBe(b); // different object identity
    expect(a).toEqual(b); // same contents
  });

  it("isCached returns a boolean", () => {
    // In cache-only mode with a fixture file, the model is "in the cache"
    // but the on-disk weight files don't exist. The wrapper's IsCached can
    // legitimately return either value; we just assert it's a boolean.
    expect(typeof model.isCached()).toBe("boolean");
  });

  it("isLoaded returns false (model is not actually loaded)", () => {
    expect(model.isLoaded()).toBe(false);
  });

  it("getPath returns a string", () => {
    expect(typeof model.getPath()).toBe("string");
  });

  it("the Model survives after the local Catalog reference is dropped", () => {
    // The Model wrapper pins its parent Manager directly (not via the
    // Catalog), so it must remain usable even if the JS Catalog reference
    // goes out of scope and gets collected.
    let local: Catalog | undefined = fixture.manager.getCatalog();
    const m = local.getModel("phi-4-mini-instruct");
    expect(m).toBeInstanceOf(Model);
    if (m === undefined) throw new Error("unreachable");
    local = undefined;
    if (typeof globalThis.gc === "function") {
      globalThis.gc();
    }
    expect(m.getInfo().alias).toBe("phi-4-mini-instruct");
  });
});
