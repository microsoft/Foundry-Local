# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from __future__ import annotations

from foundry_local_sdk.items import Item, ItemType


class ItemQueue(Item):
    """A queue of items used for streaming.

    Used for streaming input data (e.g. realtime audio chunks pushed into a live transcription session) and output
    data (e.g. streaming tokens delivered via a streaming callback).

    Mirrors the C++ ``struct ItemQueue : Item`` — a queue *is* an item (carries item type tag ``ItemType.QUEUE``).
    The native ABI stores all handles as ``flItem*`` and dispatches internally on the type tag, so ``self._ptr`` is
    a ``flItem*`` like every other ``Item`` subclass. The typed ``ItemQueue_*`` accessors require a ``flItemQueue*``
    — cffi will not implicitly convert pointer types, so each call site casts locally. ``Item_Release`` (the
    inherited ``_close`` path) is polymorphic via the ``Item`` virtual destructor and correctly destroys queues, so
    no ``_close`` override is needed.
    """

    def __init__(self) -> None:
        from foundry_local_sdk._native import ffi
        from foundry_local_sdk._native.api import api

        out = ffi.new("flItemQueue**")
        api.check_status(api.item.ItemQueue_Create(out))
        # Store as flItem* to keep storage uniform across the Item hierarchy.
        super().__init__(ffi.cast("flItem*", out[0]), owns=True)

    @property
    def item_type(self) -> ItemType:
        # Known statically; avoid the round-trip through native GetType.
        return ItemType.QUEUE

    def push(self, item: Item) -> None:
        """Push an item into the queue. Transfers ownership of the item."""
        from foundry_local_sdk._native import ffi
        from foundry_local_sdk._native.api import api

        native_ptr = item._release_ownership()
        api.check_status(
            api.item.ItemQueue_Push(ffi.cast("flItemQueue*", self._ptr), native_ptr)
        )

    def try_pop(self) -> Item | None:
        """Pop an item from the queue, or return None if empty."""
        from foundry_local_sdk._native import ffi
        from foundry_local_sdk._native.api import api

        out = ffi.new("flItem**")
        if api.item.ItemQueue_TryPop(ffi.cast("flItemQueue*", self._ptr), out):
            return Item.from_native(out[0], owns=True)
        return None

    @property
    def size(self) -> int:
        from foundry_local_sdk._native import ffi
        from foundry_local_sdk._native.api import api

        return int(api.item.ItemQueue_Size(ffi.cast("flItemQueue*", self._ptr)))

    def mark_finished(self) -> None:
        """Mark the queue as finished — no more items will be pushed."""
        from foundry_local_sdk._native import ffi
        from foundry_local_sdk._native.api import api

        api.item.ItemQueue_MarkFinished(ffi.cast("flItemQueue*", self._ptr))

    @property
    def is_finished(self) -> bool:
        from foundry_local_sdk._native import ffi
        from foundry_local_sdk._native.api import api

        return bool(api.item.ItemQueue_IsFinished(ffi.cast("flItemQueue*", self._ptr)))
