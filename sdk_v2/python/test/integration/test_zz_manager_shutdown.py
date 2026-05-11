# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Manager shutdown signalling.

Named ``test_zz_...`` so it sorts last alphabetically — calling
``shutdown()`` flips a one-way flag on the singleton native manager and
should not run before tests that exercise other manager-driven work.
"""
from __future__ import annotations


class TestManagerShutdown:
    def test_shutdown_sets_is_shutdown_requested(self, manager):
        assert manager.is_shutdown_requested() is False
        manager.shutdown()
        assert manager.is_shutdown_requested() is True

    def test_shutdown_is_idempotent(self, manager):
        # Previous test may already have called shutdown; calling again
        # must not raise and the flag must stay set.
        manager.shutdown()
        manager.shutdown()
        assert manager.is_shutdown_requested() is True
