# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Tests for CoreInterop callback helpers."""

from __future__ import annotations

import ctypes
import threading
import time

import pytest

from foundry_local_sdk.detail.core_interop import (
    CallbackHelper,
    CancelledException,
    CoreInterop,
    InteropRequest,
    ResponseBuffer,
)
from foundry_local_sdk.exception import FoundryLocalException


class FakeCoreLibrary:
    def __init__(self):
        self.calls: list[str] = []
        self.buffers = []

    def create_cancellation_context(self):
        self.calls.append("create")
        return 123

    def cancel_cancellation_context(self, context_id):
        self.calls.append(f"cancel:{context_id}")
        return 0

    def release_cancellation_context(self, context_id):
        self.calls.append(f"release:{context_id}")
        return 0

    def execute_command(self, _req, resp):
        self.calls.append("execute")
        self._set_response(resp, data=b"ok")

    def execute_command_cancellable(self, _req, resp, context_id):
        self.calls.append(f"execute_cancellable:{context_id}")
        self.cancel_event.set()
        time.sleep(0.05)
        self._set_response(resp, error=b"Operation was cancelled by user")

    def free_response(self, _resp):
        self.calls.append("free")

    def _set_response(self, resp_arg, data: bytes | None = None, error: bytes | None = None):
        resp = ctypes.cast(resp_arg, ctypes.POINTER(ResponseBuffer)).contents
        if data is not None:
            data_buffer = ctypes.create_string_buffer(data)
            self.buffers.append(data_buffer)
            resp.Data = ctypes.cast(data_buffer, ctypes.c_void_p).value
            resp.DataLength = len(data)
        if error is not None:
            error_buffer = ctypes.create_string_buffer(error)
            self.buffers.append(error_buffer)
            resp.Error = ctypes.cast(error_buffer, ctypes.c_void_p).value
            resp.ErrorLength = len(error)


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

    def test_execute_command_uses_cancellable_context_when_cancel_event_fires(self):
        fake_lib = FakeCoreLibrary()
        fake_lib.cancel_event = threading.Event()
        original_lib = CoreInterop._flcore_library
        original_available = CoreInterop._cancellable_commands_available
        CoreInterop._flcore_library = fake_lib
        CoreInterop._cancellable_commands_available = True
        interop = object.__new__(CoreInterop)

        try:
            with pytest.raises(FoundryLocalException, match="Operation cancelled"):
                interop.execute_command(
                    "chat_completions",
                    InteropRequest(params={"OpenAICreateRequest": "{}"}),
                    fake_lib.cancel_event,
                )
        finally:
            CoreInterop._flcore_library = original_lib
            CoreInterop._cancellable_commands_available = original_available

        assert fake_lib.calls == [
            "create",
            "execute_cancellable:123",
            "cancel:123",
            "release:123",
            "free",
        ]

    def test_execute_command_falls_back_when_cancellable_context_is_unavailable(self):
        fake_lib = FakeCoreLibrary()
        original_lib = CoreInterop._flcore_library
        original_available = CoreInterop._cancellable_commands_available
        CoreInterop._flcore_library = fake_lib
        CoreInterop._cancellable_commands_available = False
        interop = object.__new__(CoreInterop)

        try:
            response = interop.execute_command(
                "get_model_list",
                InteropRequest(params={}),
                threading.Event(),
            )
        finally:
            CoreInterop._flcore_library = original_lib
            CoreInterop._cancellable_commands_available = original_available

        assert response.data == "ok"
        assert fake_lib.calls == ["execute", "free"]
