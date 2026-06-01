# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""EP discovery and download/register lifecycle. Requires the manager singleton."""
from __future__ import annotations

from foundry_local_sdk.ep_types import EpInfo


class TestEpLifecycle:
    def test_discover_eps_returns_list(self, manager):
        eps = manager.discover_eps()
        assert isinstance(eps, list)
        for ep in eps:
            assert isinstance(ep, EpInfo)
