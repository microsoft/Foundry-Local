# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

from typing import TYPE_CHECKING, Iterator

if TYPE_CHECKING:
    from foundry_local_sdk.items import Item
    from foundry_local_sdk.session_types import FinishReason, TokenUsage

_API_VERSION = 1  # FOUNDRY_LOCAL_API_VERSION


class Response:
    """Inference response. Owns its native flResponse* handle.

    Iterable: iterates over all output items.
    """

    def __init__(self, ptr) -> None:
        self._ptr = ptr
        self._closed = False

    @property
    def item_count(self) -> int:
        from foundry_local_sdk._native.api import api

        return int(api.inference.Response_GetItemCount(self._ptr))

    def get_item(self, index: int) -> "Item":
        from foundry_local_sdk._native import ffi
        from foundry_local_sdk._native.api import api
        from foundry_local_sdk.items import Item

        out = ffi.new("flItem**")
        api.check_status(api.inference.Response_GetItem(self._ptr, index, out))
        return Item.from_native(out[0], owns=False)

    @property
    def finish_reason(self) -> "FinishReason":
        from foundry_local_sdk._native.api import api
        from foundry_local_sdk.session_types import FinishReason

        return FinishReason(int(api.inference.Response_GetFinishReason(self._ptr)))

    def get_usage(self) -> "TokenUsage":
        from foundry_local_sdk._native import ffi
        from foundry_local_sdk._native.api import api
        from foundry_local_sdk.session_types import TokenUsage

        usage = ffi.new("flUsage*")
        # version field tells the C layer which fields are valid to fill in.
        usage.version = _API_VERSION
        api.check_status(api.inference.Response_GetUsage(self._ptr, usage))
        return TokenUsage(
            prompt_tokens=int(usage.prompt_tokens),
            completion_tokens=int(usage.completion_tokens),
            total_tokens=int(usage.total_tokens),
        )

    def __iter__(self) -> Iterator["Item"]:
        for i in range(self.item_count):
            yield self.get_item(i)

    def _close(self) -> None:
        if not self._closed and getattr(self, "_ptr", None) is not None:
            try:
                from foundry_local_sdk._native.api import api

                api.inference.Response_Release(self._ptr)
            except Exception:
                pass
            self._ptr = None
            self._closed = True

    def __enter__(self) -> "Response":
        return self

    def __exit__(self, *_) -> None:
        self._close()

    def __del__(self) -> None:
        self._close()
