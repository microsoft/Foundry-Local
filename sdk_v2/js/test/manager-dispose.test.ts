// Dispose-lifecycle tests for the v2 SDK Manager. Isolated from the rest of
// the test suite so this file owns the only test fixtures that construct
// multiple Managers sequentially.
//
// Each test gets a fresh Manager (constructed inline, no shared fixture) so
// the dispose-then-call assertions don't poison sibling tests.
import { describe, expect, it } from "vitest";

import { isFoundryLocalError } from "../src/detail/errors.js";
import { Manager } from "../src/manager.js";

import { haveNativePrereqs, nativePrereqsDiagnostic } from "./_fixtures/cacheOnlyManager.js";

const describeIfBuilt = haveNativePrereqs ? describe : describe.skip;

if (!haveNativePrereqs) {
  // eslint-disable-next-line no-console
  console.warn(`[Manager.dispose tests] SKIPPED — ${nativePrereqsDiagnostic}`);
}

function freshManager(appNameSuffix: string): Manager {
  return new Manager({ appName: `foundry-local-js-sdk-v2-dispose-${appNameSuffix}` });
}

describeIfBuilt("Manager.dispose", () => {
  it("disposed is false on a fresh Manager", () => {
    const mgr = freshManager("fresh");
    try {
      expect(mgr.disposed).toBe(false);
    } finally {
      mgr.dispose();
    }
  });

  it("dispose() is idempotent — calling twice does not throw", () => {
    const mgr = freshManager("idempotent");
    mgr.dispose();
    expect(mgr.disposed).toBe(true);
    expect(() => mgr.dispose()).not.toThrow();
    expect(mgr.disposed).toBe(true);
  });

  it("calling getWebServiceEndpoints after dispose() throws a tagged FoundryLocalError", () => {
    const mgr = freshManager("post-dispose-call");
    mgr.dispose();
    try {
      mgr.getWebServiceEndpoints();
      throw new Error("expected getWebServiceEndpoints to throw");
    } catch (err) {
      expect(isFoundryLocalError(err)).toBe(true);
      const fle = err as Error & { code: number };
      expect(fle.code).toBe(4); // FOUNDRY_LOCAL_ERROR_INVALID_USAGE
      expect(fle.message).toMatch(/disposed/i);
    }
  });

  it("calling getCatalog after dispose() throws a tagged FoundryLocalError", () => {
    const mgr = freshManager("post-dispose-catalog");
    mgr.dispose();
    try {
      mgr.getCatalog();
      throw new Error("expected getCatalog to throw");
    } catch (err) {
      expect(isFoundryLocalError(err)).toBe(true);
      const fle = err as Error & { code: number };
      expect(fle.code).toBe(4);
    }
  });

  it("Symbol.dispose is wired and idempotent", () => {
    const mgr = freshManager("symbol-dispose");
    mgr[Symbol.dispose]();
    expect(mgr.disposed).toBe(true);
    expect(() => mgr[Symbol.dispose]()).not.toThrow();
    expect(mgr.disposed).toBe(true);
  });

  it("`using` declaration disposes at scope exit", () => {
    let captured: Manager | undefined;
    {
      using mgr = freshManager("using");
      captured = mgr;
      expect(mgr.disposed).toBe(false);
    }
    expect(captured?.disposed).toBe(true);
  });
});
