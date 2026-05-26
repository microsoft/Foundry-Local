# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Unit test for the Session abstract-base guard."""
from __future__ import annotations

import pytest

from foundry_local_sdk.session import Session


class TestSessionIsAbstract:
    def test_direct_new_raises(self):
        with pytest.raises(TypeError, match="abstract base"):
            Session.__new__(Session)

    def test_direct_call_raises(self):
        # Bypasses __init__ entirely via the __new__ guard, so no real model needed.
        with pytest.raises(TypeError, match="abstract base"):
            Session(object())  # type: ignore[arg-type]
