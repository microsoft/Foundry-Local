# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Types for the OpenAI Responses API served by Foundry Local.

These mirror the DTOs defined by neutron-server in
``src/FoundryLocalCore/Core/Responses/Contracts/``. Dataclasses are used so
callers can construct items with keyword arguments and we can serialize
discriminated unions by the ``type`` field.
"""

from __future__ import annotations

import base64
import io
import mimetypes
from dataclasses import dataclass, field, fields, is_dataclass
from typing import Any, Dict, List, Literal, Optional, Tuple, Union


# ---------------------------------------------------------------------------
# Image resize helper (optional — requires Pillow)
# ---------------------------------------------------------------------------

def _resize_image(data: bytes, media_type: str, max_size: Tuple[int, int]) -> Tuple[bytes, str]:
    """Resize *data* so it fits within *max_size* (width, height) while preserving
    aspect ratio. Returns the re-encoded bytes and MIME type.

    Requires ``Pillow`` (``pip install pillow``). Raises ``ImportError`` if it is
    not installed.
    """
    try:
        from PIL import Image  # type: ignore[import-untyped]
    except ImportError as exc:
        raise ImportError(
            "Image resizing requires Pillow. Install it with: pip install pillow"
        ) from exc

    img = Image.open(io.BytesIO(data))
    img.thumbnail(max_size, Image.LANCZOS)
    buf = io.BytesIO()
    fmt = media_type.split("/")[-1].upper().replace("JPG", "JPEG")
    if fmt not in ("JPEG", "PNG", "WEBP", "GIF"):
        fmt = "PNG"
        media_type = "image/png"
    img.save(buf, format=fmt)
    return buf.getvalue(), media_type


# ---------------------------------------------------------------------------
# Serialization helpers
# ---------------------------------------------------------------------------

def _to_dict(obj: Any) -> Any:
    """Recursively convert a dataclass (or list/dict of them) to a plain dict,
    omitting ``None`` values so the wire format matches the OpenAI spec.
    """
    if is_dataclass(obj) and not isinstance(obj, type):
        result: Dict[str, Any] = {}
        for f in fields(obj):
            value = getattr(obj, f.name)
            if value is None:
                continue
            result[f.name] = _to_dict(value)
        return result
    if isinstance(obj, list):
        return [_to_dict(v) for v in obj]
    if isinstance(obj, dict):
        return {k: _to_dict(v) for k, v in obj.items() if v is not None}
    return obj


# ---------------------------------------------------------------------------
# Content Parts
# ---------------------------------------------------------------------------

@dataclass
class InputTextContent:
    text: str = ""
    type: Literal["input_text"] = "input_text"


@dataclass
class InputImageContent:
    """Vision input. Provide exactly one of ``image_url`` or ``image_data`` (base64)."""
    media_type: str = ""
    image_url: Optional[str] = None
    image_data: Optional[str] = None
    detail: Optional[str] = None  # "low" | "high" | "auto"
    type: Literal["input_image"] = "input_image"

    def __post_init__(self) -> None:
        has_url = self.image_url is not None
        has_data = self.image_data is not None
        if has_url == has_data:
            raise ValueError(
                "Provide exactly one of image_url or image_data, not both (or neither)."
            )

    @staticmethod
    def from_file(
        path: str,
        detail: Optional[str] = None,
        max_size: Optional[Tuple[int, int]] = None,
    ) -> "InputImageContent":
        """Load an image from *path*, base64-encode it, and return an :class:`InputImageContent`.

        Args:
            path: Filesystem path to the image file.
            detail: OpenAI detail hint – ``"low"``, ``"high"``, or ``"auto"``.
            max_size: Optional ``(width, height)`` cap. If the image exceeds either
                dimension it is resized proportionally (requires ``Pillow``).
        """
        media_type, _ = mimetypes.guess_type(path)
        if not media_type or not media_type.startswith("image/"):
            raise ValueError(f"Unsupported image format: {path}")
        with open(path, "rb") as fh:
            raw = fh.read()
        if max_size is not None:
            raw, media_type = _resize_image(raw, media_type, max_size)
        return InputImageContent(
            image_data=base64.b64encode(raw).decode("ascii"),
            media_type=media_type,
            detail=detail,
        )

    @staticmethod
    def from_url(url: str, detail: Optional[str] = None) -> "InputImageContent":
        return InputImageContent(image_url=url, media_type="image/unknown", detail=detail)

    @staticmethod
    def from_bytes(
        data: bytes,
        media_type: str,
        detail: Optional[str] = None,
        max_size: Optional[Tuple[int, int]] = None,
    ) -> "InputImageContent":
        """Create an :class:`InputImageContent` from raw *data* bytes.

        Args:
            data: Raw image bytes.
            media_type: MIME type, e.g. ``"image/png"``.
            detail: OpenAI detail hint – ``"low"``, ``"high"``, or ``"auto"``.
            max_size: Optional ``(width, height)`` cap. Requires ``Pillow``.
        """
        if max_size is not None:
            data, media_type = _resize_image(data, media_type, max_size)
        return InputImageContent(
            image_data=base64.b64encode(data).decode("ascii"),
            media_type=media_type,
            detail=detail,
        )


@dataclass
class InputFileContent:
    filename: str = ""
    file_url: str = ""
    type: Literal["input_file"] = "input_file"


@dataclass
class OutputTextContent:
    text: str = ""
    annotations: Optional[List[Any]] = None
    logprobs: Optional[List[Any]] = None
    type: Literal["output_text"] = "output_text"


@dataclass
class RefusalContent:
    refusal: str = ""
    type: Literal["refusal"] = "refusal"


ContentPart = Union[
    InputTextContent, InputImageContent, InputFileContent, OutputTextContent, RefusalContent
]


def _parse_content_part(data: Dict[str, Any]) -> Optional[ContentPart]:
    t = data.get("type")
    if t == "input_text":
        return InputTextContent(text=data.get("text", ""))
    if t == "input_image":
        return InputImageContent(
            media_type=data.get("media_type", ""),
            image_url=data.get("image_url"),
            image_data=data.get("image_data"),
            detail=data.get("detail"),
        )
    if t == "input_file":
        return InputFileContent(filename=data.get("filename", ""), file_url=data.get("file_url", ""))
    if t == "output_text":
        return OutputTextContent(
            text=data.get("text", ""),
            annotations=data.get("annotations"),
            logprobs=data.get("logprobs"),
        )
    if t == "refusal":
        return RefusalContent(refusal=data.get("refusal", ""))
    # Unknown content-part type — return None so callers can filter forward-compat parts.
    return None


def _parse_content(value: Any) -> Union[str, List[ContentPart]]:
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        parts = [_parse_content_part(p) if isinstance(p, dict) else p for p in value]
        return [p for p in parts if p is not None]
    return value


# ---------------------------------------------------------------------------
# Response Items (input + output)
# ---------------------------------------------------------------------------

@dataclass
class MessageItem:
    role: str = ""
    content: Union[str, List[ContentPart]] = ""
    id: Optional[str] = None
    status: Optional[str] = None
    type: Literal["message"] = "message"


@dataclass
class FunctionCallItem:
    call_id: str = ""
    name: str = ""
    arguments: str = ""
    id: Optional[str] = None
    status: Optional[str] = None
    type: Literal["function_call"] = "function_call"


@dataclass
class FunctionCallOutputItem:
    call_id: str = ""
    output: Union[str, List[ContentPart]] = ""
    id: Optional[str] = None
    type: Literal["function_call_output"] = "function_call_output"


@dataclass
class ItemReference:
    id: str = ""
    type: Literal["item_reference"] = "item_reference"


@dataclass
class ReasoningItem:
    id: Optional[str] = None
    content: Optional[List[ContentPart]] = None
    encrypted_content: Optional[str] = None
    summary: Optional[str] = None
    status: Optional[str] = None
    type: Literal["reasoning"] = "reasoning"


ResponseInputItem = Union[
    MessageItem, FunctionCallItem, FunctionCallOutputItem, ItemReference, ReasoningItem
]
ResponseOutputItem = Union[MessageItem, FunctionCallItem, ReasoningItem]


def _parse_response_item(data: Dict[str, Any]) -> Any:
    t = data.get("type")
    if t == "message":
        return MessageItem(
            role=data.get("role", ""),
            content=_parse_content(data.get("content", "")),
            id=data.get("id"),
            status=data.get("status"),
        )
    if t == "function_call":
        return FunctionCallItem(
            call_id=data.get("call_id", ""),
            name=data.get("name", ""),
            arguments=data.get("arguments", ""),
            id=data.get("id"),
            status=data.get("status"),
        )
    if t == "function_call_output":
        return FunctionCallOutputItem(
            call_id=data.get("call_id", ""),
            output=_parse_content(data.get("output", "")),
            id=data.get("id"),
        )
    if t == "item_reference":
        return ItemReference(id=data.get("id", ""))
    if t == "reasoning":
        content_raw = data.get("content")
        return ReasoningItem(
            id=data.get("id"),
            content=[_parse_content_part(p) for p in content_raw] if isinstance(content_raw, list) else None,
            encrypted_content=data.get("encrypted_content"),
            summary=data.get("summary"),
            status=data.get("status"),
        )
    # Unknown item type — return the raw dict so callers can inspect
    return data


# ---------------------------------------------------------------------------
# Tool Definitions & Config
# ---------------------------------------------------------------------------

@dataclass
class FunctionToolDefinition:
    name: str = ""
    description: Optional[str] = None
    parameters: Optional[Dict[str, Any]] = None
    strict: Optional[bool] = None
    type: Literal["function"] = "function"


@dataclass
class FunctionToolChoice:
    name: str = ""
    type: Literal["function"] = "function"


ToolChoice = Union[str, FunctionToolChoice]  # "none" | "auto" | "required" | {type,name}


@dataclass
class TextFormat:
    type: str = "text"  # "text" | "json_object" | "json_schema" | "lark_grammar" | "regex"
    name: Optional[str] = None
    description: Optional[str] = None
    schema: Optional[Dict[str, Any]] = None
    strict: Optional[bool] = None


@dataclass
class TextConfig:
    format: Optional[TextFormat] = None


@dataclass
class ReasoningConfig:
    effort: Optional[str] = None
    summary: Optional[str] = None


# ---------------------------------------------------------------------------
# Response Object
# ---------------------------------------------------------------------------

@dataclass
class ResponseUsage:
    input_tokens: int = 0
    output_tokens: int = 0
    total_tokens: int = 0
    input_tokens_details: Optional[Dict[str, Any]] = None
    output_tokens_details: Optional[Dict[str, Any]] = None


@dataclass
class ResponseError:
    code: str = ""
    message: str = ""


@dataclass
class IncompleteDetails:
    reason: str = ""


@dataclass
class ResponseObject:
    id: str = ""
    object: str = "response"
    created_at: int = 0
    status: str = ""
    model: str = ""
    output: List[Any] = field(default_factory=list)
    completed_at: Optional[int] = None
    failed_at: Optional[int] = None
    cancelled_at: Optional[int] = None
    error: Optional[ResponseError] = None
    usage: Optional[ResponseUsage] = None
    instructions: Optional[str] = None
    previous_response_id: Optional[str] = None
    tools: Optional[List[FunctionToolDefinition]] = None
    tool_choice: Optional[Any] = None
    temperature: Optional[float] = None
    top_p: Optional[float] = None
    max_output_tokens: Optional[int] = None
    frequency_penalty: Optional[float] = None
    presence_penalty: Optional[float] = None
    seed: Optional[int] = None
    truncation: Optional[str] = None
    parallel_tool_calls: Optional[bool] = None
    store: Optional[bool] = None
    metadata: Optional[Dict[str, str]] = None
    reasoning: Optional[ReasoningConfig] = None
    text: Optional[TextConfig] = None
    user: Optional[str] = None
    incomplete_details: Optional[IncompleteDetails] = None
    # Retain anything the server returned that we don't model explicitly.
    _raw: Optional[Dict[str, Any]] = None

    @property
    def output_text(self) -> str:
        """Concatenated text from the first assistant ``message`` item in ``output``."""
        for item in self.output:
            if isinstance(item, MessageItem) and item.role == "assistant":
                content = item.content
                if isinstance(content, str):
                    return content
                if isinstance(content, list):
                    parts: List[str] = []
                    for p in content:
                        text = getattr(p, "text", None)
                        if isinstance(text, str):
                            parts.append(text)
                    return "".join(parts)
        return ""


def _parse_response_object(data: Dict[str, Any]) -> ResponseObject:
    output = data.get("output") or []
    parsed_output = [_parse_response_item(i) if isinstance(i, dict) else i for i in output]

    tools_raw = data.get("tools")
    tools = None
    if isinstance(tools_raw, list):
        tools = [
            FunctionToolDefinition(
                name=t.get("name", ""),
                description=t.get("description"),
                parameters=t.get("parameters"),
                strict=t.get("strict"),
            ) if isinstance(t, dict) else t
            for t in tools_raw
        ]

    usage = None
    if isinstance(data.get("usage"), dict):
        u = data["usage"]
        usage = ResponseUsage(
            input_tokens=u.get("input_tokens", 0),
            output_tokens=u.get("output_tokens", 0),
            total_tokens=u.get("total_tokens", 0),
            input_tokens_details=u.get("input_tokens_details"),
            output_tokens_details=u.get("output_tokens_details"),
        )

    error = None
    if isinstance(data.get("error"), dict):
        error = ResponseError(code=data["error"].get("code", ""), message=data["error"].get("message", ""))

    incomplete = None
    if isinstance(data.get("incomplete_details"), dict):
        incomplete = IncompleteDetails(reason=data["incomplete_details"].get("reason", ""))

    reasoning = None
    if isinstance(data.get("reasoning"), dict):
        reasoning = ReasoningConfig(
            effort=data["reasoning"].get("effort"),
            summary=data["reasoning"].get("summary"),
        )

    text = None
    if isinstance(data.get("text"), dict):
        fmt_raw = data["text"].get("format")
        fmt = None
        if isinstance(fmt_raw, dict):
            fmt = TextFormat(
                type=fmt_raw.get("type", "text"),
                name=fmt_raw.get("name"),
                description=fmt_raw.get("description"),
                schema=fmt_raw.get("schema"),
                strict=fmt_raw.get("strict"),
            )
        text = TextConfig(format=fmt)

    return ResponseObject(
        id=data.get("id", ""),
        object=data.get("object", "response"),
        created_at=data.get("created_at", 0),
        status=data.get("status", ""),
        model=data.get("model", ""),
        output=parsed_output,
        completed_at=data.get("completed_at"),
        failed_at=data.get("failed_at"),
        cancelled_at=data.get("cancelled_at"),
        error=error,
        usage=usage,
        instructions=data.get("instructions"),
        previous_response_id=data.get("previous_response_id"),
        tools=tools,
        tool_choice=data.get("tool_choice"),
        temperature=data.get("temperature"),
        top_p=data.get("top_p"),
        max_output_tokens=data.get("max_output_tokens"),
        frequency_penalty=data.get("frequency_penalty"),
        presence_penalty=data.get("presence_penalty"),
        seed=data.get("seed"),
        truncation=data.get("truncation"),
        parallel_tool_calls=data.get("parallel_tool_calls"),
        store=data.get("store"),
        metadata=data.get("metadata"),
        reasoning=reasoning,
        text=text,
        user=data.get("user"),
        incomplete_details=incomplete,
        _raw=data,
    )


# ---------------------------------------------------------------------------
# Delete / List helpers
# ---------------------------------------------------------------------------

@dataclass
class DeleteResponseResult:
    id: str = ""
    object: str = ""
    deleted: bool = False


@dataclass
class InputItemsListResponse:
    object: str = "list"
    data: List[Any] = field(default_factory=list)


@dataclass
class ListResponsesResult:
    object: str = "list"
    data: List[ResponseObject] = field(default_factory=list)


# ---------------------------------------------------------------------------
# Streaming Events
# ---------------------------------------------------------------------------

@dataclass
class ResponseLifecycleEvent:
    """`response.created` / `queued` / `in_progress` / `completed` / `failed` / `incomplete`."""
    type: str = ""
    response: Optional[ResponseObject] = None
    sequence_number: int = 0


@dataclass
class OutputItemAddedEvent:
    item_id: str = ""
    output_index: int = 0
    item: Any = None
    sequence_number: int = 0
    type: Literal["response.output_item.added"] = "response.output_item.added"


@dataclass
class OutputItemDoneEvent:
    item_id: str = ""
    output_index: int = 0
    item: Any = None
    sequence_number: int = 0
    type: Literal["response.output_item.done"] = "response.output_item.done"


@dataclass
class ContentPartAddedEvent:
    item_id: str = ""
    content_index: int = 0
    part: Any = None
    sequence_number: int = 0
    type: Literal["response.content_part.added"] = "response.content_part.added"


@dataclass
class ContentPartDoneEvent:
    item_id: str = ""
    content_index: int = 0
    part: Any = None
    sequence_number: int = 0
    type: Literal["response.content_part.done"] = "response.content_part.done"


@dataclass
class OutputTextDeltaEvent:
    item_id: str = ""
    output_index: int = 0
    content_index: int = 0
    delta: str = ""
    sequence_number: int = 0
    type: Literal["response.output_text.delta"] = "response.output_text.delta"


@dataclass
class OutputTextDoneEvent:
    item_id: str = ""
    output_index: int = 0
    content_index: int = 0
    text: str = ""
    sequence_number: int = 0
    type: Literal["response.output_text.done"] = "response.output_text.done"


@dataclass
class OutputTextAnnotationAddedEvent:
    item_id: str = ""
    annotation: Any = None
    sequence_number: int = 0
    type: Literal["response.output_text.annotation.added"] = "response.output_text.annotation.added"


@dataclass
class RefusalDeltaEvent:
    item_id: str = ""
    content_index: int = 0
    delta: str = ""
    sequence_number: int = 0
    type: Literal["response.refusal.delta"] = "response.refusal.delta"


@dataclass
class RefusalDoneEvent:
    item_id: str = ""
    content_index: int = 0
    refusal: str = ""
    sequence_number: int = 0
    type: Literal["response.refusal.done"] = "response.refusal.done"


@dataclass
class FunctionCallArgsDeltaEvent:
    item_id: str = ""
    output_index: int = 0
    delta: str = ""
    sequence_number: int = 0
    type: Literal["response.function_call_arguments.delta"] = "response.function_call_arguments.delta"


@dataclass
class FunctionCallArgsDoneEvent:
    item_id: str = ""
    output_index: int = 0
    arguments: str = ""
    name: str = ""
    sequence_number: int = 0
    type: Literal["response.function_call_arguments.done"] = "response.function_call_arguments.done"


@dataclass
class ReasoningSummaryPartAddedEvent:
    item_id: str = ""
    part: Any = None
    sequence_number: int = 0
    type: Literal["response.reasoning_summary_part.added"] = "response.reasoning_summary_part.added"


@dataclass
class ReasoningSummaryPartDoneEvent:
    item_id: str = ""
    part: Any = None
    sequence_number: int = 0
    type: Literal["response.reasoning_summary_part.done"] = "response.reasoning_summary_part.done"


@dataclass
class ReasoningDeltaEvent:
    item_id: str = ""
    delta: str = ""
    sequence_number: int = 0
    type: Literal["response.reasoning.delta"] = "response.reasoning.delta"


@dataclass
class ReasoningDoneEvent:
    item_id: str = ""
    text: str = ""
    sequence_number: int = 0
    type: Literal["response.reasoning.done"] = "response.reasoning.done"


@dataclass
class ReasoningSummaryTextDeltaEvent:
    item_id: str = ""
    delta: str = ""
    sequence_number: int = 0
    type: Literal["response.reasoning_summary_text.delta"] = "response.reasoning_summary_text.delta"


@dataclass
class ReasoningSummaryTextDoneEvent:
    item_id: str = ""
    text: str = ""
    sequence_number: int = 0
    type: Literal["response.reasoning_summary_text.done"] = "response.reasoning_summary_text.done"


@dataclass
class StreamingErrorEvent:
    code: Optional[str] = None
    message: Optional[str] = None
    param: Optional[str] = None
    sequence_number: int = 0
    type: Literal["error"] = "error"


@dataclass
class UnknownStreamingEvent:
    """Fallback for event types that aren't yet modeled."""
    type: str = ""
    sequence_number: int = 0
    data: Optional[Dict[str, Any]] = None


