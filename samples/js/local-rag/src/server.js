/**
 * Express server – Gas Field RAG Application.
 * Serves the web UI and provides the /api/chat endpoint.
 * Fully offline, connects to Foundry Local on dynamic port.
 */
import express from "express";
import path from "path";
import fs from "fs";
import { config } from "./config.js";
import { ChatEngine } from "./chatEngine.js";
import { parseFrontMatter, chunkText } from "./chunker.js";

const app = express();

// ── Security headers ──
app.use((_req, res, next) => {
  res.setHeader("X-Content-Type-Options", "nosniff");
  res.setHeader("X-Frame-Options", "DENY");
  res.setHeader("Referrer-Policy", "no-referrer");
  res.setHeader("Permissions-Policy", "camera=(), microphone=(), geolocation=()");
  res.setHeader(
    "Content-Security-Policy",
    "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' data:;"
  );
  next();
});

app.use(express.json({ limit: "1mb" }));
app.use(express.text({ type: "text/markdown", limit: "2mb" }));
app.use(express.static(config.publicDir));

// ── Chat engine instance ──
const engine = new ChatEngine();

// ── API: Chat (non-streaming) ──
app.post("/api/chat", async (req, res) => {
  try {
    const { message, history, compact } = req.body;
    if (!message || typeof message !== "string") {
      return res.status(400).json({ error: "message is required" });
    }

    if (compact !== undefined) engine.setCompactMode(!!compact);

    const result = await engine.query(
      message,
      Array.isArray(history) ? history : []
    );
    res.json(result);
  } catch (err) {
    console.error("[API] Error:", err.message);
    res.status(500).json({ error: "Internal server error" });
  }
});

// ── API: Chat (streaming via SSE) ──
app.post("/api/chat/stream", async (req, res) => {
  try {
    const { message, history, compact } = req.body;
    if (!message || typeof message !== "string") {
      return res.status(400).json({ error: "message is required" });
    }

    if (compact !== undefined) engine.setCompactMode(!!compact);

    res.setHeader("Content-Type", "text/event-stream");
    res.setHeader("Cache-Control", "no-cache");
    res.setHeader("Connection", "keep-alive");

    const stream = engine.queryStream(
      message,
      Array.isArray(history) ? history : []
    );

    for await (const chunk of stream) {
      res.write(`data: ${JSON.stringify(chunk)}\n\n`);
    }

    res.write("data: [DONE]\n\n");
    res.end();
  } catch (err) {
    console.error("[API] Stream error:", err.message);
    res.write(`data: ${JSON.stringify({ type: "error", data: "Internal server error" })}\n\n`);
    res.end();
  }
});

// ── API: Upload document ──
app.post("/api/upload", express.raw({ type: "*/*", limit: "2mb" }), async (req, res) => {
  try {
    const filename = req.headers["x-filename"];
    if (!filename || typeof filename !== "string") {
      return res.status(400).json({ error: "x-filename header is required" });
    }

    // Sanitise filename: allow only alphanumeric, hyphens, underscores, dots
    const safeName = path.basename(filename).replace(/[^a-zA-Z0-9._-]/g, "_");
    if (!safeName.endsWith(".md") && !safeName.endsWith(".txt")) {
      return res.status(400).json({ error: "Only .md and .txt files are accepted" });
    }

    const content = req.body.toString("utf-8");
    if (!content || content.length < 10) {
      return res.status(400).json({ error: "Document content is too short" });
    }

    // Save file to docs/ (verify path stays inside docsDir)
    const filePath = path.resolve(config.docsDir, safeName);
    if (!filePath.startsWith(path.resolve(config.docsDir))) {
      return res.status(400).json({ error: "Invalid filename" });
    }
    if (!fs.existsSync(config.docsDir)) {
      fs.mkdirSync(config.docsDir, { recursive: true });
    }
    fs.writeFileSync(filePath, content, "utf-8");

    // Chunk and ingest into vector store
    const { meta, body } = parseFrontMatter(content);
    const docId = meta.id || path.basename(safeName, path.extname(safeName));
    const title = meta.title || safeName;
    const category = meta.category || "Uploaded";

    // Remove previous version if re-uploading
    const store = engine.getStore();
    store.removeByDocId(docId);

    const chunks = chunkText(body, config.chunkSize, config.chunkOverlap);
    for (let i = 0; i < chunks.length; i++) {
      store.insert(docId, title, category, i, chunks[i]);
    }

    console.log(`[Upload] ${safeName} → ${chunks.length} chunk(s) ingested`);

    res.json({
      success: true,
      filename: safeName,
      docId,
      title,
      category,
      chunks: chunks.length,
      totalChunks: store.count(),
    });
  } catch (err) {
    console.error("[Upload] Error:", err.message);
    res.status(500).json({ error: "Upload failed" });
  }
});

// ── API: List documents ──
app.get("/api/docs", (_req, res) => {
  try {
    const docs = engine.getStore().listDocs();
    res.json({ docs });
  } catch (err) {
    console.error("[API] Docs list error:", err.message);
    res.status(500).json({ error: "Failed to list documents" });
  }
});

// ── API: Health check ──
let engineReady = false;
let lastStatus = { phase: "init", message: "Starting..." };

app.get("/api/health", (_req, res) => {
  res.json({ status: engineReady ? "ok" : "loading", model: config.model, ...lastStatus });
});

// ── API: Init status stream (SSE) — shows download/load progress to the UI ──
const statusClients = new Set();

app.get("/api/status", (_req, res) => {
  res.setHeader("Content-Type", "text/event-stream");
  res.setHeader("Cache-Control", "no-cache");
  res.setHeader("Connection", "keep-alive");

  // Send current status immediately
  res.write(`data: ${JSON.stringify(lastStatus)}\n\n`);

  if (engineReady) {
    res.write(`data: ${JSON.stringify({ phase: "ready", message: "Ready" })}\n\n`);
    res.end();
    return;
  }

  statusClients.add(res);
  _req.on("close", () => statusClients.delete(res));
});

function broadcastStatus(status) {
  lastStatus = status;
  for (const client of statusClients) {
    client.write(`data: ${JSON.stringify(status)}\n\n`);
    if (status.phase === "ready") {
      client.end();
    }
  }
  if (status.phase === "ready") statusClients.clear();
}

// ── Fallback: serve index.html for SPA ──
app.get("*", (_req, res) => {
  res.sendFile(path.join(config.publicDir, "index.html"));
});

// ── Start server ──
async function start() {
  console.log("=== Gas Field RAG – Local Support Agent ===\n");

  // Register status callback to relay progress to connected UI clients
  engine.onStatus((status) => broadcastStatus(status));

  // Start the HTTP server first so the UI is immediately accessible
  app.listen(config.port, config.host, () => {
    console.log(`[Server] UI available at http://${config.host}:${config.port}`);
    console.log("[Server] Initializing model in background...\n");
  });

  // Initialize the engine (downloads model if needed, loads it)
  await engine.init();
  engineReady = true;
  broadcastStatus({ phase: "ready", message: "Ready" });

  console.log("[Server] Fully offline – no outbound connections.\n");
}

start().catch((err) => {
  console.error("Failed to start:", err);
  broadcastStatus({ phase: "error", message: err.message || "Failed to start" });
  process.exit(1);
});
