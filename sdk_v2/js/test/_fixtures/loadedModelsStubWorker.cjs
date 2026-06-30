"use strict";

// Worker-thread body for the loaded-models stub. Runs on its own event loop so
// it can serve requests even while the SDK's *synchronous* native load-state
// calls (getLoadedModels / isLoaded) block the test's main thread. See
// loadedModelsStub.ts for the contract.
const http = require("node:http");
const { workerData, parentPort } = require("node:worker_threads");

let loaded = Array.isArray(workerData && workerData.initialLoaded) ? workerData.initialLoaded.slice() : [];

const server = http.createServer((req, res) => {
  const path = (req.url || "").split("?")[0];

  if (req.method === "GET" && path === "/models/loaded") {
    res.writeHead(200, { "content-type": "application/json" });
    res.end(JSON.stringify(loaded));
    return;
  }

  if (req.method === "GET" && (path.startsWith("/models/load/") || path.startsWith("/models/unload/"))) {
    res.writeHead(200, { "content-type": "application/json" });
    res.end("{}");
    return;
  }

  res.writeHead(404, { "content-type": "text/plain" });
  res.end("not found");
});

if (parentPort) {
  parentPort.on("message", (msg) => {
    if (msg && msg.type === "setLoaded") {
      loaded = Array.isArray(msg.ids) ? msg.ids.slice() : [];
    } else if (msg && msg.type === "close") {
      // Force exit; libcurl may hold a keep-alive connection that would stall server.close().
      process.exit(0);
    }
  });
}

server.listen(0, "127.0.0.1", () => {
  if (parentPort) {
    parentPort.postMessage({ type: "listening", port: server.address().port });
  }
});