StreamingEvent = Union[
    ResponseLifecycleEvent,
    OutputItemAddedEvent,
    OutputItemDoneEvent,
    ContentPartAddedEvent,
    ContentPartDoneEvent,
    OutputTextDeltaEvent,
    OutputTextDoneEvent,
    OutputTextAnnotationAddedEvent,
    RefusalDeltaEvent,
    RefusalDoneEvent,
    FunctionCallArgsDeltaEvent,
    FunctionCallArgsDoneEvent,
    ReasoningSummaryPartAddedEvent,
    ReasoningSummaryPartDoneEvent,
    ReasoningDeltaEvent,
    ReasoningDoneEvent,
    ReasoningSummaryTextDeltaEvent,
    ReasoningSummaryTextDoneEvent,
    StreamingErrorEvent,
    UnknownStreamingEvent,
]


_LIFECYCLE_TYPES = {
    "response.created",
    "response.queued",
    "response.in_progress",
    "response.completed",
    "response.failed",
    "response.incomplete",
}


def parse_streaming_event(data: Dict[str, Any]) -> StreamingEvent:
    """Build a typed streaming-event dataclass from a server-sent JSON payload."""
    t = data.get("type", "")
    seq = data.get("sequence_number", 0)

    if t in _LIFECYCLE_TYPES:
        resp_raw = data.get("response")
        resp = _parse_response_object(resp_raw) if isinstance(resp_raw, dict) else None
        return ResponseLifecycleEvent(type=t, response=resp, sequence_number=seq)

    if t == "response.output_item.added":
        item = data.get("item")
        return OutputItemAddedEvent(
            item_id=data.get("item_id", ""),
            output_index=data.get("output_index", 0),
            item=_parse_response_item(item) if isinstance(item, dict) else item,
            sequence_number=seq,
        )
    if t == "response.output_item.done":
        item = data.get("item")
        return OutputItemDoneEvent(
            item_id=data.get("item_id", ""),
            output_index=data.get("output_index", 0),
            item=_parse_response_item(item) if isinstance(item, dict) else item,
            sequence_number=seq,
        )
    if t == "response.content_part.added":
        part = data.get("part")
        return ContentPartAddedEvent(
            item_id=data.get("item_id", ""),
            content_index=data.get("content_index", 0),
            part=_parse_content_part(part) if isinstance(part, dict) else part,
            sequence_number=seq,
        )
    if t == "response.content_part.done":
        part = data.get("part")
        return ContentPartDoneEvent(
            item_id=data.get("item_id", ""),
            content_index=data.get("content_index", 0),
            part=_parse_content_part(part) if isinstance(part, dict) else part,
            sequence_number=seq,
        )
    if t == "response.output_text.delta":
        return OutputTextDeltaEvent(
            item_id=data.get("item_id", ""),
            output_index=data.get("output_index", 0),
            content_index=data.get("content_index", 0),
            delta=data.get("delta", ""),
            sequence_number=seq,
        )
    if t == "response.output_text.done":
        return OutputTextDoneEvent(
            item_id=data.get("item_id", ""),
            output_index=data.get("output_index", 0),
            content_index=data.get("content_index", 0),
            text=data.get("text", ""),
            sequence_number=seq,
        )
    if t == "response.output_text.annotation.added":
        return OutputTextAnnotationAddedEvent(
            item_id=data.get("item_id", ""),
            annotation=data.get("annotation"),
            sequence_number=seq,
        )
    if t == "response.refusal.delta":
        return RefusalDeltaEvent(
            item_id=data.get("item_id", ""),
            content_index=data.get("content_index", 0),
            delta=data.get("delta", ""),
            sequence_number=seq,
        )
    if t == "response.refusal.done":
        return RefusalDoneEvent(
            item_id=data.get("item_id", ""),
            content_index=data.get("content_index", 0),
            refusal=data.get("refusal", ""),
            sequence_number=seq,
        )
    if t == "response.function_call_arguments.delta":
        return FunctionCallArgsDeltaEvent(
            item_id=data.get("item_id", ""),
            output_index=data.get("output_index", 0),
            delta=data.get("delta", ""),
            sequence_number=seq,
        )
    if t == "response.function_call_arguments.done":
        return FunctionCallArgsDoneEvent(
            item_id=data.get("item_id", ""),
            output_index=data.get("output_index", 0),
            arguments=data.get("arguments", ""),
            name=data.get("name", ""),
            sequence_number=seq,
        )
    if t == "response.reasoning_summary_part.added":
        return ReasoningSummaryPartAddedEvent(
            item_id=data.get("item_id", ""), part=data.get("part"), sequence_number=seq
        )
    if t == "response.reasoning_summary_part.done":
        return ReasoningSummaryPartDoneEvent(
            item_id=data.get("item_id", ""), part=data.get("part"), sequence_number=seq
        )
    if t == "response.reasoning.delta":
        return ReasoningDeltaEvent(
            item_id=data.get("item_id", ""), delta=data.get("delta", ""), sequence_number=seq
        )
    if t == "response.reasoning.done":
        return ReasoningDoneEvent(
            item_id=data.get("item_id", ""), text=data.get("text", ""), sequence_number=seq
        )
    if t == "response.reasoning_summary_text.delta":
        return ReasoningSummaryTextDeltaEvent(
            item_id=data.get("item_id", ""), delta=data.get("delta", ""), sequence_number=seq
        )
    if t == "response.reasoning_summary_text.done":
        return ReasoningSummaryTextDoneEvent(
            item_id=data.get("item_id", ""), text=data.get("text", ""), sequence_number=seq
        )
    if t == "error":
        return StreamingErrorEvent(
            code=data.get("code"),
            message=data.get("message"),
            param=data.get("param"),
            sequence_number=seq,
        )

    return UnknownStreamingEvent(type=t, sequence_number=seq, data=data)


