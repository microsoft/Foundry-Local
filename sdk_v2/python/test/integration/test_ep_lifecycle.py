# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""EP discovery and download/register lifecycle. Requires the manager singleton."""
from __future__ import annotations

from foundry_local_sdk.ep_types import EpDownloadResult, EpInfo


class TestEpLifecycle:
    def test_discover_eps_returns_list(self, manager):
        eps = manager.discover_eps()
        assert isinstance(eps, list)
        for ep in eps:
            assert isinstance(ep, EpInfo)

    def test_download_and_register_eps_with_empty_names(self, manager):
        # Pass names=[] so the call is a no-op rather than triggering a full
        # cross-EP download in CI. Native still returns an EpDownloadResult.
        result = manager.download_and_register_eps(names=[])
        assert isinstance(result, EpDownloadResult)
