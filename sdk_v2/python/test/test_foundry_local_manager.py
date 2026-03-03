# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Tests for FoundryLocalManager – mirrors foundryLocalManager.test.ts."""

from __future__ import annotations


class TestFoundryLocalManager:
    """Foundry Local Manager Tests."""

    def test_should_initialize_successfully(self, manager):
        """Manager singleton should be non-None after initialize()."""
        assert manager is not None

    def test_should_return_catalog(self, manager):
        """Manager should expose a Catalog with a non-empty name."""
        catalog = manager.catalog
        assert catalog is not None
        assert isinstance(catalog.name, str)
        assert len(catalog.name) > 0