def _parse_delete_result(data: Dict[str, Any]) -> DeleteResponseResult:
    return DeleteResponseResult(
        id=data.get("id", ""),
        object=data.get("object", ""),
        deleted=bool(data.get("deleted", False)),
    )


def _parse_input_items_list(data: Dict[str, Any]) -> InputItemsListResponse:
    raw = data.get("data") or []
    return InputItemsListResponse(
        object=data.get("object", "list"),
        data=[_parse_response_item(i) if isinstance(i, dict) else i for i in raw],
    )


def _parse_list_responses(data: Dict[str, Any]) -> ListResponsesResult:
    raw = data.get("data") or []
    return ListResponsesResult(
        object=data.get("object", "list"),
        data=[_parse_response_object(r) if isinstance(r, dict) else r for r in raw],
    )


__all__ = [
    # Content parts
    "InputTextContent",
    "InputImageContent",
    "InputFileContent",
    "OutputTextContent",
    "RefusalContent",
    "ContentPart",
    # Items
    "MessageItem",
    "FunctionCallItem",
    "FunctionCallOutputItem",
    "ItemReference",
    "ReasoningItem",
    "ResponseInputItem",
    "ResponseOutputItem",
    # Tools & config
    "FunctionToolDefinition",
    "FunctionToolChoice",
    "ToolChoice",
    "TextFormat",
    "TextConfig",
    "ReasoningConfig",
    # Response
    "ResponseObject",
    "ResponseUsage",
    "ResponseError",
    "IncompleteDetails",
    "DeleteResponseResult",
    "InputItemsListResponse",
    "ListResponsesResult",
    # Streaming events
    "StreamingEvent",
    "ResponseLifecycleEvent",
    "OutputItemAddedEvent",
    "OutputItemDoneEvent",
    "ContentPartAddedEvent",
    "ContentPartDoneEvent",
    "OutputTextDeltaEvent",
    "OutputTextDoneEvent",
    "OutputTextAnnotationAddedEvent",
    "RefusalDeltaEvent",
    "RefusalDoneEvent",
    "FunctionCallArgsDeltaEvent",
    "FunctionCallArgsDoneEvent",
    "ReasoningSummaryPartAddedEvent",
    "ReasoningSummaryPartDoneEvent",
    "ReasoningDeltaEvent",
    "ReasoningDoneEvent",
    "ReasoningSummaryTextDeltaEvent",
    "ReasoningSummaryTextDoneEvent",
    "StreamingErrorEvent",
    "UnknownStreamingEvent",
    "parse_streaming_event",
]
