# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Direct ``Session`` tests — bypasses the OpenAI compat layer.

Verifies that ``ChatSession.process_request`` and the streaming generator
work against the typed Item / Request / Response wrappers. The OpenAI
clients exercise the same code paths but go through JSON; these tests
prove the typed entry points work too.
"""
from __future__ import annotations

import pytest

from foundry_local import (
    ChatSession,
    FinishReason,
    ItemType,
    MessageItem,
    Request,
    Response,
    SessionParam,
    TextItem,
    TokenUsage,
)


@pytest.fixture(scope="module")
def chat_session(chat_model):
    s = ChatSession(chat_model)
    s.set_options({SessionParam.Temperature: "0", SessionParam.MaxOutputTokens: "32"})
    return s


def _short_request() -> Request:
    return Request().add_item(MessageItem.user("Reply with the single word: hello"))


class TestProcessRequest:
    def test_returns_response(self, chat_session):
        resp = chat_session.process_request(_short_request())
        assert isinstance(resp, Response)
        assert resp.item_count >= 1

    def test_finish_reason_is_stop_or_length(self, chat_session):
        resp = chat_session.process_request(_short_request())
        assert resp.finish_reason in {FinishReason.STOP, FinishReason.LENGTH}

    def test_usage_has_positive_counts(self, chat_session):
        resp = chat_session.process_request(_short_request())
        usage = resp.get_usage()
        assert isinstance(usage, TokenUsage)
        assert usage.prompt_tokens > 0
        assert usage.completion_tokens > 0
        assert usage.total_tokens == usage.prompt_tokens + usage.completion_tokens

    def test_iterating_response_yields_items(self, chat_session):
        resp = chat_session.process_request(_short_request())
        items = list(resp)
        assert len(items) == resp.item_count
        # At least one TEXT or MESSAGE item must be present.
        kinds = {it.item_type for it in items}
        assert kinds & {ItemType.TEXT, ItemType.MESSAGE}


class TestStreaming:
    def test_yields_items_in_order(self, chat_session):
        chat_session.set_streaming(True)
        try:
            items = list(chat_session.process_streaming_request(_short_request()))
        finally:
            chat_session.set_streaming(False)

        assert items, "Streaming session must yield at least one item"

    def test_break_mid_stream_does_not_break_session(self, chat_session):
        chat_session.set_streaming(True)
        try:
            gen = chat_session.process_streaming_request(_short_request())
            first = next(gen)
            assert first is not None
            gen.close()  # triggers finally → request.cancel() + thread join
        finally:
            chat_session.set_streaming(False)

        # Session must still process subsequent (non-streaming) requests.
        resp = chat_session.process_request(_short_request())
        assert resp.item_count >= 1

    def test_streaming_without_enable_raises(self, chat_session):
        # Make sure streaming is off.
        chat_session.set_streaming(False)
        from foundry_local.exception import FoundryLocalException

        with pytest.raises(FoundryLocalException, match="Streaming"):
            list(chat_session.process_streaming_request(_short_request()))


class TestTurnCount:
    def test_turn_count_increases_per_request(self, chat_model):
        # Use a fresh session so the count starts at 0.
        s = ChatSession(chat_model)
        s.set_options({SessionParam.Temperature: "0", SessionParam.MaxOutputTokens: "16"})
        assert s.turn_count == 0
        s.process_request(Request().add_item(MessageItem.user("hi")))
        assert s.turn_count == 1
        s.process_request(Request().add_item(MessageItem.user("again")))
        assert s.turn_count == 2


class TestEmbeddingsSession:
    """Typed embedding flow: ``TextItem`` in, ``TensorItem`` out.

    This exercises the working code path that bypasses the OpenAI-JSON
    wrapping in ``foundry_local/openai/embedding_client.py``.
    """

    def test_returns_one_tensor_per_input(self, embedding_model):
        from foundry_local import EmbeddingsSession, TensorItem

        sess = EmbeddingsSession(embedding_model)
        req = Request().add_item(TextItem("hello world"))
        resp = sess.process_request(req)

        items = list(resp)
        assert items, "Embedding response must have at least one item"
        # Exactly one tensor for a single input.
        tensor_items = [it for it in items if isinstance(it, TensorItem)]
        assert tensor_items, f"Expected a TensorItem, got {[type(i).__name__ for i in items]}"
        assert len(tensor_items) == 1

    def test_tensor_has_nonzero_dimensions(self, embedding_model):
        from foundry_local import EmbeddingsSession, TensorItem

        sess = EmbeddingsSession(embedding_model)
        req = Request().add_item(TextItem("vector me"))
        resp = sess.process_request(req)

        tensor = next(it for it in resp if isinstance(it, TensorItem))
        # A real embedding has at least one non-trivial axis with > 1 element.
        if hasattr(tensor, "dimensions") and tensor.dimensions:
            assert any(d > 1 for d in tensor.dimensions), (
                f"Embedding tensor has only singleton dims: {tensor.dimensions}"
            )
