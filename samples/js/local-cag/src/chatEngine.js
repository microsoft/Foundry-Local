/**
 * Foundry Local chat engine – Context-Aware Generation (CAG).
 * Uses the Foundry Local SDK (native bindings) to run inference
 * directly in-process, with no HTTP round-trips to a local server.
 *
 * Architecture: CAG injects the full domain knowledge base into the
 * system prompt at startup. No vector search, no embeddings, no
 * retrieval step at query time.
 */
import { FoundryLocalManager } from "foundry-local-sdk";
import { config } from "./config.js";
import { selectBestModel } from "./modelSelector.js";
import { SYSTEM_PROMPT, SYSTEM_PROMPT_COMPACT } from "./prompts.js";
import {
  loadDocuments,
  buildDomainContext,
  buildCompactContext,
  findRelevantDocs,
  buildSelectedContext,
  buildDocumentIndex,
  listDocuments,
} from "./context.js";

export class ChatEngine {
  constructor() {
    this.chatClient = null;
    this.modelAlias = null;
    this.compactMode = false;
    this.docs = [];
    this.domainContext = "";
    this.compactContext = "";
    this.docIndex = "";
  }

  /**
   * Initialise the engine: load domain context, start Foundry Local, load model.
   * @param {function} [onProgress] – callback receiving { stage, message, progress?, model? }
   */
  async init(onProgress = () => {}) {
    // 1. Pre-load all domain documents into memory
    onProgress({ stage: "context", message: "Loading domain documents..." });
    console.log("[ChatEngine] Loading domain context...");
    this.docs = loadDocuments();
    this.domainContext = buildDomainContext(this.docs);
    this.compactContext = buildCompactContext(this.docs);
    this.docIndex = buildDocumentIndex(this.docs);
    console.log(
      `[ChatEngine] Context loaded: ${this.docs.length} documents (${this.domainContext.length} chars).`
    );
    onProgress({ stage: "context", message: `Loaded ${this.docs.length} domain documents` });

    // 2. Initialise Foundry Local SDK (native bindings, no CLI)
    onProgress({ stage: "sdk", message: "Initialising Foundry Local SDK..." });
    console.log("[ChatEngine] Initialising Foundry Local SDK...");
    const manager = FoundryLocalManager.create({ appName: "gas-field-cag" });

    // 3. Select the best model for this device (or use the forced alias)
    onProgress({ stage: "selecting", message: "Selecting best model for this device..." });
    const { model, reason } = await selectBestModel(manager.catalog, {
      forceModel: config.model || undefined,
      ramBudgetPercent: config.ramBudgetPercent,
      maxModelSizeMb: config.maxModelSizeMb,
    });
    this.selectionReason = reason;
    onProgress({ stage: "selected", message: `Selected model: ${model.alias}`, model: model.alias });

    // 4. Download model if not cached
    if (!model.isCached) {
      console.log(`[ChatEngine] Downloading model ${model.alias}...`);
      onProgress({ stage: "downloading", message: `Downloading ${model.alias}...`, progress: 0, model: model.alias });
      await model.download((progress) => {
        process.stdout.write(`\r[ChatEngine] Download: ${progress.toFixed(0)}%`);
        onProgress({ stage: "downloading", message: `Downloading ${model.alias}...`, progress, model: model.alias });
      });
      console.log("");
    } else {
      onProgress({ stage: "cached", message: `${model.alias} is already cached`, model: model.alias });
    }

    // 5. Load model into memory
    onProgress({ stage: "loading", message: `Loading ${model.alias} into memory...`, model: model.alias });
    console.log(`[ChatEngine] Loading model ${model.alias} into memory...`);
    await model.load();
    this.modelAlias = model.alias;
    console.log(`[ChatEngine] Model loaded: ${model.id} (${model.alias})`);

    // 6. Create a ChatClient for direct in-process inference
    this.chatClient = model.createChatClient();
    this.chatClient.settings.temperature = 0.1;
    console.log("[ChatEngine] ChatClient ready (in-process inference).");
    onProgress({ stage: "ready", message: "Ready", model: model.alias });
  }

  /**
   * Get the list of loaded domain documents.
   */
  getDocuments() {
    return listDocuments(this.docs);
  }

  /**
   * Set compact mode for extreme latency / edge devices.
   */
  setCompactMode(enabled) {
    this.compactMode = enabled;
    console.log(`[ChatEngine] Compact mode: ${enabled ? "ON" : "OFF"}`);
  }

  /**
   * Build the messages array with pre-loaded context injection.
   *
   * Prompt structure:
   *   System: role + behavioural rules
   *   System: full domain context (pre-loaded, not retrieved)
   *   ...conversation history...
   *   User: question
   */
  _buildMessages(userMessage, history = []) {
    const systemPrompt = this.compactMode
      ? SYSTEM_PROMPT_COMPACT
      : SYSTEM_PROMPT;

    const recentHistory = history
      .slice(-4)
      .filter((entry) => entry && typeof entry.content === "string" && entry.content.trim())
      .map((entry) => ({ role: entry.role, content: entry.content.trim() }));

    // Select only the most relevant documents for this query
    const { docs: relevant, matched, terms } = findRelevantDocs(
      userMessage,
      this.docs,
      config.maxContextDocs,
    );
    const context = this.compactMode
      ? buildCompactContext(relevant)
      : buildSelectedContext(relevant, userMessage, {
          terms,
          maxCharsPerDoc: 1600,
          maxSections: 2,
        });
    const contextEnvelope = matched
      ? `Relevant documents for this query:\n\n${context}`
      : `Available documents:\n${this.docIndex}\n\nRelevant documents for this query:\n\n${context}`;

    console.log(
      `[ChatEngine] Query context: ${relevant.length} docs ` +
      `(${context.length} chars) – ${relevant.map((d) => d.id).join(", ")}`,
    );

    return [
      { role: "system", content: systemPrompt },
      {
        role: "system",
        content: contextEnvelope,
      },
      ...recentHistory,
      { role: "user", content: userMessage },
    ];
  }

  /**
   * Generate a response for a user query (non-streaming).
   */
  async query(userMessage, history = []) {
    const messages = this._buildMessages(userMessage, history);

    this.chatClient.settings.maxTokens = this.compactMode ? 512 : 1024;
    const response = await this.chatClient.completeChat(messages);

    return {
      text: response.choices[0].message.content,
    };
  }

  /**
   * Generate a streaming response for a user query.
   * Returns an async iterable of text chunks.
   */
  async *queryStream(userMessage, history = []) {
    const messages = this._buildMessages(userMessage, history);

    this.chatClient.settings.maxTokens = this.compactMode ? 512 : 1024;

    // Collect streamed chunks via callback and yield them
    const chunks = [];
    let resolve;
    let done = false;

    const promise = this.chatClient
      .completeStreamingChat(messages, (chunk) => {
        const content = chunk.choices?.[0]?.delta?.content;
        if (content) {
          chunks.push(content);
          if (resolve) {
            const r = resolve;
            resolve = null;
            r();
          }
        }
      })
      .then(() => {
        done = true;
        if (resolve) {
          const r = resolve;
          resolve = null;
          r();
        }
      });

    let index = 0;
    while (!done || index < chunks.length) {
      if (index < chunks.length) {
        yield { type: "text", data: chunks[index++] };
      } else {
        await new Promise((r) => { resolve = r; });
      }
    }

    // Ensure the streaming promise settles
    await promise;
  }
}
