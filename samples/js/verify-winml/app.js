/**
 * Foundry Local SDK - WinML 2.0 EP Verification Script (JavaScript)
 *
 * Verifies:
 *   1. WinML execution providers are discovered and registered
 *   2. GPU models appear in catalog after EP registration
 *   3. Streaming chat completions work on a WinML-accelerated model
 *   4. OpenAI SDK chat completions work against a WinML-loaded model
 */

import { FoundryLocalManager } from "foundry-local-sdk";
import OpenAI from "openai";

const PASS = "\x1b[92m[PASS]\x1b[0m";
const FAIL = "\x1b[91m[FAIL]\x1b[0m";
const INFO = "\x1b[94m[INFO]\x1b[0m";

const results = [];

function logResult(testName, passed, detail = "") {
  const status = passed ? PASS : FAIL;
  const msg = detail ? `${status} ${testName} - ${detail}` : `${status} ${testName}`;
  console.log(msg);
  results.push({ testName, passed });
}

function printSeparator(title) {
  console.log(`\n${"=".repeat(60)}`);
  console.log(`  ${title}`);
  console.log(`${"=".repeat(60)}\n`);
}

function isWinmlEp(name) {
  const lower = name.toLowerCase();
  return lower.includes("winml") || lower.includes("dml");
}

async function main() {
  // ── 0. Initialize FoundryLocalManager ──────────────────────
  printSeparator("Initialization");
  const manager = await FoundryLocalManager.create({
    appName: "verify_winml",
    logLevel: "info",
  });
  console.log(`${INFO} FoundryLocalManager initialized.`);

  // ── 1. Discover & Register EPs ────────────────────────────
  printSeparator("Step 1: Discover & Register Execution Providers");
  try {
    const eps = await manager.discoverEps();
    console.log(`${INFO} Discovered ${eps.length} execution providers:`);
    for (const ep of eps) {
      console.log(`  - ${ep.name.padEnd(40)}  Registered: ${ep.isRegistered}`);
    }
    logResult("EP Discovery", true, `${eps.length} EP(s) found`);
  } catch (e) {
    logResult("EP Discovery", false, e.message);
  }

  try {
    const result = await manager.downloadAndRegisterEps((epName, percent) => {
      process.stdout.write(`\r  Downloading ${epName}: ${percent.toFixed(1)}%`);
    });
    console.log();
    console.log(`${INFO} EP registration result: success=${result.success}, status=${result.status}`);
    if (result.registeredEps?.length) console.log(`  Registered: ${result.registeredEps.join(", ")}`);
    if (result.failedEps?.length) console.log(`  Failed:     ${result.failedEps.join(", ")}`);
    logResult("EP Download & Registration", result.success);
  } catch (e) {
    console.log();
    logResult("EP Download & Registration", false, e.message);
  }

  // ── 2. List Models & Find GPU/WinML Variants ───────────────
  printSeparator("Step 2: Model Catalog - GPU/WinML Models");
  const models = await manager.catalog.listModels();
  console.log(`${INFO} Total models in catalog: ${models.length}`);

  const gpuVariants = [];
  const winmlVariants = [];

  for (const model of models) {
    for (const variant of model.variants) {
      const rt = variant.info?.runtime;
      if (rt?.deviceType === "GPU") {
        gpuVariants.push(variant);
        if (isWinmlEp(rt.executionProvider || "")) {
          winmlVariants.push(variant);
        }
      }
    }
  }

  console.log(`${INFO} GPU model variants: ${gpuVariants.length}`);
  for (const v of gpuVariants) {
    const ep = v.info?.runtime?.executionProvider || "?";
    console.log(`  - ${v.id.padEnd(50)}  EP: ${ep}`);
  }

  logResult("Catalog - GPU models found", gpuVariants.length > 0, `${gpuVariants.length} GPU variant(s)`);

  // Pick a GPU variant (prefer WinML, fall back to any GPU)
  const chosen = winmlVariants[0] || gpuVariants[0];
  if (!chosen) {
    console.log(`\n${FAIL} No GPU models available. Cannot proceed with inference tests.`);
    printSummary();
    process.exit(1);
  }

  const chosenEp = chosen.info?.runtime?.executionProvider || "unknown";
  console.log(`\n${INFO} Selected model: ${chosen.id} (EP: ${chosenEp})`);

  // ── 3. Download & Load Model ──────────────────────────────
  printSeparator("Step 3: Download & Load Model");
  try {
    await chosen.download((percent) => {
      process.stdout.write(`\r  Downloading model: ${percent.toFixed(1)}%`);
    });
    console.log();
    logResult("Model Download", true);
  } catch (e) {
    console.log();
    logResult("Model Download", false, e.message);
    printSummary();
    process.exit(1);
  }

  try {
    await chosen.load();
    logResult("Model Load", true, `Loaded ${chosen.id}`);
  } catch (e) {
    logResult("Model Load", false, e.message);
    printSummary();
    process.exit(1);
  }

  // ── 4. Streaming Chat Completions (Native SDK) ────────────
  printSeparator("Step 4: Streaming Chat Completions (Native)");
  const messages = [
    { role: "system", content: "You are a helpful assistant." },
    { role: "user", content: "What is 2 + 2? Reply with just the number." },
  ];

  try {
    const client = manager.getChatClient();
    let responseText = "";
    const start = Date.now();
    for await (const chunk of client.completeStreamingChat(messages, { modelId: chosen.id })) {
      if (chunk.text) {
        responseText += chunk.text;
        process.stdout.write(chunk.text);
      }
    }
    const elapsed = ((Date.now() - start) / 1000).toFixed(2);
    console.log();
    logResult("Streaming Chat (Native)", responseText.length > 0, `${responseText.length} chars in ${elapsed}s`);
  } catch (e) {
    logResult("Streaming Chat (Native)", false, e.message);
  }

  // ── 5. OpenAI SDK Chat Completions ────────────────────────
  printSeparator("Step 5: Chat Completions (OpenAI SDK)");
  try {
    manager.startWebService();
    const webUrl = manager.urls?.[0];
    if (!webUrl) throw new Error("Web service did not return a URL");
    console.log(`${INFO} Web service started at: ${webUrl}`);

    const oaiClient = new OpenAI({
      baseURL: `${webUrl}/v1`,
      apiKey: "not-needed",
    });
    const response = await oaiClient.chat.completions.create({
      model: chosen.id,
      messages: [
        { role: "system", content: "You are a helpful assistant." },
        { role: "user", content: "Name three colors. Reply briefly." },
      ],
    });
    const content = response.choices[0]?.message?.content || "";
    console.log(`  Response: ${content.slice(0, 200)}`);
    logResult("Chat (OpenAI SDK)", content.length > 0, `${content.length} chars`);
  } catch (e) {
    logResult("Chat (OpenAI SDK)", false, e.message);
  }

  printSummary();
}

function printSummary() {
  printSeparator("Summary");
  const passed = results.filter((r) => r.passed).length;
  for (const { testName, passed: p } of results) {
    console.log(`  ${p ? "✓" : "✗"} ${testName}`);
  }
  console.log(`\n  ${passed}/${results.length} tests passed`);
  if (passed < results.length) process.exit(1);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
