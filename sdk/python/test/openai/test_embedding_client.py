# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Tests for EmbeddingClient – mirrors EmbeddingClientTests.cs."""

from __future__ import annotations

import math

import pytest

from ..conftest import EMBEDDING_MODEL_ALIAS


def _get_loaded_embedding_model(catalog):
    """Helper: ensure the embedding model is selected, loaded, and return Model."""
    cached = catalog.get_cached_models()
    assert len(cached) > 0

    cached_variant = next((m for m in cached if m.alias == EMBEDDING_MODEL_ALIAS), None)
    assert cached_variant is not None, f"{EMBEDDING_MODEL_ALIAS} should be cached"

    model = catalog.get_model(EMBEDDING_MODEL_ALIAS)
    assert model is not None

    model.select_variant(cached_variant)
    model.load()
    return model


class TestEmbeddingClient:
    """Embedding Client Tests."""

    def test_should_generate_embedding(self, catalog):
        """Basic embedding generation."""
        model = _get_loaded_embedding_model(catalog)
        try:
            embedding_client = model.get_embedding_client()
            assert embedding_client is not None

            response = embedding_client.generate_embedding(
                "The quick brown fox jumps over the lazy dog"
            )

            assert response is not None
            assert response.model is not None
            assert len(response.data) == 1
            assert response.data[0].index == 0
            assert len(response.data[0].embedding) == 1024

            print(f"Embedding dimension: {len(response.data[0].embedding)}")
            print(f"First value: {response.data[0].embedding[0]}")
            print(f"Last value: {response.data[0].embedding[-1]}")
        finally:
            model.unload()

    def test_should_generate_normalized_embedding(self, catalog):
        """Verify L2 norm is approximately 1.0."""
        model = _get_loaded_embedding_model(catalog)
        try:
            embedding_client = model.get_embedding_client()

            inputs = [
                "The quick brown fox jumps over the lazy dog",
                "Machine learning is a subset of artificial intelligence",
                "The capital of France is Paris",
            ]

            for input_text in inputs:
                response = embedding_client.generate_embedding(input_text)
                embedding = response.data[0].embedding

                assert len(embedding) == 1024

                norm = math.sqrt(sum(v * v for v in embedding))
                assert 0.99 <= norm <= 1.01, f"L2 norm {norm} not approximately 1.0"

                for val in embedding:
                    assert -1.0 <= val <= 1.0
        finally:
            model.unload()

    def test_should_produce_different_embeddings_for_different_inputs(self, catalog):
        """Different inputs should produce different embeddings."""
        model = _get_loaded_embedding_model(catalog)
        try:
            embedding_client = model.get_embedding_client()

            response1 = embedding_client.generate_embedding("The quick brown fox")
            response2 = embedding_client.generate_embedding("The capital of France is Paris")

            emb1 = response1.data[0].embedding
            emb2 = response2.data[0].embedding

            assert len(emb1) == len(emb2)

            # Cosine similarity should not be 1.0
            dot = sum(a * b for a, b in zip(emb1, emb2))
            norm1 = math.sqrt(sum(a * a for a in emb1))
            norm2 = math.sqrt(sum(b * b for b in emb2))
            cosine_similarity = dot / (norm1 * norm2)
            assert cosine_similarity < 0.99
        finally:
            model.unload()

    def test_should_produce_same_embedding_for_same_input(self, catalog):
        """Same input should produce identical embeddings."""
        model = _get_loaded_embedding_model(catalog)
        try:
            embedding_client = model.get_embedding_client()

            response1 = embedding_client.generate_embedding("Deterministic embedding test")
            response2 = embedding_client.generate_embedding("Deterministic embedding test")

            emb1 = response1.data[0].embedding
            emb2 = response2.data[0].embedding

            for i in range(len(emb1)):
                assert emb1[i] == emb2[i]
        finally:
            model.unload()

    def test_should_raise_for_empty_input(self, catalog):
        """Empty input should raise ValueError."""
        model = _get_loaded_embedding_model(catalog)
        try:
            embedding_client = model.get_embedding_client()

            with pytest.raises(ValueError):
                embedding_client.generate_embedding("")
        finally:
            model.unload()
