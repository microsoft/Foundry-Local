/**
 * Context module for Context-Augmented Generation (CAG).
 * Reads all domain documents from the docs/ folder at startup
 * and provides them as pre-loaded, structured context blocks.
 *
 * Unlike RAG (which retrieves chunks at query time), CAG injects
 * the full domain knowledge into the prompt upfront — no vector
 * search, no embeddings, no retrieval step.
 */
import fs from "fs";
import path from "path";
import { config } from "./config.js";

const STOP_WORDS = new Set([
  "about", "after", "before", "could", "field", "from", "have",
  "into", "local", "mode", "need", "should", "that", "them",
  "there", "these", "this", "what", "when", "with", "would", "your",
]);

function getDocContent(doc) {
  return (doc.content ?? doc.body ?? "").trim();
}

function normalize(text) {
  return String(text || "").toLowerCase();
}

function tokenize(text) {
  return normalize(text)
    .split(/[^a-z0-9]+/)
    .filter((term) => term.length > 2 && !STOP_WORDS.has(term));
}

function uniqueTerms(text) {
  return [...new Set(tokenize(text))];
}

function trimToLength(text, maxLength) {
  if (!text || text.length <= maxLength) return text;
  const slice = text.slice(0, maxLength);
  const lastBreak = Math.max(slice.lastIndexOf("\n"), slice.lastIndexOf(". "));
  return `${slice.slice(0, lastBreak > 200 ? lastBreak : maxLength).trim()}\n...`;
}

function splitSections(content) {
  const lines = content.split("\n");
  const sections = [];
  let heading = "Overview";
  let bodyLines = [];

  const pushSection = () => {
    const body = bodyLines.join("\n").trim();
    if (!heading && !body) return;
    sections.push({
      heading,
      body,
      text: [heading, body].filter(Boolean).join("\n"),
      normalizedHeading: normalize(heading),
      normalizedBody: normalize(body),
    });
  };

  for (const line of lines) {
    if (/^#{1,3}\s+/.test(line)) {
      pushSection();
      heading = line.trim();
      bodyLines = [];
      continue;
    }
    bodyLines.push(line);
  }
  pushSection();
  return sections.filter((s) => s.body || s.heading !== "Overview");
}

function extractCompactContent(content) {
  const lines = content.split("\n");
  const keyLines = [];
  let inSafety = false;
  let inProcedure = false;

  for (const line of lines) {
    if (/^##\s*(safety|warning)/i.test(line)) {
      inSafety = true;
      inProcedure = false;
      keyLines.push(line);
    } else if (/^##\s*procedure/i.test(line)) {
      inProcedure = true;
      inSafety = false;
      keyLines.push(line);
    } else if (/^##\s/.test(line)) {
      inSafety = false;
      inProcedure = false;
    } else if (inSafety || inProcedure) {
      keyLines.push(line);
    }
  }

  if (keyLines.length > 0) return keyLines.join("\n").trim();
  return lines.filter((l) => l.trim()).slice(0, 5).join("\n");
}

function buildSectionText(section, maxLength) {
  const heading = section.heading === "Overview" ? "" : section.heading;
  return trimToLength([heading, section.body].filter(Boolean).join("\n"), maxLength);
}

function scoreSection(section, terms) {
  let score = 0;
  for (const term of terms) {
    if (section.normalizedHeading.includes(term)) score += 5;
    if (section.normalizedBody.includes(term)) score += 2;
  }
  return score;
}

function buildFocusedDocContext(doc, terms, { compact = false, maxCharsPerDoc = 1600, maxSections = 2 } = {}) {
  const titleLine = `--- ${doc.title} [${doc.id}] ---`;

  if (compact) {
    const compactContent = trimToLength(doc.compactContent || extractCompactContent(getDocContent(doc)), maxCharsPerDoc);
    return [titleLine, compactContent].join("\n");
  }

  const sections = Array.isArray(doc.sections) && doc.sections.length > 0
    ? doc.sections
    : splitSections(getDocContent(doc));

  if (terms.length === 0) {
    return [titleLine, trimToLength(getDocContent(doc), maxCharsPerDoc)].join("\n");
  }

  const ranked = sections
    .map((section) => ({ section, score: scoreSection(section, terms) }))
    .sort((a, b) => b.score - a.score);

  const positiveMatches = ranked.filter((e) => e.score > 0).slice(0, maxSections);
  const chosen = positiveMatches.length > 0 ? positiveMatches : ranked.slice(0, 1);

  let remaining = maxCharsPerDoc;
  const blocks = [];
  for (const entry of chosen) {
    if (remaining <= 0) break;
    const sectionText = buildSectionText(entry.section, remaining);
    if (!sectionText) continue;
    blocks.push(sectionText);
    remaining -= sectionText.length + 2;
  }

  const content = blocks.join("\n\n") || trimToLength(getDocContent(doc), maxCharsPerDoc);
  return [titleLine, content].join("\n");
}

