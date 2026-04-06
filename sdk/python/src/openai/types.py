# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

"""OpenAI-compatible DTO types for chat completions."""

from __future__ import annotations

from typing import Any, Dict, List, Optional

from pydantic import BaseModel


# Type alias for chat completion message parameters (plain dicts).
ChatCompletionMessageParam = Dict[str, Any]


class FunctionCall(BaseModel):
    """A function call within a tool call."""
    name: str
    arguments: str


class FunctionCallChunk(BaseModel):
    """Incremental function call in a streaming response."""
    name: Optional[str] = None
    arguments: Optional[str] = None


class ChatCompletionMessageToolCall(BaseModel):
    """A tool call in a chat completion response."""
    id: str
    type: str
    function: FunctionCall


class ChoiceToolCallChunk(BaseModel):
    """Incremental tool call in a streaming chunk."""
    index: int
    id: Optional[str] = None
    type: Optional[str] = None
    function: Optional[FunctionCallChunk] = None


class ChatCompletionMessage(BaseModel):
    """A message in a chat completion response."""
    role: str
    content: Optional[str] = None
    tool_calls: Optional[List[ChatCompletionMessageToolCall]] = None


class CompletionUsage(BaseModel):
    """Token usage statistics."""
    prompt_tokens: int
    completion_tokens: int
    total_tokens: int


class Choice(BaseModel):
    """A choice in a non-streaming chat completion response."""
    index: int
    message: ChatCompletionMessage
    finish_reason: Optional[str] = None
    logprobs: Optional[Any] = None


class ChatCompletion(BaseModel):
    """Non-streaming chat completion response."""
    id: str
    object: str
    created: int
    model: str
    choices: List[Choice]
    usage: Optional[CompletionUsage] = None
    system_fingerprint: Optional[str] = None


class ChoiceDelta(BaseModel):
    """Delta content in a streaming chunk."""
    role: Optional[str] = None
    content: Optional[str] = None
    tool_calls: Optional[List[ChoiceToolCallChunk]] = None


class StreamChoice(BaseModel):
    """A choice in a streaming chat completion chunk."""
    index: int
    delta: Optional[ChoiceDelta] = None
    finish_reason: Optional[str] = None
    logprobs: Optional[Any] = None


class ChatCompletionChunk(BaseModel):
    """Streaming chat completion chunk."""
    id: str
    object: str
    created: int
    model: str
    choices: List[StreamChoice]
    usage: Optional[CompletionUsage] = None
    system_fingerprint: Optional[str] = None
