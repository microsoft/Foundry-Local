# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

import json
import logging
import sys

if sys.version_info >= (3, 11):
    from enum import StrEnum
else:
    from enum import Enum

    class StrEnum(str, Enum):
        def __str__(self) -> str:
            return self.value

from ..exception import FoundryLocalException

from .core_interop import CoreInterop

logger = logging.getLogger(__name__)


def get_cached_model_ids(core_interop: CoreInterop) -> list[str]:
    """Get the list of models that have been downloaded and are cached."""

    response = core_interop.execute_command("get_cached_models")
    if response.error is not None:
        raise FoundryLocalException(f"Failed to get cached models: {response.error}")

    # response is json array of strings
    try:
        model_ids = json.loads(response.data)
    except json.JSONDecodeError as e:
        raise FoundryLocalException(f"Failed to decode JSON response: Response was: {response.data}") from e

    return model_ids