export function buildSearchTerms(query) {
  return uniqueTerms(query);
}

export function findRelevantDocs(query, docs, maxDocs = 3) {
  const terms = buildSearchTerms(query);

  if (terms.length === 0) {
    return { docs: docs.slice(0, maxDocs), matched: false, terms };
  }

  const scored = docs.map((doc) => {
    const searchTitle = doc.searchTitle || normalize(doc.title);
    const searchCategory = doc.searchCategory || normalize(doc.category);
    const searchContent = doc.searchContent || normalize(getDocContent(doc));
    let score = 0;
    for (const term of terms) {
      if (searchTitle.includes(term)) score += 8;
      if (searchCategory.includes(term)) score += 3;
      if (searchContent.includes(term)) score += 1;
    }
    return { doc, score };
  });

  scored.sort((a, b) => b.score - a.score);
  const selected = scored.slice(0, maxDocs).filter((e) => e.score > 0);

  return {
    docs: selected.length > 0 ? selected.map((e) => e.doc) : docs.slice(0, maxDocs),
    matched: selected.length > 0,
    terms,
  };
}

/**
 * Parse YAML-like front-matter from a markdown document.
 */
export function parseFrontMatter(text) {
  const match = text.match(/^---\r?\n([\s\S]*?)\r?\n---\r?\n([\s\S]*)$/);
  if (!match) return { meta: {}, body: text };

  const meta = {};
  for (const line of match[1].split("\n")) {
    const idx = line.indexOf(":");
    if (idx > 0) {
      meta[line.slice(0, idx).trim()] = line.slice(idx + 1).trim();
    }
  }
  return { meta, body: match[2] };
}

/**
 * Load all markdown documents from the docs/ folder.
 */
export function loadDocuments() {
  const docsDir = config.docsDir;
  if (!fs.existsSync(docsDir)) {
    console.warn(`[Context] Docs directory not found: ${docsDir}`);
    return [];
  }

  const files = fs.readdirSync(docsDir).filter((f) => f.endsWith(".md")).sort();

  const docs = [];
  for (const file of files) {
    const raw = fs.readFileSync(path.join(docsDir, file), "utf-8");
    const { meta, body } = parseFrontMatter(raw);
    const content = body.trim();

    docs.push({
      id: meta.id || path.basename(file, ".md"),
      title: meta.title || file,
      category: meta.category || "General",
      content,
      compactContent: extractCompactContent(content),
      sections: splitSections(content),
      searchTitle: normalize(meta.title || file),
      searchCategory: normalize(meta.category || "General"),
      searchContent: normalize(content),
    });
  }

  return docs;
}

/**
 * Build the full domain context block from all loaded documents.
 */
export function buildDomainContext(docs) {
  if (docs.length === 0) return "";

  const categories = new Map();
  for (const doc of docs) {
    if (!categories.has(doc.category)) categories.set(doc.category, []);
    categories.get(doc.category).push(doc);
  }

  const sections = [];
  for (const [category, categoryDocs] of categories) {
    sections.push(`=== ${category} ===`);
    for (const doc of categoryDocs) {
      sections.push(`--- ${doc.title} [${doc.id}] ---`);
      sections.push(getDocContent(doc));
      sections.push("");
    }
  }

  return sections.join("\n");
}

/**
 * Build a compact context summary for edge/constrained devices.
 */
export function buildCompactContext(docs) {
  if (docs.length === 0) return "";

  const sections = [];
  for (const doc of docs) {
    sections.push(`--- ${doc.title} [${doc.id}] ---`);
    sections.push(doc.compactContent || extractCompactContent(getDocContent(doc)));
    sections.push("");
  }

  return sections.join("\n");
}

/**
 * Build context from a subset of selected documents.
 */
export function buildSelectedContext(docs, query = "", options = {}) {
  const terms = options.terms || buildSearchTerms(query);
  const sections = docs.map((doc) => buildFocusedDocContext(doc, terms, options));
  return sections.join("\n\n");
}

/**
 * Build a short document index listing all available topics.
 */
export function buildDocumentIndex(docs) {
  return docs.map((d) => `- ${d.title} [${d.id}]`).join("\n");
}

/**
 * Get a list of loaded documents (for the /api/context endpoint).
 */
export function listDocuments(docs) {
  return docs.map((d) => ({ id: d.id, title: d.title, category: d.category }));
}
