# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum


class FinishReason(IntEnum):
    NONE = 0
    ERROR = 1
    STOP = 2
    LENGTH = 3
    TOOL_CALLS = 4


@dataclass(frozen=True)
class TokenUsage:
    prompt_tokens: int
    completion_tokens: int
    total_tokens: int


class SessionParam:
    """Well-known parameter key constants for Session.set_options / Request.set_options.

    Values must be string representations: floats as "0.7", ints as "256",
    bools as "true"/"false". Arbitrary keys beyond these are also accepted —
    the native layer passes them through.
    """

    Temperature = "temperature"        # float [0.0, 2.0]. Default is model-specific.
    TopP = "top_p"                     # float [0.0, 1.0]. Nucleus sampling.
    TopK = "top_k"                     # int. Top-k sampling.
    MaxOutputTokens = "max_output_tokens"  # int. Max tokens to generate.
    FrequencyPenalty = "frequency_penalty"  # float [-2.0, 2.0].
    PresencePenalty = "presence_penalty"    # float [-2.0, 2.0].
    Seed = "seed"                      # int. For reproducible outputs.
    EarlyStopping = "early_stopping"   # bool ("true"/"false"). Stop on stop sequence vs. only at max tokens.
    ToolChoice = "tool_choice"         # string: "auto", "none", or "required".
