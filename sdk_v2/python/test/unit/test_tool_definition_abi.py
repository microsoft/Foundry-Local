# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Unit tests for the flToolDefinition binding layout and API version stamping.

The cffi extension is compiled against the real ``foundry_local_c.h``, so these assertions are the
Python-side half of the ABI contract: the struct this package fills in has to be the struct the
native library reads, and the version stamped on it has to be the version requested from
``FoundryLocalGetApi``. No model is loaded.
"""
from __future__ import annotations

from foundry_local_sdk._native import ffi
from foundry_local_sdk._native import _cffi_bindings
from foundry_local_sdk._native.api import _FOUNDRY_LOCAL_API_VERSION
from foundry_local_sdk.session import _TOOL_KIND_CUSTOM, _TOOL_KIND_FUNCTION, _API_VERSION


class TestToolDefinitionAbi:
    def test_kind_is_appended_after_the_legacy_prefix(self):
        pointer_size = ffi.sizeof("void *")
        assert ffi.offsetof("flToolDefinition", "json_schema") == 3 * pointer_size
        assert ffi.offsetof("flToolDefinition", "kind") == 4 * pointer_size
        assert ffi.sizeof("flToolDefinition") == 5 * pointer_size

    def test_kind_is_a_fixed_width_32_bit_field(self):
        # flToolKind is a uint32_t typedef, not an enum, so its width is the same on every
        # toolchain and every binding lays the struct out identically.
        assert ffi.sizeof("flToolKind") == 4

    def test_tool_kind_values_match_the_header(self):
        # Read back through the compiled extension, so these are the header's values rather than a
        # second copy of them.
        assert _TOOL_KIND_FUNCTION == _cffi_bindings.lib.FOUNDRY_LOCAL_TOOL_KIND_FUNCTION
        assert _TOOL_KIND_CUSTOM == _cffi_bindings.lib.FOUNDRY_LOCAL_TOOL_KIND_CUSTOM
        assert _TOOL_KIND_FUNCTION == 0
        assert _TOOL_KIND_CUSTOM == 1

    def test_stamped_version_matches_the_requested_version(self):
        # Stamping a version the requested API table does not support would be silently wrong: the
        # native side reads `kind` only from a version 2 definition.
        assert _API_VERSION == _FOUNDRY_LOCAL_API_VERSION
        assert _API_VERSION == 2

    def test_definition_round_trips_through_the_struct(self):
        c_name = ffi.new("char[]", b"apply_patch\x00")
        c_desc = ffi.new("char[]", b"applies a patch\x00")
        c_schema = ffi.new("char[]", b"\x00")

        definition = ffi.new("flToolDefinition*")
        definition.version = _API_VERSION
        definition.name = c_name
        definition.description = c_desc
        definition.json_schema = c_schema
        definition.kind = _TOOL_KIND_CUSTOM

        assert definition.version == 2
        assert ffi.string(definition.name) == b"apply_patch"
        assert ffi.string(definition.json_schema) == b""
        assert definition.kind == _TOOL_KIND_CUSTOM
