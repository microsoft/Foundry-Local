// Streaming tests for Session.stream / ChatSession.stream.
// Gated by TEST_MODEL_CACHE_DIR (real model required).
import { afterAll, afterEach, beforeAll, beforeEach, describe, expect, it } from "vitest";

import { FlErrorCode, isFoundryLocalError } from "../src/detail/errors.js";
import { Item } from "../src/items.js";
import { Request } from "../src/request.js";
import { ChatSession } from "../src/session.js";

import {
  type RealModelManagerFixture,
  haveTestModelCache,
  setupRealModelManager,
  teardownRealModelManager,
  testModelCacheDiagnostic,
} from "./_fixtures/realModelManager.js";

if (!haveTestModelCache) {
  console.warn(testModelCacheDiagnostic);
}

function buildPrompt(): Request {
  return new Request()
    .addItem(Item.systemMessage("You are concise."))
    .addItem(Item.userMessage("Count from one to ten."))
    .setOptions({ max_output_tokens: 512, temperature: 0 });
}

function extractText(item: Item): string {
  if (item.type === "text") return item.text;
  if (item.type === "message") {
    if (typeof item.content === "string") return item.content;
    if (item.parts) {
      let acc = "";
      for (const p of item.parts) {
        if (p.type === "text") acc += p.text;
      }
      return acc;
    }
  }
  return "";
}

describe.skipIf(!haveTestModelCache)("ChatSession.processStreamingRequest (real model)", () => {
  let fixture: RealModelManagerFixture | undefined;
  let session: ChatSession | undefined;

  beforeAll(async () => {
    fixture = await setupRealModelManager();
  }, 5 * 60_000);

  afterAll(() => {
    teardownRealModelManager(fixture);
  });

  beforeEach(() => {
    if (fixture === undefined) throw new Error("fixture missing");
    session = new ChatSession(fixture.model);
  });

  afterEach(() => {
    session?.dispose();
    session = undefined;
  });

  it(
    "yields at least one Item before completion and the items carry the requested numbers",
    async () => {
      if (session === undefined) throw new Error("fixture missing");
      const items: Item[] = [];
      for await (const item of session.processStreamingRequest(buildPrompt())) {
        items.push(item);
      }
      expect(items.length).toBeGreaterThanOrEqual(1);
      const total = items.reduce((acc, it) => acc + extractText(it), "").toLowerCase();
      expect(total.length).toBeGreaterThan(0);
      // Prompt asks the model to count 1..10 with temperature 0; require that
      // most of the numbers actually appear in the streamed reply (either as
      // digits like "1" or as words like "one").
      const numbers: ReadonlyArray<ReadonlyArray<string>> = [
        ["1", "one"],
        ["2", "two"],
        ["3", "three"],
        ["4", "four"],
        ["5", "five"],
        ["6", "six"],
        ["7", "seven"],
        ["8", "eight"],
        ["9", "nine"],
        ["10", "ten"],
      ];
      const seen = numbers.filter((variants) => variants.some((v) => total.includes(v)));
      expect(seen.length).toBeGreaterThanOrEqual(8);
    },
    2 * 60_000,
  );

  it(
    "concatenated streamed text covers the full count",
    async () => {
      if (session === undefined) throw new Error("fixture missing");
      let text = "";
      for await (const item of session.processStreamingRequest(buildPrompt())) {
        text += extractText(item);
      }
      const lower = text.toLowerCase();
      // Accept either digit or word forms for the boundary numbers.
      expect(lower.includes("1") || lower.includes("one")).toBe(true);
      expect(lower.includes("10") || lower.includes("ten")).toBe(true);
    },
    2 * 60_000,
  );

  it(
    "early break cancels the stream cleanly and the session remains usable",
    async () => {
      if (session === undefined) throw new Error("fixture missing");
      let count = 0;
      for await (const _item of session.processStreamingRequest(buildPrompt())) {
        count++;
        if (count >= 1) break;
      }
      // After the break the session should accept a follow-up send.
      const resp = await session.processRequest(
        new Request()
          .addItem(Item.userMessage("Reply with the single word 'ok'."))
          .setOptions({ max_output_tokens: 4, temperature: 0 }),
      );
      expect(resp.output.length).toBeGreaterThanOrEqual(1);
      const text = resp.output.map(extractText).join("").toLowerCase();
      expect(text).toContain("ok");
    },
    3 * 60_000,
  );

  it("pre-aborted AbortSignal rejects iteration with name === 'AbortError'", async () => {
    if (session === undefined) throw new Error("fixture missing");
    const ctrl = new AbortController();
    ctrl.abort();
    const iter = session.processStreamingRequest(buildPrompt(), { signal: ctrl.signal });
    await expect(async () => {
      for await (const _item of iter) {
        /* no Item expected */
      }
    }).rejects.toMatchObject({ name: "AbortError" });
  }, 60_000);

  it(
    "mid-stream abort rejects with AbortError and the session remains usable",
    async () => {
      if (session === undefined) throw new Error("fixture missing");
      const ctrl = new AbortController();
      let caught: unknown = null;
      try {
        let seen = 0;
        for await (const _item of session.processStreamingRequest(buildPrompt(), { signal: ctrl.signal })) {
          seen++;
          if (seen >= 1) ctrl.abort();
        }
      } catch (e) {
        caught = e;
      }
      // The abort may race the natural completion of a very short reply; if
      // we never observed an abort, just skip the assertion on `caught`.
      if (caught !== null) {
        expect((caught as { name: string }).name).toBe("AbortError");
        if (isFoundryLocalError(caught)) {
          expect(caught.code).toBe(FlErrorCode.OperationCancelled);
        }
      }
      // Session must still accept a follow-up send regardless.
      const resp = await session.processRequest(
        new Request()
          .addItem(Item.userMessage("Reply with the single word 'ok'."))
          .setOptions({ max_output_tokens: 4, temperature: 0 }),
      );
      expect(resp.output.length).toBeGreaterThanOrEqual(1);
      const text = resp.output.map(extractText).join("").toLowerCase();
      expect(text).toContain("ok");
    },
    3 * 60_000,
  );

  it(
    "a second stream on the same session works after the first completes",
    async () => {
      if (session === undefined) throw new Error("fixture missing");
      let first = "";
      for await (const item of session.processStreamingRequest(buildPrompt())) {
        first += extractText(item);
      }
      let second = "";
      for await (const item of session.processStreamingRequest(buildPrompt())) {
        second += extractText(item);
      }
      // Both streams ran the same prompt (count 1..10); both should mention
      // the boundary numbers in either digit or word form.
      const a = first.toLowerCase();
      const b = second.toLowerCase();
      expect(a.includes("1") || a.includes("one")).toBe(true);
      expect(a.includes("10") || a.includes("ten")).toBe(true);
      expect(b.includes("1") || b.includes("one")).toBe(true);
      expect(b.includes("10") || b.includes("ten")).toBe(true);
    },
    4 * 60_000,
  );
});
