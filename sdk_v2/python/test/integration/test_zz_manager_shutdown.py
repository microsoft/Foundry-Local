# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Manager shutdown signalling.

Named ``test_zz_...`` so it sorts last alphabetically — calling
``shutdown()`` flips a one-way flag on the singleton native manager and
should not run before tests that exercise other manager-driven work.

.. warning::
   **Fragile ordering invariant.** This test mutates the session-scoped
   ``FoundryLocalManager`` singleton in a non-reversible way. It only works
   because:

   1. pytest collects integration tests in alphabetical filename order, so
      ``test_zz_*.py`` runs after every other ``test_*.py``.
   2. Together with :file:`test_zz_singleton_recreate.py`, these are the
      only two files that mutate the singleton.

   If a future test file is added with a name that sorts after ``test_zz_``
   (or uses ``pytest-randomly`` / ``pytest-ordering``), this invariant breaks
   silently and the new test will run against a shut-down manager.

   The principled fix is subprocess isolation (``pytest-forked`` with
   ``@pytest.mark.forked``) or a separate CI invocation for singleton-
   lifecycle tests. Defer until a third lifecycle test exists or the
   collection order changes.
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
