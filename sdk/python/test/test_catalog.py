# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Tests for Catalog – mirrors catalog.test.ts."""

from __future__ import annotations

import json

from foundry_local_sdk.catalog import Catalog
from foundry_local_sdk.detail.core_interop import Response

from .conftest import TEST_MODEL_ALIAS


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

    def test_should_resolve_latest_version_for_model_and_variant_inputs(self):
        """get_latest_version() should resolve latest variant and preserve Model input when already latest."""

        test_model_infos = [
            {
                "id": "test-model:3",
                "name": "test-model",
                "version": 3,
                "alias": "test-alias",
                "displayName": "Test Model",
                "providerType": "test",
                "uri": "test://model/3",
                "modelType": "ONNX",
                "runtime": {"deviceType": "CPU", "executionProvider": "CPUExecutionProvider"},
                "cached": False,
                "createdAt": 1700000003,
            },
            {
                "id": "test-model:2",
                "name": "test-model",
                "version": 2,
                "alias": "test-alias",
                "displayName": "Test Model",
                "providerType": "test",
                "uri": "test://model/2",
                "modelType": "ONNX",
                "runtime": {"deviceType": "CPU", "executionProvider": "CPUExecutionProvider"},
                "cached": False,
                "createdAt": 1700000002,
            },
            {
                "id": "test-model:1",
                "name": "test-model",
                "version": 1,
                "alias": "test-alias",
                "displayName": "Test Model",
                "providerType": "test",
                "uri": "test://model/1",
                "modelType": "ONNX",
                "runtime": {"deviceType": "CPU", "executionProvider": "CPUExecutionProvider"},
                "cached": False,
                "createdAt": 1700000001,
            },
        ]

        class _MockCoreInterop:
            def execute_command(self, command_name, command_input=None):
                if command_name == "get_catalog_name":
                    return Response(data="TestCatalog", error=None)
                if command_name == "get_model_list":
                    return Response(data=json.dumps(test_model_infos), error=None)
                if command_name == "get_cached_models":
                    return Response(data="[]", error=None)
                return Response(data=None, error=f"Unexpected command: {command_name}")

        class _MockModelLoadManager:
            def list_loaded(self):
                return []

        catalog = Catalog(_MockModelLoadManager(), _MockCoreInterop())

        model = catalog.get_model("test-alias")
        assert model is not None

        variants = model.variants
        assert len(variants) == 3

        latest_variant = variants[0]
        middle_variant = variants[1]
        oldest_variant = variants[2]

        assert latest_variant.id == "test-model:3"
        assert middle_variant.id == "test-model:2"
        assert oldest_variant.id == "test-model:1"

        result1 = catalog.get_latest_version(latest_variant)
        assert result1.id == "test-model:3"

        result2 = catalog.get_latest_version(middle_variant)
        assert result2.id == "test-model:3"

        result3 = catalog.get_latest_version(oldest_variant)
        assert result3.id == "test-model:3"

        model.select_variant(latest_variant)
        result4 = catalog.get_latest_version(model)
        assert result4 is model
