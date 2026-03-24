/**
 * Dynamic model selector – picks the best Foundry Local model for
 * the current device based on available system RAM and the SDK catalogue.
 *
 * Selection strategy:
 *  1. Enumerate all chat-completion models from the catalogue.
 *  2. Exclude models that are too large for available RAM.
 *  3. Rank remaining models by a quality preference order.
 *  4. Boost cached models to avoid lengthy downloads.
 *  5. Return the best match.
 */
import os from "os";

// Chat models ranked by quality for domain Q&A tasks (best first).
const QUALITY_RANK = [
  "qwen2.5-7b",
  "qwen2.5-14b",
  "phi-4",
  "gpt-oss-20b",
  "mistral-7b-v0.2",
  "phi-4-mini-reasoning",
  "phi-3.5-mini",
  "phi-3-mini-128k",
  "phi-3-mini-4k",
  "qwen2.5-1.5b",
  "qwen2.5-0.5b",
];

// Aliases to skip (not suited for domain Q&A chat)
const SKIP_ALIASES = new Set([
  "qwen2.5-coder-0.5b",
  "qwen2.5-coder-1.5b",
  "qwen2.5-coder-7b",
  "qwen2.5-coder-14b",
]);

/**
 * Pick the best chat model from the Foundry Local catalogue that
 * fits within the device's RAM budget.
 *
 * @param {object}  catalog        – FoundryLocalManager.catalog instance
 * @param {object}  [opts]
 * @param {number}  [opts.ramBudgetPercent=0.6] – fraction of total RAM
 * @param {number}  [opts.maxModelSizeMb=4096] – hard cap on model file size in MB
 * @param {string}  [opts.forceModel]          – bypass selection and use this alias
 * @returns {Promise<{model, reason: string}>}
 */
export async function selectBestModel(catalog, opts = {}) {
  const forceAlias = opts.forceModel || process.env.FOUNDRY_MODEL;
  if (forceAlias) {
    const model = await catalog.getModel(forceAlias);
    return { model, reason: `forced via ${opts.forceModel ? "config" : "FOUNDRY_MODEL env"}` };
  }

  const totalRamMb = os.totalmem() / (1024 * 1024);
  const budgetPercent = opts.ramBudgetPercent ?? 0.6;
  const budgetMb = totalRamMb * budgetPercent;
  const maxSizeMb = opts.maxModelSizeMb ?? 4096;

  console.log(
    `[ModelSelector] System RAM: ${(totalRamMb / 1024).toFixed(1)} GB  ` +
    `| Budget (${(budgetPercent * 100).toFixed(0)}%): ${(budgetMb / 1024).toFixed(1)} GB` +
    `  | Max model size: ${(maxSizeMb / 1024).toFixed(1)} GB`
  );

  const allModels = await catalog.getModels();

  // Filter to chat-completion models that fit within the RAM budget
  const candidates = [];
  for (const m of allModels) {
    // Use the public API: iterate model.variants and use variant.modelInfo
    const variant = m.variants.find(v => v.modelInfo?.task === "chat-completion");
    if (!variant) continue;
    const info = variant.modelInfo;
    if (SKIP_ALIASES.has(info.alias)) continue;
    if (info.fileSizeMb > budgetMb) {
      console.log(`[ModelSelector]   skip ${info.alias} (${(info.fileSizeMb / 1024).toFixed(1)} GB > RAM budget)`);
      continue;
    }
    if (info.fileSizeMb > maxSizeMb) {
      console.log(`[ModelSelector]   skip ${info.alias} (${(info.fileSizeMb / 1024).toFixed(1)} GB > max model size)`);
      continue;
    }
    candidates.push({ model: m, info });
  }

  if (candidates.length === 0) {
    throw new Error(
      "No chat model fits within the available RAM budget " +
      `(${(budgetMb / 1024).toFixed(1)} GB). ` +
      "Try increasing ramBudgetPercent or freeing memory."
    );
  }

  // Score each candidate: quality rank + cache bonus
  const scored = candidates.map(({ model, info }) => {
    const rankIndex = QUALITY_RANK.indexOf(info.alias);
    const qualityScore = rankIndex >= 0
      ? (QUALITY_RANK.length - rankIndex) * 10
      : 1;
    const cacheBonus = model.isCached ? 5 : 0;
    const score = qualityScore + cacheBonus;
    return { model, info, score };
  });

  scored.sort((a, b) => b.score - a.score);

  const best = scored[0];
  const reason =
    `auto-selected (${(best.info.fileSizeMb / 1024).toFixed(1)} GB, ` +
    `${best.model.isCached ? "cached" : "will download"}, ` +
    `rank ${scored.indexOf(best) + 1}/${scored.length})`;

  console.log(`[ModelSelector] Selected: ${best.info.alias} – ${reason}`);
  return { model: best.model, reason };
}
