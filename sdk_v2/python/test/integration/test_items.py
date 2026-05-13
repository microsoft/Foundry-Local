# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Item lifecycle and round-trip tests through the native item vtable.

These verify the Python wrapper invariants:

- Construction creates a native handle and Python owns it (``_owns=True``).
- ``add_item`` transfers ownership to the request — the wrapper must not
  release the handle on ``__del__`` afterwards.
- Field round-trips through native ``GetText`` / ``GetMessage`` / etc.
  preserve user data exactly (utf-8 strings, raw bytes).
- ``__del__`` on a partially-constructed item must not crash.
"""
from __future__ import annotations

import pytest

from foundry_local_sdk import (
    BytesItem,
    ItemType,
    MessageItem,
    MessageRole,
    Request,
    TextItem,
    TextItemType,
)
from foundry_local_sdk.item_queue import ItemQueue
from foundry_local_sdk.items import Item


@pytest.fixture(autouse=True)
def _require_native(native_api):
    # Pull the native fixture so every test in this file skips cleanly when
    # the cffi extension is not available.
    return native_api


class TestTextItem:
    def test_create_and_field_round_trip(self):
        t = TextItem("hello world")
        assert t.text == "hello world"
        assert t.type == TextItemType.DEFAULT
        assert t.item_type == ItemType.TEXT

    def test_openai_json_text_type(self):
        t = TextItem('{"k": 1}', TextItemType.OPENAI_JSON)
        assert t.type == TextItemType.OPENAI_JSON

    def test_unicode_round_trip(self):
        s = "héllo — 你好 — 🦀"
        assert TextItem(s).text == s

    def test_repr_contains_text(self):
        assert "hello" in repr(TextItem("hello"))

    def test_explicit_close_safe_to_call_twice(self):
        t = TextItem("x")
        t._close()
        t._close()  # must not raise


class TestMessageItem:
    def test_string_content_wraps_in_text_part(self):
        m = MessageItem.user("hi")
        assert m.role == MessageRole.USER
        parts = m.parts
        assert len(parts) == 1
        assert isinstance(parts[0], TextItem)
        assert parts[0].text == "hi"

    def test_role_factories_match_enum(self):
        assert MessageItem.system("x").role == MessageRole.SYSTEM
        assert MessageItem.user("x").role == MessageRole.USER
        assert MessageItem.assistant("x").role == MessageRole.ASSISTANT
        assert MessageItem.developer("x").role == MessageRole.DEVELOPER

    def test_empty_list_content_rejected(self):
        with pytest.raises(ValueError):
            MessageItem.user([])

    def test_optional_name_round_trip(self):
        m = MessageItem.user("hi", name="alice")
        assert m.name == "alice"

    def test_no_name_is_none(self):
        m = MessageItem.user("hi")
        assert m.name is None


class TestBytesItem:
    def test_round_trips_arbitrary_bytes(self):
        payload = bytes(range(256))
        b = BytesItem(payload)
        assert b.data == payload
        assert b.item_type == ItemType.BYTES

    def test_empty_payload(self):
        b = BytesItem(b"")
        assert b.data == b""

    def test_accepts_memoryview(self):
        ba = bytearray(b"abc")
        b = BytesItem(memoryview(ba))
        assert b.data == b"abc"


class TestOwnershipTransfer:
    def test_add_item_transfers_native_ownership_to_request(self):
        t = TextItem("payload")
        assert t._owns is True

        with Request() as req:
            req.add_item(t)

            # After transfer the wrapper no longer owns the native handle —
            # otherwise __del__ on the wrapper would double-free.
            assert t._owns is False

    def test_request_get_item_returns_non_owning_view(self):
        with Request() as req:
            req.add_item(TextItem("look"))
            view = req.get_item(0)
            # The view points at memory owned by the request, so `_owns` is False.
            assert view._owns is False
            assert isinstance(view, TextItem)
            assert view.text == "look"


class TestPartialConstructionSafety:
    def test_close_on_uninitialised_item_does_not_crash(self):
        # Build an item without running __init__ — simulates a subclass that
        # raised inside its own constructor before super().__init__ ran.
        obj = TextItem.__new__(TextItem)
        obj._close()  # must be a no-op, not an AttributeError or crash
        # __del__ likewise must be safe.
        del obj


class TestItemQueueIsItem:
    """ItemQueue : Item — mirrors the C++ struct hierarchy."""

    def test_item_queue_is_item_subclass(self):
        q = ItemQueue()
        try:
            assert isinstance(q, Item)
        finally:
            q._close()

    def test_item_queue_reports_queue_item_type(self):
        q = ItemQueue()
        try:
            assert q.item_type == ItemType.QUEUE
        finally:
            q._close()

    def test_add_item_accepts_queue_polymorphically(self):
        q = ItemQueue()
        try:
            with Request() as req:
                # No duck-typing branch needed — queue is an Item.
                req.add_item(q, transfer_ownership=False)
                # Caller retains ownership when transfer_ownership=False.
                assert q._owns is True
                # We can keep using the queue after handing it to the request.
                q.push(TextItem("streamed"))
                assert q.size == 1
        finally:
            q._close()
