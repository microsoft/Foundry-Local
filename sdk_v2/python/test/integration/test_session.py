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

from foundry_local_sdk import (
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


@pytest.fixture
def chat_session(chat_model):
    """Function-scoped chat session.

    Creating a ``ChatSession`` is just a ``Session_Create`` call against the
    already-loaded ``chat_model`` (the expensive part) — cheap enough to do
    per-test. Function scope means every test gets a clean session with
    streaming off, no turn-count carryover, and no option pollution.
    """
    with ChatSession(chat_model) as s:
        s.set_options({SessionParam.Temperature: "0", SessionParam.MaxOutputTokens: "32"})
        yield s


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
    # Multi-token prompt with deterministic substrings so we can validate both
    # that streaming delivers multiple deltas (not a single coalesced item)
    # AND that the streamed content matches expectations. A 0.5B model may
    # abbreviate or reorder, so we require a subset rather than all four.
    _UK_COUNTRY_TOKENS = ("england", "scotland", "wales", "ireland")
    _UK_CAPITAL_TOKENS = ("london", "edinburgh", "cardiff", "belfast")

    @staticmethod
    def _count_tokens(text: str, tokens: tuple[str, ...]) -> int:
        lower = text.lower()
        return sum(1 for t in tokens if t in lower)

    @staticmethod
    def _uk_countries_request() -> Request:
        return Request().add_item(MessageItem.user("Name the countries in the United Kingdom."))

    def test_yields_items_in_order(self, chat_session):
        # Override the fixture's 32-token cap so the model can list multiple countries.
        chat_session.set_options({SessionParam.Temperature: "0", SessionParam.MaxOutputTokens: "128"})
        chat_session.set_streaming(True)

        items = list(chat_session.process_streaming_request(self._uk_countries_request()))

        # Real streaming must deliver more than a single coalesced delta.
        assert len(items) >= 2, f"Expected multiple streamed items, got {len(items)}"

        text = "".join(
            it.text for it in items
            if it.item_type == ItemType.TEXT and isinstance(it, TextItem)
        )
        assert text.strip(), "Streamed content was empty"

        found = self._count_tokens(text, self._UK_COUNTRY_TOKENS)
        assert found >= 2, (
            f"Expected at least 2 UK country names in streamed response. Got: {text!r}"
        )

    def test_streaming_multi_turn_history_aware(self, chat_session):
        """Turn 2 depends on turn 1's context, validating session history and streaming reuse."""
        chat_session.set_options({SessionParam.Temperature: "0", SessionParam.MaxOutputTokens: "128"})
        chat_session.set_streaming(True)

        # Turn 1: UK countries.
        items1 = list(chat_session.process_streaming_request(self._uk_countries_request()))
        assert len(items1) >= 2
        text1 = "".join(it.text for it in items1 if isinstance(it, TextItem))
        assert self._count_tokens(text1, self._UK_COUNTRY_TOKENS) >= 2, (
            f"Turn 1: expected UK countries. Got: {text1!r}"
        )

        # Turn 2: capital of each — only answerable from turn 1's context.
        req2 = Request().add_item(MessageItem.user("What is the capital of each?"))
        items2 = list(chat_session.process_streaming_request(req2))
        assert len(items2) >= 2
        text2 = "".join(it.text for it in items2 if isinstance(it, TextItem))
        assert self._count_tokens(text2, self._UK_CAPITAL_TOKENS) >= 2, (
            f"Turn 2: expected UK capitals. Got: {text2!r}"
        )

    def test_break_mid_stream_does_not_break_session(self, chat_session):
        chat_session.set_streaming(True)
        gen = chat_session.process_streaming_request(_short_request())
        first = next(gen)
        assert first is not None
        gen.close()  # triggers finally → request.cancel() + thread join

        # Session must still process subsequent (non-streaming) requests.
        chat_session.set_streaming(False)
        resp = chat_session.process_request(_short_request())
        assert resp.item_count >= 1

    def test_streaming_without_enable_raises(self, chat_session):
        # Streaming is off by default on a fresh session.
        from foundry_local_sdk.exception import FoundryLocalException

        with pytest.raises(FoundryLocalException, match="Streaming"):
            list(chat_session.process_streaming_request(_short_request()))


class TestTurnCount:
    def test_turn_count_increases_per_request(self, chat_model):
        # Use a fresh session so the count starts at 0.
        with ChatSession(chat_model) as s:
            s.set_options({SessionParam.Temperature: "0", SessionParam.MaxOutputTokens: "16"})
            assert s.turn_count == 0
            with Request().add_item(MessageItem.user("hi")) as req:
                s.process_request(req)
            assert s.turn_count == 1
            with Request().add_item(MessageItem.user("again")) as req:
                s.process_request(req)
            assert s.turn_count == 2


class TestEmbeddingsSession:
    """Typed embedding flow: ``TextItem`` in, ``TensorItem`` out.

    This exercises the working code path that bypasses the OpenAI-JSON
    wrapping in ``foundry_local_sdk/openai/embedding_client.py``.
    """

    def test_returns_one_tensor_per_input(self, embedding_model):
        from foundry_local_sdk import EmbeddingsSession, TensorItem

        with (
            EmbeddingsSession(embedding_model) as sess,
            Request().add_item(TextItem("hello world")) as req,
        ):
            resp = sess.process_request(req)
            try:
                items = list(resp)
                assert items, "Embedding response must have at least one item"
                # Exactly one tensor for a single input.
                tensor_items = [it for it in items if isinstance(it, TensorItem)]
                assert tensor_items, (
                    f"Expected a TensorItem, got {[type(i).__name__ for i in items]}"
                )
                assert len(tensor_items) == 1
            finally:
                resp._close()

    def test_tensor_has_nonzero_dimensions(self, embedding_model):
        from foundry_local_sdk import EmbeddingsSession, TensorItem

        with (
            EmbeddingsSession(embedding_model) as sess,
            Request().add_item(TextItem("vector me")) as req,
        ):
            resp = sess.process_request(req)
            try:
                tensor = next(it for it in resp if isinstance(it, TensorItem))
                # A real embedding has at least one non-trivial axis with > 1 element.
                if hasattr(tensor, "dimensions") and tensor.dimensions:
                    assert any(d > 1 for d in tensor.dimensions), (
                        f"Embedding tensor has only singleton dims: {tensor.dimensions}"
                    )
            finally:
                resp._close()
