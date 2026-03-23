// Application configuration – all paths relative to project root
import { fileURLToPath } from "url";
import path from "path";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(__dirname, "..");

export const config = {
  // Model
  model: "phi-3.5-mini",

  // RAG
  docsDir: path.join(ROOT, "docs"),
  dbPath: path.join(ROOT, "data", "rag.db"),
  chunkSize: 200,       // tokens (approx) – kept small for NPU compatibility
  chunkOverlap: 25,     // tokens overlap between chunks
  topK: 3,              // number of chunks to retrieve – limited for NPU context window

  // Server
  port: 3000,
  host: "127.0.0.1",

  // UI
  publicDir: path.join(ROOT, "public"),
};
