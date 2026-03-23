// Application configuration – all paths relative to project root
import { fileURLToPath } from "url";
import path from "path";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(__dirname, "..");

export const config = {
  // Model – set FOUNDRY_MODEL to force a specific alias (e.g. "phi-3.5-mini").
  // When left empty the app auto-selects the best model for the device.
  model: process.env.FOUNDRY_MODEL || "",

  // Maximum fraction of total system RAM the model may occupy (0–1).
  ramBudgetPercent: parseFloat(process.env.RAM_BUDGET) || 0.6,

  // Maximum model file size in MB. Models larger than this are skipped
  // even if they fit in the RAM budget. Keeps CPU inference practical.
  // Set MAX_MODEL_MB to override (e.g. MAX_MODEL_MB=10240 for 10 GB).
  maxModelSizeMb: parseInt(process.env.MAX_MODEL_MB, 10) || 8192,

  // Context (CAG)
  docsDir: path.join(ROOT, "docs"),

  // Maximum number of documents injected per query. All documents are
  // pre-loaded at startup but only the most relevant ones are included
  // in each prompt to keep context small enough for CPU inference.
  maxContextDocs: parseInt(process.env.MAX_CONTEXT_DOCS, 10) || 3,

  // Server
  port: parseInt(process.env.PORT, 10) || 3000,
  host: "127.0.0.1",

  // UI
  publicDir: path.join(ROOT, "public"),
};
