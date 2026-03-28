// Application configuration – all paths relative to project root
import { fileURLToPath } from "url";
import path from "path";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(__dirname, "..");

export const config = {
  // Model
  model: process.env.FOUNDRY_MODEL || "phi-3.5-mini",

  // RAG
  docsDir: path.join(ROOT, "docs"),
  dbPath: path.join(ROOT, "data", "rag.db"),
  chunkSize: 200,       // tokens (approx) \u2013 kept small for NPU compatibility
  chunkOverlap: 25,     // tokens overlap between chunks
  topK: 3,              // number of chunks to retrieve \u2013 limited for NPU context window

  // Server
  port: parseInt(process.env.PORT, 10) || 3000,
  host: process.env.HOST || "127.0.0.1",

  // UI
  publicDir: path.join(ROOT, "public"),
};
