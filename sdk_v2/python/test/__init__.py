# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Unit tests for FinishReason, TokenUsage, and SessionParam constants."""
from __future__ import annotations

import dataclasses

import pytest

from foundry_local_sdk import FinishReason, SessionParam, TokenUsage


class TestFinishReason:
    def test_values_match_native_c_enum(self):
        # Mirrors flFinishReason in foundry_local_c.h.
        assert FinishReason.NONE == 0
        assert FinishReason.ERROR == 1
        assert FinishReason.STOP == 2
        assert FinishReason.LENGTH == 3
        assert FinishReason.TOOL_CALLS == 4

    def test_int_construction_preserves_identity(self):
        assert FinishReason(2) is FinishReason.STOP

    def test_unknown_value_raises(self):
        with pytest.raises(ValueError):
            FinishReason(999)


class TestTokenUsage:
    def test_is_frozen_dataclass(self):
        u = TokenUsage(prompt_tokens=10, completion_tokens=20, total_tokens=30)
        with pytest.raises(dataclasses.FrozenInstanceError):
            u.prompt_tokens = 0  # type: ignore[misc]

    def test_field_round_trip(self):
        u = TokenUsage(prompt_tokens=1, completion_tokens=2, total_tokens=3)
        assert (u.prompt_tokens, u.completion_tokens, u.total_tokens) == (1, 2, 3)

    def test_equality_by_value(self):
        a = TokenUsage(1, 2, 3)
        b = TokenUsage(1, 2, 3)
        c = TokenUsage(1, 2, 4)
        assert a == b
        assert a != c


class TestSessionParam:
    def test_param_names_are_strings(self):
        # The native layer treats parameter names as plain strings — these
        # must remain stable across SDK versions.
        assert SessionParam.Temperature == "temperature"
        assert SessionParam.TopP == "top_p"
        assert SessionParam.TopK == "top_k"
        assert SessionParam.MaxOutputTokens == "max_output_tokens"
        assert SessionParam.FrequencyPenalty == "frequency_penalty"
        assert SessionParam.PresencePenalty == "presence_penalty"
        assert SessionParam.Seed == "seed"
        assert SessionParam.EarlyStopping == "early_stopping"
        assert SessionParam.ToolChoice == "tool_choice"
