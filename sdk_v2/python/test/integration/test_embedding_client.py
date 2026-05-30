# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""End-to-end EmbeddingClient tests.

The bulk of embedding correctness (L2 normalization, batched-vs-single
parity) is covered in C++ — these focus on the Python wrapper:

- Single-input vs multi-input request shapes are accepted.
- Response is parsed into a typed ``CreateEmbeddingResponse`` even when
  the server omits the optional ``object`` and ``usage`` fields.
- Vector dimensions are consistent across inputs in the same batch.
- Distinct inputs produce distinct vectors.
"""
from __future__ import annotations
from openai.types import CreateEmbeddingResponse

import math

import pytest

pytest.importorskip("openai")


@pytest.fixture
def embedding_client(embedding_model):
    """Function-scoped EmbeddingClient.

    ``get_embedding_client()`` is a thin Python-side wrapper over the model handle;
    each call to ``generate_embedding(s)`` builds its own native ``EmbeddingsSession``
    internally. Function scope keeps tests isolated and signals that the client is
    cheap to create.
    """
    return embedding_model.get_embedding_client()


SAMPLE_INPUTS = [
    "The quick brown fox jumps over the lazy dog.",
    "Pack my box with five dozen liquor jugs.",
    "How vexingly quick daft zebras jump!",
]


class TestSingle:
    def test_returns_typed_response(self, embedding_client):
        resp = embedding_client.generate_embedding(SAMPLE_INPUTS[0])
        assert isinstance(resp, CreateEmbeddingResponse)

    def test_one_vector_for_one_input(self, embedding_client):
        resp = embedding_client.generate_embedding(SAMPLE_INPUTS[0])
        assert len(resp.data) == 1
        assert len(resp.data[0].embedding) > 0

    def test_empty_input_rejected_before_native_call(self, embedding_client):
        with pytest.raises(ValueError):
            embedding_client.generate_embedding("")

    def test_whitespace_only_rejected(self, embedding_client):
        with pytest.raises(ValueError):
            embedding_client.generate_embedding("   ")


class TestBatched:
    def test_one_vector_per_input(self, embedding_client):
        resp = embedding_client.generate_embeddings(SAMPLE_INPUTS)
        assert len(resp.data) == len(SAMPLE_INPUTS)

    def test_consistent_dimensions(self, embedding_client):
        resp = embedding_client.generate_embeddings(SAMPLE_INPUTS)
        dims = {len(d.embedding) for d in resp.data}
        assert len(dims) == 1, f"Expected uniform dim, got {dims}"

    def test_distinct_inputs_produce_distinct_vectors(self, embedding_client):
        resp = embedding_client.generate_embeddings(SAMPLE_INPUTS)
        v0 = list(resp.data[0].embedding)
        v1 = list(resp.data[1].embedding)
        # Cosine similarity well below 1 — distinct sentences shouldn't collide.
        dot = sum(a * b for a, b in zip(v0, v1))
        n0 = math.sqrt(sum(a * a for a in v0))
        n1 = math.sqrt(sum(a * a for a in v1))
        cos = dot / (n0 * n1) if n0 and n1 else 0.0
        assert cos < 0.999, f"Expected distinct vectors, got cosine sim {cos}"

    def test_empty_list_rejected(self, embedding_client):
        with pytest.raises(ValueError):
            embedding_client.generate_embeddings([])

    def test_empty_element_in_list_rejected(self, embedding_client):
        with pytest.raises(ValueError):
            embedding_client.generate_embeddings(["ok", ""])
