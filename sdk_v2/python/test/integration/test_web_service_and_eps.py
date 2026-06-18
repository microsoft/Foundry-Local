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

from foundry_local_sdk.ep_types import EpInfo
from foundry_local_sdk.exception import FoundryLocalException


# ---------------------------------------------------------------------------
# Web service lifecycle
# ---------------------------------------------------------------------------

class TestWebServiceLifecycle:
    async def test_start_web_service_populates_urls(self, manager):
        await manager.start_web_service()
        try:
            assert isinstance(manager.urls, list)
            assert len(manager.urls) > 0
            for url in manager.urls:
                assert isinstance(url, str)
                assert url.startswith("http://")
        finally:
            await manager.stop_web_service()
        assert manager.urls is None

    async def test_stop_without_start_raises(self, manager):
        # Make sure no prior test left the service running.
        if manager.urls is not None:
            await manager.stop_web_service()
        assert manager.urls is None

        with pytest.raises(FoundryLocalException) as exc_info:
            await manager.stop_web_service()
        assert "not running" in str(exc_info.value).lower()

    async def test_start_stop_start_cycle(self, manager):
        try:
            await manager.start_web_service()
            assert manager.urls and len(manager.urls) > 0

            await manager.stop_web_service()
            assert manager.urls is None

            await manager.start_web_service()
            assert manager.urls and len(manager.urls) > 0
        finally:
            if manager.urls is not None:
                await manager.stop_web_service()


# ---------------------------------------------------------------------------
# EP discovery
# ---------------------------------------------------------------------------

class TestEpDiscovery:
    async def test_discover_eps_returns_list(self, manager):
        eps = await manager.discover_eps()
        assert isinstance(eps, list)
        for ep in eps:
            assert isinstance(ep, EpInfo)
            assert isinstance(ep.name, str)
            assert ep.name != ""
            assert isinstance(ep.is_registered, bool)
