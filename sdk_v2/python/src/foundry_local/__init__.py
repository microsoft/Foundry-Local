# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
import logging
import sys

from foundry_local.exception import FoundryLocalException
from foundry_local.version import __version__
from foundry_local.logging_helper import LogLevel
from foundry_local.configuration import Configuration
from foundry_local.foundry_local_manager import FoundryLocalManager
from foundry_local.catalog import Catalog
from foundry_local.imodel import IModel
from foundry_local.ep_types import EpInfo, EpDownloadResult
from foundry_local.model_info import (
    ModelInfo,
    PromptTemplate,
    Runtime,
    Parameter,
    ModelSettings,
    DeviceType,
)
from foundry_local.items import (
    Item,
    TextItem,
    MessageItem,
    BytesItem,
    ImageItem,
    AudioItem,
    ToolCallItem,
    ToolResultItem,
    TensorItem,
    ItemType,
    TextItemType,
    MessageRole,
    TensorDataType,
)
from foundry_local.session_types import FinishReason, TokenUsage, SessionParam
from foundry_local.request import Request
from foundry_local.response import Response
from foundry_local.session import Session, ChatSession, AudioSession, EmbeddingsSession

_logger = logging.getLogger(__name__)
_logger.setLevel(logging.WARNING)
_sc = logging.StreamHandler(stream=sys.stdout)
_formatter = logging.Formatter(
    "[foundry-local] | %(asctime)s | %(levelname)-8s | %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
_sc.setFormatter(_formatter)
_logger.addHandler(_sc)
_logger.propagate = False

# Alias for callers who prefer the un-prefixed name.
Model = IModel

__all__ = [
    "FoundryLocalException",
    "__version__",
    "LogLevel",
    "Configuration",
    "FoundryLocalManager",
    "Catalog",
    "IModel",
    "Model",
    "ModelInfo",
    "PromptTemplate",
    "Runtime",
    "Parameter",
    "ModelSettings",
    "DeviceType",
    "EpInfo",
    "EpDownloadResult",
    "Item",
    "TextItem",
    "MessageItem",
    "BytesItem",
    "ImageItem",
    "AudioItem",
    "ToolCallItem",
    "ToolResultItem",
    "TensorItem",
    "ItemType",
    "TextItemType",
    "MessageRole",
    "TensorDataType",
    "FinishReason",
    "TokenUsage",
    "SessionParam",
    "Request",
    "Response",
    "Session",
    "ChatSession",
    "AudioSession",
    "EmbeddingsSession",
]
