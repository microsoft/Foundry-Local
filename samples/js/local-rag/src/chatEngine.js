/**
 * Foundry Local chat engine.
 * Uses the Foundry Local SDK to discover, load, and run inference
 * on a local model. Performs RAG retrieval and generates responses.
 * Selects the hardware-optimised model variant automatically and
 * reports download/load progress via a status callback.
 */
import { FoundryLocalManager } from "foundry-local-sdk";
import { VectorStore } from "./vectorStore.js";
import { config } from "./config.js";
import { SYSTEM_PROMPT, SYSTEM_PROMPT_COMPACT } from "./prompts.js";

export class ChatEngine {
  constructor() {
    this.chatClient = null;
    this.model = null;
    this.store = null;
    this.compactMode = false;
    this.modelAlias = null;
    /** @type {(status: {phase: string, message: string, progress?: number}) => void} */
    this._statusCallback = null;
  }

  /** Register a callback that receives init status updates for the UI. */
  onStatus(callback) {
    this._statusCallback = callback;
  }

  _emitStatus(phase, message, progress) {
    const status = { phase, message, ...(progress !== undefined && { progress }) };
    console.log(`[ChatEngine] ${message}`);
    if (this._statusCallback) this._statusCallback(status);
  }

  /**
   * Initialize the engine: create Foundry Local manager, discover and load
   * the best model variant for this hardware, and open the vector store.
   */
  async init() {
    this._emitStatus("init", "Initializing Foundry Local SDK...");

    // Create the manager (requires appName)
    const manager = FoundryLocalManager.create({ appName: "gas-field-local-rag" });
    const catalog = manager.catalog;

    this._emitStatus("catalog", "Discovering available models...");
    this.model = await catalog.getModel(config.model);
    this.modelAlias = this.model.alias;

    // The SDK auto-selects the best variant for this hardware (GPU > NPU > CPU)
    this._emitStatus("variant", `Selected model: ${this.modelAlias}`);

    // Download the model if not already cached, with progress reporting
    if (!this.model.isCached) {
      this._emitStatus("download", `Downloading ${this.modelAlias}... This may take a few minutes on first run.`, 0);
      await this.model.download((progress) => {
        const pct = Math.round(progress);
        this._emitStatus("download", `Downloading ${this.modelAlias}... ${pct}%`, progress / 100);
      });
      this._emitStatus("download", `Download complete.`, 1);
    } else {
      this._emitStatus("cached", `Model ${this.modelAlias} is already cached.`);
    }

    // Load the model into memory
    this._emitStatus("loading", `Loading ${this.modelAlias} into memory...`);
    await this.model.load();

    // Create the native chat client with performance settings pre-configured
    this.chatClient = this.model.createChatClient();
    this.chatClient.settings.temperature = 0.1; // Low for deterministic, safety-critical responses
    this._emitStatus("ready", `Model ready: ${this.modelAlias}`);

    // Open the local vector store
    this.store = new VectorStore(config.dbPath);
    const count = this.store.count();
    this._emitStatus("ready", `Vector store ready: ${count} chunks indexed.`);

    if (count === 0) {
      console.warn("[ChatEngine] WARNING: No documents ingested. Run 'npm run ingest' first.");
    }
  }

  /** Expose the vector store for direct operations (e.g. upload ingestion). */
  getStore() {
    return this.store;
  }

  /**
   * Set compact mode for extreme latency / edge devices.
   */
  setCompactMode(enabled) {
    this.compactMode = enabled;
    console.log(`[ChatEngine] Compact mode: ${enabled ? "ON" : "OFF"}`);
  }

  /**
   * Retrieve relevant context from the local knowledge base.
   */
  retrieve(query) {
    const topK = this.compactMode ? Math.min(config.topK, 3) : config.topK;
    return this.store.search(query, topK);
  }

  /**
   * Format retrieved chunks into a context block for the prompt.
   */
  _buildContext(chunks) {
    if (chunks.length === 0) {
      return "No relevant documents found in local knowledge base.";
    }

    return chunks
      .map(
        (c, i) =>
          `--- Document ${i + 1}: ${c.title} [${c.category}] ---\n${c.content}`
      )
      .join("\n\n");
  }

  /**
   * Generate a response for a user query (non-streaming).
   */
  async query(userMessage, history = []) {
    // 1. Retrieve relevant chunks
    const chunks = this.retrieve(userMessage);
    const context = this._buildContext(chunks);

    // 2. Build messages array
    const systemPrompt = this.compactMode ? SYSTEM_PROMPT_COMPACT : SYSTEM_PROMPT;
    const messages = [
      { role: "system", content: systemPrompt },
      {
        role: "system",
        content: `Retrieved context from local knowledge base:\n\n${context}`,
      },
      ...history,
      { role: "user", content: userMessage },
    ];

    // 3. Call the local model via the native chat client
    this.chatClient.settings.maxTokens = this.compactMode ? 512 : 1024;
    const response = await this.chatClient.completeChat(messages);

    return {
      text: response.choices[0].message.content,
      sources: chunks.map((c) => ({
        title: c.title,
        category: c.category,
        docId: c.doc_id,
        score: Math.round(c.score * 100) / 100,
      })),
    };
  }

  /**
   * Generate a streaming response for a user query.
   * Returns an async iterable of text chunks.
   */
  async *queryStream(userMessage, history = []) {
    // 1. Retrieve relevant chunks
    const chunks = this.retrieve(userMessage);
    const context = this._buildContext(chunks);

    // 2. Build messages array
    const systemPrompt = this.compactMode ? SYSTEM_PROMPT_COMPACT : SYSTEM_PROMPT;
    const messages = [
      { role: "system", content: systemPrompt },
      {
        role: "system",
        content: `Retrieved context from local knowledge base:\n\n${context}`,
      },
      ...history,
      { role: "user", content: userMessage },
    ];

    // 3. Stream from the local model via the SDK's callback-based streaming
    this.chatClient.settings.maxTokens = this.compactMode ? 512 : 1024;

    // Buffer extracted content strings in a compacting queue so memory tracks unread data.
    const textChunks = [];
    let head = 0;
    let resolve;
    let done = false;

    const notify = () => {
      if (resolve) {
        const wake = resolve;
        resolve = null;
        wake();
      }
    };

    const compactTextChunks = () => {
      if (head > 0 && head * 2 >= textChunks.length) {
        textChunks.splice(0, head);
        head = 0;
      }
    };

    const streamPromise = this.chatClient.completeStreamingChat(messages, (chunk) => {
      const content = chunk.choices?.[0]?.delta?.content;
      if (content) {
        textChunks.push(content);
        notify();
      }
    }).then(() => {
      done = true;
      notify();
    });

    // Yield sources metadata first
    yield {
      type: "sources",
      data: chunks.map((c) => ({
        title: c.title,
        category: c.category,
        docId: c.doc_id,
        score: Math.round(c.score * 100) / 100,
      })),
    };

    // Yield text chunks from the SDK streaming callback buffer
    while (!done || head < textChunks.length) {
      if (head >= textChunks.length && !done) {
        await new Promise((r) => { resolve = r; });
      }
      while (head < textChunks.length) {
        const text = textChunks[head++];
        compactTextChunks();
        yield { type: "text", data: text };
      }
    }

    // Ensure the stream promise resolves cleanly
    await streamPromise;
  }

  close() {
    if (this.model) {
      this.model.unload().catch(() => {});
    }
    if (this.store) this.store.close();
  }
}
