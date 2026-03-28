# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Tests for FoundryLocalManager – mirrors foundryLocalManager.test.ts."""

from __future__ import annotations

import warnings


class _Response:
    def __init__(self, data=None, error=None):
        self.data = data
        self.error = error


class _FakeCoreInterop:
    def __init__(self, responses):
        self._responses = responses
        self.calls = []

    def execute_command(self, command_name, command_input=None):
        self.calls.append((command_name, command_input))
        return self._responses[command_name]


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

    def test_discover_eps_returns_ep_info(self, manager):
        original_core = manager._core_interop
        manager._core_interop = _FakeCoreInterop(
            {
                "discover_eps": _Response(
                    data='[{"Name":"CUDAExecutionProvider","IsRegistered":true}]',
                    error=None,
                )
            }
        )

        try:
            eps = manager.discover_eps()
        finally:
            manager._core_interop = original_core

        assert isinstance(eps, list)
        assert len(eps) == 1
        assert eps[0].name == "CUDAExecutionProvider"
        assert eps[0].is_registered is True

    def test_download_and_register_eps_returns_result(self, manager):
        original_core = manager._core_interop
        manager._core_interop = _FakeCoreInterop(
            {
                "download_and_register_eps": _Response(
                    data=(
                        '{"Success":true,"Status":"ok",'
                        '"RegisteredEps":["CUDAExecutionProvider"],"FailedEps":[]}'
                    ),
                    error=None,
                )
            }
        )

        try:
            result = manager.download_and_register_eps(["CUDAExecutionProvider"])
        finally:
            manager._core_interop = original_core

        assert result.success is True
        assert result.status == "ok"
        assert result.registered_eps == ["CUDAExecutionProvider"]
        assert result.failed_eps == []
