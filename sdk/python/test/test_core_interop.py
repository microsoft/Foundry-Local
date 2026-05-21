# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Tests for CoreInterop callback helpers."""

from __future__ import annotations

import ctypes
import threading

from foundry_local_sdk.detail.core_interop import CallbackHelper, CancelledException


class TestCoreInterop:
    def test_callback_helper_returns_cancel_when_cancel_event_is_set(self):
        """Callback helper should return 1 without invoking Python callback when cancelled."""
        cancel_event = threading.Event()
        cancel_event.set()
        called = False

        def _callback(_chunk: str) -> None:
            nonlocal called
            called = True

        helper = CallbackHelper(_callback, cancel_event)
        helper_ref = ctypes.py_object(helper)
        helper_ptr = ctypes.cast(ctypes.pointer(helper_ref), ctypes.c_void_p)
        data = ctypes.create_string_buffer(b"50")

        result = CallbackHelper.callback(data, 2, helper_ptr)

        assert result == 1
        assert called is False
        assert isinstance(helper.exception, CancelledException)
