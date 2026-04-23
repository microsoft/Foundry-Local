# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Unit tests for the Responses API client (no live server required).

Mirrors the scenarios covered by the JS SDK's ``responsesClient.test.ts`` and
the Python spec's §5. HTTP calls are intercepted via :mod:`unittest.mock`.
"""

from __future__ import annotations

import base64
import io
import json
from typing import Any, Dict, List
from unittest.mock import MagicMock, patch

import pytest

from foundry_local_sdk.openai.responses_client import (
    ResponsesAPIError,
    ResponsesClient,
    ResponsesClientSettings,
    _parse_sse_block,
    _iter_sse_events,
    _SSE_DONE,
)
from foundry_local_sdk.openai.responses_types import (
    FunctionCallItem,
    FunctionToolDefinition,
    InputImageContent,
    InputTextContent,
    MessageItem,
    OutputTextContent,
    ReasoningConfig,
    ResponseObject,
    TextConfig,
    TextFormat,
    _to_dict,
    parse_streaming_event,
    OutputTextDeltaEvent,
    ResponseLifecycleEvent,
    StreamingErrorEvent,
    UnknownStreamingEvent,
)

BASE_URL = "http://127.0.0.1:5273"
MODEL_ID = "test-model"


def _fake_json_response(payload: Dict[str, Any], status: int = 200):
    resp = MagicMock()
    resp.ok = 200 <= status < 300
    resp.status_code = status
    resp.text = json.dumps(payload)
    return resp


def _fake_stream_response(sse_payload: str, status: int = 200):
    resp = MagicMock()
    resp.ok = 200 <= status < 300
    resp.status_code = status
    resp.text = sse_payload
    # iter_content returns the full payload in one bytes chunk.
    resp.iter_content = MagicMock(return_value=iter([sse_payload.encode("utf-8")]))
    resp.close = MagicMock()
    return resp


# ---------------------------------------------------------------------------
# Settings
# ---------------------------------------------------------------------------

class TestResponsesClientSettings:
    def test_serialize_defaults_contains_store(self):
        # store defaults to True — matches OpenAI convention
        s = ResponsesClientSettings()
        serialized = s._serialize()
        assert serialized == {"store": True}

    def test_store_defaults_to_true(self):
        assert ResponsesClientSettings().store is True

    def test_serialize_all_fields(self):
        s = ResponsesClientSettings()
        s.instructions = "Be concise."
        s.temperature = 0.2
        s.top_p = 0.9
        s.max_output_tokens = 256
        s.frequency_penalty = 0.1
        s.presence_penalty = 0.2
        s.tool_choice = "auto"
        s.truncation = "auto"
        s.parallel_tool_calls = False
        s.store = False
        s.metadata = {"run": "1"}
        s.reasoning = ReasoningConfig(effort="medium")
        s.text = TextConfig(format=TextFormat(type="json_object"))
        s.seed = 42

        out = s._serialize()
        assert out["instructions"] == "Be concise."
        assert out["temperature"] == 0.2
        assert out["top_p"] == 0.9
        assert out["max_output_tokens"] == 256
        assert out["frequency_penalty"] == 0.1
        assert out["presence_penalty"] == 0.2
        assert out["tool_choice"] == "auto"
        assert out["truncation"] == "auto"
        assert out["parallel_tool_calls"] is False
        assert out["store"] is False
        assert out["metadata"] == {"run": "1"}
        assert out["reasoning"] == {"effort": "medium"}
        assert out["text"] == {"format": {"type": "json_object"}}
        assert out["seed"] == 42

    def test_serialize_omits_none(self):
        s = ResponsesClientSettings()
        s.temperature = None  # explicit None is omitted
        assert "temperature" not in s._serialize()


# ---------------------------------------------------------------------------
# Input / tool / id validation
# ---------------------------------------------------------------------------

class TestInputValidation:
    def setup_method(self):
        self.client = ResponsesClient(BASE_URL, MODEL_ID)

    def test_rejects_none(self):
        with pytest.raises(ValueError, match="None"):
            self.client._build_request(None, {}, stream=False)

    def test_rejects_empty_string(self):
        with pytest.raises(ValueError, match="empty"):
            self.client._build_request("", {}, stream=False)

    def test_rejects_whitespace_string(self):
        with pytest.raises(ValueError, match="empty"):
            self.client._build_request("   ", {}, stream=False)

    def test_rejects_empty_array(self):
        with pytest.raises(ValueError, match="empty"):
            self.client._build_request([], {}, stream=False)

    def test_rejects_item_without_type(self):
        with pytest.raises(ValueError, match="type"):
            self.client._build_request([{"role": "user"}], {}, stream=False)

    def test_accepts_string_input(self):
        body = self.client._build_request("Hi", {}, stream=False)
        assert body["input"] == "Hi"
        assert body["model"] == MODEL_ID

    def test_accepts_dict_input_items(self):
        body = self.client._build_request(
            [{"type": "message", "role": "user", "content": "hi"}], {}, stream=False
        )
        assert isinstance(body["input"], list)
        assert body["input"][0]["type"] == "message"

    def test_accepts_dataclass_input_items(self):
        item = MessageItem(role="user", content="hello")
        body = self.client._build_request([item], {}, stream=False)
        assert body["input"][0]["type"] == "message"
        assert body["input"][0]["role"] == "user"
        assert body["input"][0]["content"] == "hello"

    def test_stream_flag_set(self):
        body = self.client._build_request("hi", {}, stream=True)
        assert body["stream"] is True

    def test_requires_model(self):
        c = ResponsesClient(BASE_URL)  # no default model
        with pytest.raises(ValueError, match="[Mm]odel"):
            c._build_request("hi", {}, stream=False)

    def test_options_model_overrides_default(self):
        body = self.client._build_request("hi", {"model": "override"}, stream=False)
        assert body["model"] == "override"


class TestToolValidation:
    def setup_method(self):
        self.client = ResponsesClient(BASE_URL, MODEL_ID)

    def test_rejects_non_function_type(self):
        with pytest.raises(ValueError, match="function"):
            self.client._build_request("hi", {"tools": [{"type": "retrieval", "name": "x"}]}, stream=False)

    def test_rejects_empty_name(self):
        with pytest.raises(ValueError, match="name"):
            self.client._build_request("hi", {"tools": [{"type": "function", "name": ""}]}, stream=False)

    def test_rejects_non_list(self):
        with pytest.raises(ValueError, match="list"):
            self.client._build_request("hi", {"tools": "nope"}, stream=False)

    def test_accepts_valid_dict_tool(self):
        body = self.client._build_request(
            "hi",
            {"tools": [{"type": "function", "name": "multiply", "parameters": {}}]},
            stream=False,
        )
        assert body["tools"][0]["name"] == "multiply"

    def test_accepts_dataclass_tool(self):
        tool = FunctionToolDefinition(name="multiply", description="x*y")
        body = self.client._build_request("hi", {"tools": [tool]}, stream=False)
        assert body["tools"][0]["type"] == "function"
        assert body["tools"][0]["name"] == "multiply"
        assert body["tools"][0]["description"] == "x*y"


class TestIdValidation:
    def setup_method(self):
        self.client = ResponsesClient(BASE_URL, MODEL_ID)

    def test_rejects_empty_id(self):
        with pytest.raises(ValueError, match="non-empty"):
            self.client.get("")

    def test_rejects_whitespace_id(self):
        with pytest.raises(ValueError, match="non-empty"):
            self.client.get("   ")

    def test_rejects_too_long_id(self):
        with pytest.raises(ValueError, match="length"):
            self.client.get("x" * 2000)


# ---------------------------------------------------------------------------
# output_text convenience
# ---------------------------------------------------------------------------

class TestOutputText:
    def test_extracts_from_string_content(self):
        resp = ResponseObject(output=[MessageItem(role="assistant", content="hello world")])
        assert resp.output_text == "hello world"

    def test_extracts_from_content_parts(self):
        resp = ResponseObject(output=[
            MessageItem(
                role="assistant",
                content=[OutputTextContent(text="foo "), OutputTextContent(text="bar")],
            )
        ])
        assert resp.output_text == "foo bar"

    def test_returns_empty_when_no_assistant(self):
        resp = ResponseObject(output=[MessageItem(role="user", content="hi")])
        assert resp.output_text == ""

    def test_returns_empty_for_empty_output(self):
        assert ResponseObject().output_text == ""

    def test_skips_function_call_items(self):
        resp = ResponseObject(output=[
            FunctionCallItem(call_id="c1", name="f", arguments="{}"),
            MessageItem(role="assistant", content="done"),
        ])
        assert resp.output_text == "done"


# ---------------------------------------------------------------------------
# SSE parsing
# ---------------------------------------------------------------------------

class TestSSEParsing:
    def test_parses_complete_event(self):
        block = 'event: response.output_text.delta\ndata: {"type":"response.output_text.delta","delta":"hi","sequence_number":3}'
        evt = _parse_sse_block(block)
        assert isinstance(evt, OutputTextDeltaEvent)
        assert evt.delta == "hi"
        assert evt.sequence_number == 3

    def test_done_signal(self):
        assert _parse_sse_block("data: [DONE]") is _SSE_DONE

    def test_multi_line_data(self):
        # Per SSE spec, multiple data: lines join with \n into one JSON doc.
        block = 'data: {"type":"error",\ndata: "message":"oops","sequence_number":0}'
        evt = _parse_sse_block(block)
        assert isinstance(evt, StreamingErrorEvent)
        assert evt.message == "oops"

    def test_invalid_json_raises(self):
        block = 'data: {not valid json'
        with pytest.raises(ResponsesAPIError):
            _parse_sse_block(block)

    def test_empty_block_returns_none(self):
        assert _parse_sse_block("") is None
        assert _parse_sse_block("\n\n") is None

    def test_ignores_non_data_lines(self):
        block = 'id: 1\nretry: 1000\nevent: response.created\ndata: {"type":"response.created","response":{"id":"r1"},"sequence_number":0}'
        evt = _parse_sse_block(block)
        assert isinstance(evt, ResponseLifecycleEvent)
        assert evt.type == "response.created"

    def test_error_event(self):
        block = 'data: {"type":"error","code":"bad","message":"oops","sequence_number":0}'
        evt = _parse_sse_block(block)
        assert isinstance(evt, StreamingErrorEvent)
        assert evt.code == "bad"
        assert evt.message == "oops"

    def test_iter_sse_events_handles_partial_chunks(self):
        payload_events = [
            'event: response.output_text.delta\ndata: {"type":"response.output_text.delta","delta":"Hel","sequence_number":1}\n\n',
            'event: response.output_text.delta\ndata: {"type":"response.output_text.delta","delta":"lo","sequence_number":2}\n\n',
            'data: [DONE]\n\n',
        ]
        full = "".join(payload_events).encode("utf-8")

        # Split the bytes into irregular chunks to exercise buffering.
        chunks = [full[i:i + 7] for i in range(0, len(full), 7)]

        resp = MagicMock()
        resp.iter_content = MagicMock(return_value=iter(chunks))
        resp.close = MagicMock()

        events = list(_iter_sse_events(resp))
        assert len(events) == 2
        assert all(isinstance(e, OutputTextDeltaEvent) for e in events)
        assert "".join(e.delta for e in events) == "Hello"
        resp.close.assert_called()

    def test_iter_sse_handles_crlf(self):
        payload = (
            'event: response.output_text.delta\r\n'
            'data: {"type":"response.output_text.delta","delta":"x","sequence_number":0}\r\n'
            '\r\n'
            'data: [DONE]\r\n\r\n'
        )
        resp = MagicMock()
        resp.iter_content = MagicMock(return_value=iter([payload.encode("utf-8")]))
        resp.close = MagicMock()

        events = list(_iter_sse_events(resp))
        assert len(events) == 1
        assert events[0].delta == "x"

    def test_unknown_event_type(self):
        block = 'data: {"type":"response.brand_new_event","sequence_number":7}'
        evt = _parse_sse_block(block)
        assert isinstance(evt, UnknownStreamingEvent)
        assert evt.type == "response.brand_new_event"


# ---------------------------------------------------------------------------
# Vision types
# ---------------------------------------------------------------------------

class TestVisionTypes:
    def test_input_image_from_bytes(self):
        data = b"\x89PNG\r\n\x1a\nfakedata"
        img = InputImageContent.from_bytes(data, "image/png", detail="high")
        assert img.media_type == "image/png"
        assert img.detail == "high"
        assert base64.b64decode(img.image_data) == data

    def test_input_image_from_url(self):
        img = InputImageContent.from_url("https://example.com/x.png")
        assert img.image_url == "https://example.com/x.png"
        assert img.image_data is None

    def test_input_image_from_file(self, tmp_path):
        data = b"\x89PNG\r\n\x1a\nfakedata"
        p = tmp_path / "test.png"
        p.write_bytes(data)
        img = InputImageContent.from_file(str(p))
        assert img.media_type == "image/png"
        assert base64.b64decode(img.image_data) == data

    def test_input_image_from_file_rejects_non_image(self, tmp_path):
        p = tmp_path / "text.txt"
        p.write_text("not an image")
        with pytest.raises(ValueError, match="Unsupported"):
            InputImageContent.from_file(str(p))

    def test_input_image_serialization(self):
        img = InputImageContent(media_type="image/png", image_data="abc", detail="low")
        d = _to_dict(img)
        assert d == {"media_type": "image/png", "image_data": "abc", "detail": "low", "type": "input_image"}
        # image_url left unset should be omitted
        assert "image_url" not in d


# ---------------------------------------------------------------------------
# Type serialization & parsing
# ---------------------------------------------------------------------------

class TestTypeSerialization:
    def test_message_item_to_dict(self):
        msg = MessageItem(
            role="user",
            content=[InputTextContent(text="Hi"), InputImageContent(media_type="image/png", image_data="abc")],
        )
        d = _to_dict(msg)
        assert d["type"] == "message"
        assert d["role"] == "user"
        assert d["content"][0] == {"text": "Hi", "type": "input_text"}
        assert d["content"][1]["type"] == "input_image"
        assert "id" not in d  # None omitted

    def test_function_tool_to_dict(self):
        tool = FunctionToolDefinition(
            name="multiply",
            description="x*y",
            parameters={"type": "object", "properties": {"a": {"type": "number"}}},
            strict=True,
        )
        d = _to_dict(tool)
        assert d == {
            "name": "multiply",
            "description": "x*y",
            "parameters": {"type": "object", "properties": {"a": {"type": "number"}}},
            "strict": True,
            "type": "function",
        }

    def test_response_object_from_dict(self):
        from foundry_local_sdk.openai.responses_types import _parse_response_object

        payload = {
            "id": "resp_abc",
            "object": "response",
            "created_at": 1700000000,
            "status": "completed",
            "model": "phi-4-mini",
            "output": [
                {
                    "type": "message",
                    "role": "assistant",
                    "content": [{"type": "output_text", "text": "Hello!"}],
                }
            ],
            "usage": {"input_tokens": 3, "output_tokens": 2, "total_tokens": 5},
            "store": True,
        }
        r = _parse_response_object(payload)
        assert r.id == "resp_abc"
        assert r.status == "completed"
        assert r.usage.total_tokens == 5
        assert r.output_text == "Hello!"

    def test_streaming_event_parsing_lifecycle(self):
        evt = parse_streaming_event(
            {
                "type": "response.completed",
                "response": {"id": "resp_1", "status": "completed"},
                "sequence_number": 10,
            }
        )
        assert isinstance(evt, ResponseLifecycleEvent)
        assert evt.type == "response.completed"
        assert evt.response.id == "resp_1"
        assert evt.sequence_number == 10


# ---------------------------------------------------------------------------
# End-to-end (mocked HTTP)
# ---------------------------------------------------------------------------

class TestClientHTTPFlow:
    def setup_method(self):
        self.client = ResponsesClient(BASE_URL, MODEL_ID)

    def test_create_posts_correct_body(self):
        payload = {
            "id": "resp_1",
            "object": "response",
            "status": "completed",
            "model": MODEL_ID,
            "output": [
                {"type": "message", "role": "assistant", "content": "ok"},
            ],
        }
        with patch("foundry_local_sdk.openai.responses_client.requests.request") as mock_req:
            mock_req.return_value = _fake_json_response(payload)
            result = self.client.create("hello", temperature=0.3)

        assert result.id == "resp_1"
        assert result.output_text == "ok"

        _, kwargs = mock_req.call_args
        assert mock_req.call_args.args[0] == "POST"
        assert mock_req.call_args.args[1] == f"{BASE_URL}/v1/responses"
        body = json.loads(kwargs["data"])
        assert body["model"] == MODEL_ID
        assert body["input"] == "hello"
        assert body["temperature"] == 0.3
        assert body["store"] is True  # default
        assert "stream" not in body

    def test_get_uses_url_encoded_path(self):
        weird_id = "resp_with/slashes and spaces"
        with patch("foundry_local_sdk.openai.responses_client.requests.request") as mock_req:
            mock_req.return_value = _fake_json_response(
                {"id": weird_id, "object": "response", "status": "completed", "model": MODEL_ID, "output": []}
            )
            self.client.get(weird_id)

        path = mock_req.call_args.args[1]
        assert "resp_with%2Fslashes%20and%20spaces" in path
        assert mock_req.call_args.args[0] == "GET"

    def test_delete_parses_result(self):
        with patch("foundry_local_sdk.openai.responses_client.requests.request") as mock_req:
            mock_req.return_value = _fake_json_response(
                {"id": "resp_1", "object": "response.deleted", "deleted": True}
            )
            result = self.client.delete("resp_1")
        assert result.deleted is True
        assert result.id == "resp_1"

    def test_http_error_raises_responses_api_error(self):
        resp = MagicMock()
        resp.ok = False
        resp.status_code = 400
        resp.text = '{"error":{"message":"bad"}}'
        with patch("foundry_local_sdk.openai.responses_client.requests.request", return_value=resp):
            with pytest.raises(ResponsesAPIError) as excinfo:
                self.client.create("hi")
        assert excinfo.value.status_code == 400
        assert "bad" in str(excinfo.value)

    def test_create_streaming_yields_events(self):
        sse = (
            'event: response.output_text.delta\n'
            'data: {"type":"response.output_text.delta","delta":"a","sequence_number":1}\n'
            '\n'
            'event: response.output_text.delta\n'
            'data: {"type":"response.output_text.delta","delta":"b","sequence_number":2}\n'
            '\n'
            'data: [DONE]\n\n'
        )
        with patch("foundry_local_sdk.openai.responses_client.requests.post") as mock_post:
            mock_post.return_value = _fake_stream_response(sse)
            events = list(self.client.create_streaming("hi"))

        assert len(events) == 2
        assert "".join(e.delta for e in events) == "ab"
        _, kwargs = mock_post.call_args
        body = json.loads(kwargs["data"])
        assert body["stream"] is True
        assert kwargs["headers"]["Accept"] == "text/event-stream"

    def test_streaming_http_error(self):
        resp = MagicMock()
        resp.ok = False
        resp.status_code = 500
        resp.text = "boom"
        resp.close = MagicMock()
        with patch("foundry_local_sdk.openai.responses_client.requests.post", return_value=resp):
            with pytest.raises(ResponsesAPIError) as excinfo:
                list(self.client.create_streaming("hi"))
        assert excinfo.value.status_code == 500

    def test_settings_merge_precedence(self):
        self.client.settings.temperature = 0.1
        self.client.settings.max_output_tokens = 100
        with patch("foundry_local_sdk.openai.responses_client.requests.request") as mock_req:
            mock_req.return_value = _fake_json_response(
                {"id": "r", "object": "response", "status": "completed", "model": MODEL_ID, "output": []}
            )
            # Per-call overrides client settings
            self.client.create("hi", temperature=0.9)

        body = json.loads(mock_req.call_args.kwargs["data"])
        assert body["temperature"] == 0.9  # per-call wins
        assert body["max_output_tokens"] == 100  # settings default preserved


class TestManagerFactory:
    """Ensure the factory method wiring doesn't require a running server."""

    def test_manager_raises_if_web_service_not_started(self):
        from foundry_local_sdk.exception import FoundryLocalException

        # Build a stand-in manager without going through the constructor's
        # heavy initialization path.
        mgr = MagicMock()
        mgr.urls = None
        # Bind the real method to our MagicMock so we exercise actual logic.
        from foundry_local_sdk.foundry_local_manager import FoundryLocalManager as M

        with pytest.raises(FoundryLocalException, match="[Ww]eb service"):
            M.create_responses_client(mgr, "some-model")

    def test_manager_returns_client_when_urls_set(self):
        mgr = MagicMock()
        mgr.urls = [BASE_URL]
        from foundry_local_sdk.foundry_local_manager import FoundryLocalManager as M

        client = M.create_responses_client(mgr, "phi")
        assert isinstance(client, ResponsesClient)
        assert client._model_id == "phi"
        assert client._base_url == BASE_URL
