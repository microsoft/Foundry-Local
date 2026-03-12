# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Tests for Catalog – mirrors catalog.test.ts."""

from __future__ import annotations

from test.conftest import TEST_MODEL_ALIAS


class TestCatalog:
    """Catalog Tests."""

    def test_should_initialize_with_catalog_name(self, catalog):
        """Catalog should expose a non-empty name string."""
        assert isinstance(catalog.name, str)
        assert len(catalog.name) > 0

    def test_should_list_models(self, catalog):
        """list_models() should return a non-empty list containing the test model."""
        models = catalog.list_models()
        assert isinstance(models, list)
        assert len(models) > 0

        # Verify test model is present
        aliases = {m.alias for m in models}
        assert TEST_MODEL_ALIAS in aliases

    def test_should_get_model_by_alias(self, catalog):
        """get_model() should return a Model whose alias matches."""
        model = catalog.get_model(TEST_MODEL_ALIAS)
        assert model is not None
        assert model.alias == TEST_MODEL_ALIAS

    def test_should_return_none_for_empty_alias(self, catalog):
        """get_model('') should return None (unknown alias)."""
        result = catalog.get_model("")
        assert result is None

    def test_should_return_none_for_unknown_alias(self, catalog):
        """get_model() with a random alias should return None."""
        result = catalog.get_model("definitely-not-a-real-model-alias-12345")
        assert result is None

    def test_should_get_cached_models(self, catalog):
        """get_cached_models() should return a list with at least the test model."""
        cached = catalog.get_cached_models()
        assert isinstance(cached, list)
        assert len(cached) > 0

        # At least the test model should be cached
        aliases = {m.alias for m in cached}
        assert TEST_MODEL_ALIAS in aliases

    def test_should_get_model_variant_by_id(self, catalog):
        """get_model_variant() with a valid ID should return the variant."""
        cached = catalog.get_cached_models()
        assert len(cached) > 0
        variant = cached[0]

        result = catalog.get_model_variant(variant.id)
        assert result is not None
        assert result.id == variant.id

    def test_should_return_none_for_empty_variant_id(self, catalog):
        """get_model_variant('') should return None."""
        result = catalog.get_model_variant("")
        assert result is None

    def test_should_return_none_for_unknown_variant_id(self, catalog):
        """get_model_variant() with a random ID should return None."""
        result = catalog.get_model_variant("definitely-not-a-real-model-id-12345")
        assert result is None
