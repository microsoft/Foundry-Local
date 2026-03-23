/**
 * Express server – Gas Field CAG Application.
 * Serves the web UI and provides the /api/chat endpoint.
 * Fully offline, connects to Foundry Local on dynamic port.
 *
 * Uses Context-Aware Generation (CAG): all domain knowledge is
 * pre-loaded at startup and injected into the prompt — no retrieval,
 * no vector search, no embeddings.
 */
import express from "express";
import path from "path";
import { config } from "./config.js";
import { ChatEngine } from "./chatEngine.js";

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
app.use(express.static(config.publicDir));

// ── Chat engine instance ──
const engine = new ChatEngine();

// ── Initialisation state (broadcast to connected SSE clients) ──
let initState = { stage: "starting", message: "Starting up..." };
const statusClients = new Set();

function broadcastStatus(state) {
  initState = state;
  const payload = `data: ${JSON.stringify(state)}\n\n`;
  for (const client of statusClients) {
    client.write(payload);
  }
}

// ── API: Server-Sent Events for initialisation status ──
app.get("/api/status", (_req, res) => {
  res.setHeader("Content-Type", "text/event-stream");
  res.setHeader("Cache-Control", "no-cache");
  res.setHeader("Connection", "keep-alive");
  // Send current state immediately
  res.write(`data: ${JSON.stringify(initState)}\n\n`);
  statusClients.add(res);
  _req.on("close", () => statusClients.delete(res));
});

// ── Guard: reject chat requests while model is loading ──
function requireReady(_req, res, next) {
  if (initState.stage !== "ready") {
    return res.status(503).json({
      error: "Model is still loading. Please wait.",
      status: initState,
    });
  }
  next();
}

// ── API: Chat (non-streaming) ──
app.post("/api/chat", requireReady, async (req, res) => {
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
app.post("/api/chat/stream", requireReady, async (req, res) => {
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

// ── API: List pre-loaded context documents ──
app.get("/api/context", (_req, res) => {
  try {
    const docs = engine.getDocuments();
    res.json({ docs, count: docs.length });
  } catch (err) {
    console.error("[API] Context list error:", err.message);
    res.status(500).json({ error: "Failed to list context documents" });
  }
});

// ── API: Health check ──
app.get("/api/health", (_req, res) => {
  res.json({
    status: "ok",
    model: engine.modelAlias,
    modelSelection: engine.selectionReason,
    architecture: "CAG",
  });
});

// ── Fallback: serve index.html for SPA ──
app.get("*", (_req, res) => {
  res.sendFile(path.join(config.publicDir, "index.html"));
});

// ── Start server FIRST so the frontend can connect for status updates ──
async function start() {
  console.log("=== Gas Field CAG – Local Support Agent ===\n");

  const server = await new Promise((resolve, reject) => {
    const candidate = app.listen(config.port, config.host, () => {
      console.log(`[Server] Running at http://${config.host}:${config.port}`);
      console.log("[Server] Fully offline – no outbound connections.");
      console.log("[Server] Architecture: Context-Aware Generation (CAG)");
      console.log("[Server] Initialising engine – open the browser to see progress...\n");
      resolve(candidate);
    });

    candidate.once("error", (err) => {
      if (err.code === "EADDRINUSE") {
        console.error(`[Server] Port ${config.port} is already in use.`);
        console.error("[Server] Stop the other process or set a different PORT.");
      } else {
        console.error("[Server] Failed to start:", err.message);
      }
      reject(err);
    });
  });

  try {
    // Initialise engine AFTER the server is confirmed listening, broadcasting progress
    await engine.init(broadcastStatus);
    console.log("\n[Server] Engine ready – accepting requests.\n");
  } catch (err) {
    server.close();
    throw err;
  }
}

start().catch((err) => {
  console.error("Failed to start:", err);
  process.exit(1);
});
