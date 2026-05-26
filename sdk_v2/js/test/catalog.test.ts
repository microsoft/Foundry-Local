// Catalog tests for the v2 JS SDK. Uses the shared cache-only fixture
// helper so this file constructs exactly one Manager + cache directory.
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
  console.warn(`[Catalog tests] SKIPPED — ${nativePrereqsDiagnostic}`);
}

describeIfBuilt("Catalog (cache-only)", () => {
  let fixture: CacheOnlyManagerFixture;
  let catalog: Catalog;

  beforeAll(() => {
    fixture = setupCacheOnlyManager({ appName: "foundry-local-js-sdk-v2-catalog-tests" });
    catalog = fixture.manager.getCatalog();
  });

  afterAll(() => {
    teardownCacheOnlyManager(fixture);
  });

  it("getName returns a non-empty string", () => {
    const name = catalog.getName();
    expect(typeof name).toBe("string");
    expect(name.length).toBeGreaterThan(0);
  });

  it("getModels returns both fixture models", () => {
    const models = catalog.getModels();
    expect(models).toHaveLength(2);
    for (const m of models) {
      expect(m).toBeInstanceOf(Model);
    }
    const aliases = models.map((m) => m.getInfo().alias).sort();
    expect(aliases).toEqual(["phi-4-mini-instruct", "qwen2.5-0.5b-instruct"]);
  });

  it("getModels returned ids match the fixture", () => {
    const ids = catalog
      .getModels()
      .map((m) => m.getInfo().id)
      .sort();
    expect(ids).toEqual(["phi-4-mini-instruct-generic-cpu:2", "qwen2.5-0.5b-instruct-generic-cpu:1"]);
  });

  it("getModel resolves a known alias to a Model with matching info", () => {
    const model = catalog.getModel("phi-4-mini-instruct");
    expect(model).toBeInstanceOf(Model);
    if (model === undefined) throw new Error("unreachable");
    const info = model.getInfo();
    expect(info.alias).toBe("phi-4-mini-instruct");
    expect(info.id).toBe("phi-4-mini-instruct-generic-cpu:2");
    expect(info.name).toBe("phi-4-mini-instruct-generic-cpu");
    expect(info.version).toBe(2);
    expect(info.task).toBe("chat-completion");
    expect(info.publisher).toBe("Microsoft");
  });

  it("getModel resolves a different known alias", () => {
    const model = catalog.getModel("qwen2.5-0.5b-instruct");
    expect(model).toBeInstanceOf(Model);
    if (model === undefined) throw new Error("unreachable");
    expect(model.getInfo().id).toBe("qwen2.5-0.5b-instruct-generic-cpu:1");
  });

  it("getModel returns undefined for an unknown alias", () => {
    const model = catalog.getModel("does-not-exist-anywhere");
    expect(model).toBeUndefined();
  });

  it("getModelVariant resolves a full model id", () => {
    const variant = catalog.getModelVariant("qwen2.5-0.5b-instruct-generic-cpu:1");
    expect(variant).toBeInstanceOf(Model);
    if (variant === undefined) throw new Error("unreachable");
    const info = variant.getInfo();
    expect(info.id).toBe("qwen2.5-0.5b-instruct-generic-cpu:1");
    expect(info.alias).toBe("qwen2.5-0.5b-instruct");
  });

  it("getCachedModels returns Model instances", () => {
    // Cache-only mode: every model in the cache JSON counts as cached.
    const cached = catalog.getCachedModels();
    expect(cached.length).toBeGreaterThanOrEqual(0);
    for (const m of cached) {
      expect(m).toBeInstanceOf(Model);
    }
  });

  it("getLoadedModels returns an empty array (nothing loaded)", () => {
    const loaded = catalog.getLoadedModels();
    expect(loaded).toEqual([]);
  });
});
