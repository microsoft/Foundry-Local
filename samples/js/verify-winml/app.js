/**
 * Foundry Local SDK - WinML 2.0 EP Verification Script (JavaScript)
 *
 * Verifies:
 *   1. Execution providers are discovered and registered
 *   2. Accelerated models appear in catalog after EP registration
 *   3. Streaming chat completions work on an accelerated model
 */

import { FoundryLocalManager } from "foundry-local-sdk";

const PASS = "\x1b[92m[PASS]\x1b[0m";
const FAIL = "\x1b[91m[FAIL]\x1b[0m";
const INFO = "\x1b[94m[INFO]\x1b[0m";
const WARN = "\x1b[93m[WARN]\x1b[0m";

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

function isAcceleratedVariant(variant) {
  const runtime = variant.info?.runtime;
  return Boolean(runtime && ["GPU", "NPU"].includes(runtime.deviceType));
}

function getDevicePriority(variant) {
  const deviceType = variant.info?.runtime?.deviceType;
  if (deviceType === "GPU") {
    return 0;
  }

  if (deviceType === "NPU") {
    return 1;
  }

  return 2;
}

async function main() {
  // ── 0. Initialize FoundryLocalManager ──────────────────────
  printSeparator("Initialization");
  const manager = FoundryLocalManager.create({
    appName: "verify_winml",
    logLevel: "info",
  });
  console.log(`${INFO} FoundryLocalManager initialized.`);

  // ── 1. Discover & Register EPs ────────────────────────────
  printSeparator("Step 1: Discover & Register Execution Providers");
  let eps = [];
  try {
    eps = manager.discoverEps();
    console.log(`${INFO} Discovered ${eps.length} execution providers:`);
    for (const ep of eps) {
      console.log(`  - ${ep.name.padEnd(40)}  Registered: ${ep.isRegistered}`);
    }
    logResult("EP Discovery", true, `${eps.length} EP(s) found`);
  } catch (e) {
    logResult("EP Discovery", false, e.message);
  }

  if (!eps.length) {
    const detail = "No execution providers discovered on this machine";
    logResult("EP Download & Registration", false, detail);
    console.log(`\n${FAIL} ${detail}.`);
    printSummary();
    return;
  }

  try {
    let lastProgressEp = null;
    let lastProgressPercent = -1;
    const result = await manager.downloadAndRegisterEps((epName, percent) => {
      if (lastProgressEp && (lastProgressEp !== epName || percent < lastProgressPercent)) {
        process.stdout.write("\n");
      }
      lastProgressEp = epName;
      lastProgressPercent = percent;
      process.stdout.write(`\r  Downloading ${epName}: ${percent.toFixed(1)}%`);
    });
    if (lastProgressEp) {
      console.log();
    }

    console.log(`${INFO} EP registration result: success=${result.success}, status=${result.status}`);
    if (result.registeredEps?.length) {
      console.log(`  Registered: ${result.registeredEps.join(", ")}`);
    }
    if (result.failedEps?.length) {
      console.log(`  Failed:     ${result.failedEps.join(", ")}`);
    }

    const downloadOk = result.success || (result.registeredEps?.length ?? 0) > 0;
    const detail = downloadOk && result.registeredEps?.length
      ? `${result.registeredEps.length} EP(s) registered`
      : result.status;
    logResult("EP Download & Registration", downloadOk, detail);
    if (!downloadOk) {
      printSummary();
      return;
    }
  } catch (e) {
    console.log();
    logResult("EP Download & Registration", false, e.message);
    printSummary();
    return;
  }

  // ── 2. List Models & Find Accelerated Variants ────────────
  printSeparator("Step 2: Model Catalog - Accelerated Models");
  const models = await manager.catalog.getModels();
  console.log(`${INFO} Total models in catalog: ${models.length}`);

  const acceleratedVariants = [];

  for (const model of models) {
    for (const variant of model.variants) {
      if (isAcceleratedVariant(variant)) {
        acceleratedVariants.push(variant);
      }
    }
  }

  console.log(`${INFO} Accelerated model variants: ${acceleratedVariants.length}`);
  for (const variant of acceleratedVariants) {
    const runtime = variant.info?.runtime;
    const ep = runtime?.executionProvider || "?";
    const device = runtime?.deviceType || "?";
    console.log(`  - ${variant.id.padEnd(50)}  Device: ${String(device).padEnd(3)}  EP: ${ep}`);
  }

  logResult(
    "Catalog - Accelerated models found",
    acceleratedVariants.length > 0,
    `${acceleratedVariants.length} accelerated variant(s)`,
  );

  const chosen = [...acceleratedVariants].sort(
    (left, right) => getDevicePriority(left) - getDevicePriority(right),
  )[0];
  if (!chosen) {
    console.log(`\n${FAIL} No accelerated model variants are available.`);
    console.log(`${WARN} Ensure the system has a compatible accelerator and matching model variants installed.`);
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
    const client = chosen.createChatClient();
    let responseText = "";
    const start = Date.now();
    for await (const chunk of client.completeStreamingChat(messages)) {
      const content = chunk?.choices?.[0]?.delta?.content;
      if (content) {
        responseText += content;
        process.stdout.write(content);
      }
    }
    const elapsed = ((Date.now() - start) / 1000).toFixed(2);
    console.log();
    logResult("Streaming Chat (Native)", responseText.length > 0, `${responseText.length} chars in ${elapsed}s`);
  } catch (e) {
    logResult("Streaming Chat (Native)", false, e.message);
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
