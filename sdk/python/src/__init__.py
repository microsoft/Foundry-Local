# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
import logging
import sys

from .configuration import Configuration
from .foundry_local_manager import FoundryLocalManager
from .openai.responses_client import ResponsesClient, ResponsesClientSettings
from .openai.responses_types import (
    ContentPart,
    DeleteResponseResult,
    FunctionCallItem,
    FunctionCallOutputItem,
    FunctionToolDefinition,
    InputFileContent,
    InputImageContent,
    InputItemsListResponse,
    InputTextContent,
    ItemReference,
    ListResponsesResult,
    MessageItem,
    OutputTextContent,
    ReasoningConfig,
    ReasoningItem,
    RefusalContent,
    ResponseError,
    ResponseInputItem,
    ResponseObject,
    ResponseOutputItem,
    ResponseUsage,
    StreamingEvent,
    TextConfig,
    TextFormat,
    parse_streaming_event,
)
from .version import __version__

_logger = logging.getLogger(__name__)
_logger.setLevel(logging.WARNING)

_sc = logging.StreamHandler(stream=sys.stdout)
_formatter = logging.Formatter(
    "[foundry-local] | %(asctime)s | %(levelname)-8s | %(message)s", datefmt="%Y-%m-%d %H:%M:%S"
)
_sc.setFormatter(_formatter)
_logger.addHandler(_sc)
_logger.propagate = False

__all__ = [
    "Configuration",
    "ContentPart",
    "DeleteResponseResult",
    "FoundryLocalManager",
    "FunctionCallItem",
    "FunctionCallOutputItem",
    "FunctionToolDefinition",
    "InputFileContent",
    "InputImageContent",
    "InputItemsListResponse",
    "InputTextContent",
    "ItemReference",
    "ListResponsesResult",
    "MessageItem",
    "OutputTextContent",
    "ReasoningConfig",
    "ReasoningItem",
    "RefusalContent",
    "ResponseError",
    "ResponseInputItem",
    "ResponseObject",
    "ResponseOutputItem",
    "ResponseUsage",
    "ResponsesClient",
    "ResponsesClientSettings",
    "StreamingEvent",
    "TextConfig",
    "TextFormat",
    "__version__",
    "parse_streaming_event",
]
