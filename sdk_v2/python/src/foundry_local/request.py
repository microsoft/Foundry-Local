# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from foundry_local.items import Item

_API_VERSION = 1  # FOUNDRY_LOCAL_API_VERSION


class Request:
    """Inference request. Owns its native flRequest* handle.

    Supports fluent chaining — add_item() and set_options() return self.

    Items added via add_item() transfer native ownership to the request.
    Do not use the item after adding it.
    """

    def __init__(self) -> None:
        from foundry_local._native import ffi
        from foundry_local._native.api import api

        out = ffi.new("flRequest**")
        api.check_status(api.inference.Request_Create(out))
        self._ptr = out[0]
        self._closed = False

    def add_item(self, item: "Item") -> "Request":
        """Add item to request. Transfers native ownership — do not use item after this."""
        from foundry_local._native.api import api

        native_ptr = item._release_ownership()
        api.check_status(api.inference.Request_AddItem(self._ptr, native_ptr, True))
        return self

    @property
    def item_count(self) -> int:
        from foundry_local._native.api import api

        return int(api.inference.Request_GetItemCount(self._ptr))

    def get_item(self, index: int) -> "Item":
        from foundry_local._native import ffi
        from foundry_local._native.api import api
        from foundry_local.items import Item

        out = ffi.new("flItem**")
        api.check_status(api.inference.Request_GetItem(self._ptr, index, out))
        return Item.from_native(out[0], owns=False)

    def set_options(self, options: dict[str, str]) -> "Request":
        """Set per-request inference options. Overrides session-level options for this request."""
        from foundry_local._native import ffi
        from foundry_local._native.api import api

        kvp_out = ffi.new("flKeyValuePairs**")
        api.root.CreateKeyValuePairs(kvp_out)
        kvp = kvp_out[0]
        try:
            for key, value in options.items():
                api.root.AddKeyValuePair(kvp, key.encode("utf-8"), str(value).encode("utf-8"))
            api.check_status(api.inference.Request_SetOptions(self._ptr, kvp))
        finally:
            api.root.KeyValuePairs_Release(kvp)
        return self

    def cancel(self) -> None:
        """Signal cancellation for an in-flight request."""
        from foundry_local._native.api import api

        api.check_status(api.inference.Request_Cancel(self._ptr))

    def _close(self) -> None:
        if not self._closed and getattr(self, "_ptr", None) is not None:
            try:
                from foundry_local._native.api import api

                api.inference.Request_Release(self._ptr)
            except Exception:
                pass
            self._ptr = None
            self._closed = True

    def __enter__(self) -> "Request":
        return self

    def __exit__(self, *_) -> None:
        self._close()

    def __del__(self) -> None:
        self._close()
