/**
 * Local vector store backed by SQLite.
 * Stores document chunks and their term-frequency vectors for offline RAG retrieval.
 *
 * Performance optimisations:
 * - Inverted index: maps terms -> chunk IDs for fast candidate filtering
 * - Row cache: parsed TF maps kept in memory to avoid JSON.parse on every query
 * - Prepared statements: reused across calls
 */
import Database from "better-sqlite3";
import path from "path";
import fs from "fs";
import { termFrequency, cosineSimilarity } from "./chunker.js";

export class VectorStore {
  constructor(dbPath) {
    // Ensure data directory exists
    const dir = path.dirname(dbPath);
    if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });

    this.db = new Database(dbPath);
    this.db.pragma("journal_mode = WAL");
    this._init();

    // In-memory caches for fast retrieval
    this._rowCache = null;   // Array of { id, doc_id, title, category, content, tf }
    this._invertedIndex = null; // Map<term, Set<rowIndex>>
  }

  _init() {
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS chunks (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        doc_id TEXT NOT NULL,
        title TEXT,
        category TEXT,
        chunk_index INTEGER NOT NULL,
        content TEXT NOT NULL,
        tf_json TEXT NOT NULL
      );
      CREATE INDEX IF NOT EXISTS idx_doc_id ON chunks(doc_id);
    `);

    // Prepare reusable statements
    this._stmtInsert = this.db.prepare(
      "INSERT INTO chunks (doc_id, title, category, chunk_index, content, tf_json) VALUES (?, ?, ?, ?, ?, ?)"
    );
    this._stmtAll = this.db.prepare("SELECT * FROM chunks");
    this._stmtCount = this.db.prepare("SELECT COUNT(*) as cnt FROM chunks");
    this._stmtListDocs = this.db.prepare(
      "SELECT doc_id, title, category, COUNT(*) as chunks FROM chunks GROUP BY doc_id ORDER BY title"
    );
    this._stmtDeleteDoc = this.db.prepare("DELETE FROM chunks WHERE doc_id = ?");
  }

  /** Invalidate in-memory caches (called after any mutation). */
  _invalidateCache() {
    this._rowCache = null;
    this._invertedIndex = null;
  }

  /** Build or return the in-memory row cache and inverted index. */
  _ensureCache() {
    if (this._rowCache) return;

    const rows = this._stmtAll.all();
    this._rowCache = rows.map((row) => {
      const tf = new Map(JSON.parse(row.tf_json));
      return { id: row.id, doc_id: row.doc_id, title: row.title, category: row.category, content: row.content, tf };
    });

    // Build inverted index: term -> set of row indices
    this._invertedIndex = new Map();
    for (let i = 0; i < this._rowCache.length; i++) {
      for (const term of this._rowCache[i].tf.keys()) {
        if (!this._invertedIndex.has(term)) {
          this._invertedIndex.set(term, new Set());
        }
        this._invertedIndex.get(term).add(i);
      }
    }
  }

  /** Remove all existing chunks (for fresh re-ingestion). */
  clear() {
    this.db.exec("DELETE FROM chunks");
    this._invalidateCache();
  }

  /** Insert a single chunk. */
  insert(docId, title, category, chunkIndex, content) {
    const tf = termFrequency(content);
    const tfJson = JSON.stringify([...tf]);
    this._stmtInsert.run(docId, title, category, chunkIndex, content, tfJson);
    this._invalidateCache();
  }

  /** Retrieve top-K most relevant chunks for a query. */
  search(query, topK = 5) {
    const queryTf = termFrequency(query);
    this._ensureCache();

    // Use inverted index to find candidate chunks that share at least one term
    const candidateIndices = new Set();
    for (const term of queryTf.keys()) {
      const indices = this._invertedIndex.get(term);
      if (indices) {
        for (const idx of indices) candidateIndices.add(idx);
      }
    }

    // Score only candidates instead of all rows
    const scored = [];
    for (const idx of candidateIndices) {
      const row = this._rowCache[idx];
      const score = cosineSimilarity(queryTf, row.tf);
      if (score > 0) {
        scored.push({ ...row, score, tf_json: undefined });
      }
    }

    scored.sort((a, b) => b.score - a.score);
    return scored.slice(0, topK);
  }

  /** Remove all chunks for a specific document. */
  removeByDocId(docId) {
    this._stmtDeleteDoc.run(docId);
    this._invalidateCache();
  }

  /** Get total chunk count. */
  count() {
    return this._stmtCount.get().cnt;
  }

  /** List distinct documents in the store. */
  listDocs() {
    return this._stmtListDocs.all();
  }

  close() {
    this.db.close();
  }
}
