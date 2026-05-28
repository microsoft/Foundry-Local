# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Web-service lifecycle and EP discovery integration tests.

Exercises ``FoundryLocalManager.start_web_service`` / ``stop_web_service`` and the EP discovery / registration
entry points. No model is required — these tests run against the session-scoped manager fixture.
"""
from __future__ import annotations

import pytest

from foundry_local_sdk.ep_types import EpDownloadResult, EpInfo
from foundry_local_sdk.exception import FoundryLocalException


# ---------------------------------------------------------------------------
# Web service lifecycle
# ---------------------------------------------------------------------------

class TestWebServiceLifecycle:
    def test_start_web_service_populates_urls(self, manager):
        manager.start_web_service()
        try:
            assert isinstance(manager.urls, list)
            assert len(manager.urls) > 0
            for url in manager.urls:
                assert isinstance(url, str)
                assert url.startswith("http://")
        finally:
            manager.stop_web_service()
        assert manager.urls is None

    def test_stop_without_start_raises(self, manager):
        # Make sure no prior test left the service running.
        if manager.urls is not None:
            manager.stop_web_service()
        assert manager.urls is None

        with pytest.raises(FoundryLocalException) as exc_info:
            manager.stop_web_service()
        assert "not running" in str(exc_info.value).lower()

    def test_start_stop_start_cycle(self, manager):
        try:
            manager.start_web_service()
            assert manager.urls and len(manager.urls) > 0

            manager.stop_web_service()
            assert manager.urls is None

            manager.start_web_service()
            assert manager.urls and len(manager.urls) > 0
        finally:
            if manager.urls is not None:
                manager.stop_web_service()


# ---------------------------------------------------------------------------
# EP discovery
# ---------------------------------------------------------------------------

class TestEpDiscovery:
    def test_discover_eps_returns_list(self, manager):
        eps = manager.discover_eps()
        assert isinstance(eps, list)
        for ep in eps:
            assert isinstance(ep, EpInfo)
            assert isinstance(ep.name, str)
            assert ep.name != ""
            assert isinstance(ep.is_registered, bool)

    def test_download_and_register_eps_empty_list_is_noop(self, manager):
        result = manager.download_and_register_eps([])
        assert isinstance(result, EpDownloadResult)
        assert result.success is True
        assert result.status == "Completed"
        assert result.registered_eps == []
        assert result.failed_eps == []
