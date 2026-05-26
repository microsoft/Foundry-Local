// Manager-only tests for the v2 JS SDK. Uses the shared cache-only fixture
// helper. One Manager per file: created in `beforeAll`, disposed in
// `afterAll`. See `_fixtures/cacheOnlyManager.ts` for the policy.
//
// Dispose lifecycle tests live in `manager-dispose.test.ts` so this file
// can keep its Manager alive across every test.
import { afterAll, beforeAll, describe, expect, it } from "vitest";

import { Manager } from "../src/manager.js";

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
  console.warn(`[Manager tests] SKIPPED — ${nativePrereqsDiagnostic}`);
}

describeIfBuilt("Manager", () => {
  let fixture: CacheOnlyManagerFixture;

  beforeAll(() => {
    fixture = setupCacheOnlyManager({ appName: "foundry-local-js-sdk-v2-manager-tests" });
  });
  afterAll(() => {
    teardownCacheOnlyManager(fixture);
  });

  it("is an instance of Manager", () => {
    expect(fixture.manager).toBeInstanceOf(Manager);
  });

  it("rejects missing or empty appName before native construction", () => {
    expect(() => new Manager({ appName: "" })).toThrow(TypeError);
    // @ts-expect-error — exercising runtime guard
    expect(() => new Manager({})).toThrow(TypeError);
    // @ts-expect-error — exercising runtime guard
    expect(() => new Manager(undefined)).toThrow(TypeError);
  });

  it("rejects non-string modelCacheDir / externalServiceUrl", () => {
    // @ts-expect-error — exercising runtime guard
    expect(() => new Manager({ appName: "x", modelCacheDir: 5 })).toThrow(TypeError);
    // @ts-expect-error — exercising runtime guard
    expect(() => new Manager({ appName: "x", externalServiceUrl: {} })).toThrow(TypeError);
  });

  it("getWebServiceEndpoints returns an empty array when no service is running", () => {
    // Per foundry_local::Manager::GetWebServiceEndpoints contract: an empty
    // vector means the embedded web service is not running.
    const endpoints = fixture.manager.getWebServiceEndpoints();
    expect(endpoints).toEqual([]);
  });

  it("getCatalog returns the same Catalog backing each call", () => {
    // The underlying ICatalog& is owned by the Manager and stable across
    // calls — each call returns a fresh JS wrapper, but `getName()` should
    // be deterministic and non-empty.
    const a = fixture.manager.getCatalog();
    const b = fixture.manager.getCatalog();
    expect(a.getName()).toBe(b.getName());
    expect(a.getName().length).toBeGreaterThan(0);
  });

  it("disposed is false while the Manager is alive", () => {
    expect(fixture.manager.disposed).toBe(false);
  });
});
