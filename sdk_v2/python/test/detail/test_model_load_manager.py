# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Tests for ModelLoadManager – mirrors modelLoadManager.test.ts."""

from __future__ import annotations

import pytest

from foundry_local_sdk.detail.model_load_manager import ModelLoadManager
from test.conftest import TEST_MODEL_ALIAS, IS_RUNNING_IN_CI, skip_in_ci


class TestModelLoadManagerCoreInterop:
    """ModelLoadManager tests using Core Interop (no external URL)."""

    def _get_model_id(self, catalog) -> str:
        """Resolve the variant ID for the test model alias."""
        cached = catalog.get_cached_models()
        variant = next((m for m in cached if m.alias == TEST_MODEL_ALIAS), None)
        assert variant is not None, f"{TEST_MODEL_ALIAS} should be cached"
        return variant.id

    def test_should_load_model(self, catalog, core_interop):
        """Load model via core interop and verify it appears in loaded list."""
        model_id = self._get_model_id(catalog)
        mlm = ModelLoadManager(core_interop)

        mlm.load(model_id)
        loaded = mlm.list_loaded()
        assert model_id in loaded

        # Cleanup
        mlm.unload(model_id)

    def test_should_unload_model(self, catalog, core_interop):
        """Load then unload model via core interop."""
        model_id = self._get_model_id(catalog)
        mlm = ModelLoadManager(core_interop)

        mlm.load(model_id)
        loaded = mlm.list_loaded()
        assert model_id in loaded

        mlm.unload(model_id)
        loaded = mlm.list_loaded()
        assert model_id not in loaded

    def test_should_list_loaded_models(self, catalog, core_interop):
        """list_loaded() should return an array containing the loaded model."""
        model_id = self._get_model_id(catalog)
        mlm = ModelLoadManager(core_interop)

        mlm.load(model_id)
        loaded = mlm.list_loaded()

        assert isinstance(loaded, list)
        assert model_id in loaded

        # Cleanup
        mlm.unload(model_id)


class TestModelLoadManagerExternalService:
    """ModelLoadManager tests using external web service URL (skipped in CI)."""

    @skip_in_ci
    def test_should_load_and_unload_via_external_service(self, manager, catalog, core_interop):
        """Load/unload model through the web service endpoint."""
        cached = catalog.get_cached_models()
        variant = next((m for m in cached if m.alias == TEST_MODEL_ALIAS), None)
        assert variant is not None
        model_id = variant.id

        # Start web service
        try:
            manager.start_web_service()
        except Exception as e:
            pytest.skip(f"Failed to start web service: {e}")

        urls = manager.urls
        if not urls or len(urls) == 0:
            pytest.skip("Web service started but no URLs returned")

        service_url = urls[0]

        try:
            # Setup: load via core interop
            setup_mlm = ModelLoadManager(core_interop)
            setup_mlm.load(model_id)
            loaded = setup_mlm.list_loaded()
            assert model_id in loaded

            # Unload via external service
            ext_mlm = ModelLoadManager(core_interop, service_url)
            ext_mlm.unload(model_id)

            # Verify via core interop
            loaded = setup_mlm.list_loaded()
            assert model_id not in loaded
        finally:
            try:
                manager.stop_web_service()
            except Exception:
                pass

    @skip_in_ci
    def test_should_list_loaded_via_external_service(self, manager, catalog, core_interop):
        """list_loaded() through the web service endpoint should match core interop."""
        cached = catalog.get_cached_models()
        variant = next((m for m in cached if m.alias == TEST_MODEL_ALIAS), None)
        assert variant is not None
        model_id = variant.id

        try:
            manager.start_web_service()
        except Exception as e:
            pytest.skip(f"Failed to start web service: {e}")

        urls = manager.urls
        if not urls or len(urls) == 0:
            pytest.skip("Web service started but no URLs returned")

        service_url = urls[0]

        try:
            # Setup: load via core
            setup_mlm = ModelLoadManager(core_interop)
            setup_mlm.load(model_id)

            # Verify via external service
            ext_mlm = ModelLoadManager(core_interop, service_url)
            loaded = ext_mlm.list_loaded()
            assert isinstance(loaded, list)
            assert model_id in loaded

            # Cleanup
            setup_mlm.unload(model_id)
        finally:
            try:
                manager.stop_web_service()
            except Exception:
                pass
