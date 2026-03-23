"""
Document Loader
────────────────
Load and chunk local text/markdown files for the retriever agent.
"""

from __future__ import annotations

import logging
from dataclasses import dataclass, field
from pathlib import Path

log = logging.getLogger(__name__)

SUPPORTED_EXTENSIONS = {".txt", ".md", ".markdown"}


@dataclass
class DocumentChunk:
    """A chunk of text from a source file."""
    source: str
    text: str
    index: int


@dataclass
class LoadedDocuments:
    """All loaded document chunks and metadata."""
    chunks: list[DocumentChunk] = field(default_factory=list)
    file_count: int = 0
    combined_text: str = ""


def load_documents(
    docs_path: str,
    max_chars_per_chunk: int = 2000,
) -> LoadedDocuments:
    """Load all supported files from *docs_path* and split into chunks."""
    folder = Path(docs_path)
    if not folder.is_dir():
        log.warning("Documents folder not found: %s", docs_path)
        return LoadedDocuments()

    chunks: list[DocumentChunk] = []
    file_count = 0

    for fp in sorted(folder.iterdir()):
        if fp.suffix.lower() not in SUPPORTED_EXTENSIONS:
            continue
        try:
            content = fp.read_text(encoding="utf-8")
        except Exception as exc:
            log.warning("Skipping %s: %s", fp.name, exc)
            continue

        file_count += 1

        # Split into chunks of roughly max_chars_per_chunk on line boundaries
        lines = content.splitlines(keepends=True)
        buf: list[str] = []
        buf_len = 0
        idx = 0

        for line in lines:
            if buf_len + len(line) > max_chars_per_chunk and buf:
                chunks.append(DocumentChunk(
                    source=fp.name,
                    text="".join(buf),
                    index=idx,
                ))
                idx += 1
                buf = []
                buf_len = 0
            buf.append(line)
            buf_len += len(line)

        if buf:
            chunks.append(DocumentChunk(
                source=fp.name,
                text="".join(buf),
                index=idx,
            ))

    combined = "\n\n".join(
        f"[{c.source} chunk {c.index}]\n{c.text}" for c in chunks
    )

    log.info("Loaded %d files → %d chunks", file_count, len(chunks))
    return LoadedDocuments(chunks=chunks, file_count=file_count, combined_text=combined)
