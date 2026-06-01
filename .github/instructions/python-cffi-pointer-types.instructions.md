---
description: Use when working on the Python SDK's native bindings (cffi), especially when defining new Item subclasses, working with flItemQueue, or extending Request/Session methods that pass native handles.
applyTo: sdk_v2/python/src/foundry_local_sdk/**
---

# cffi Pointer Typing Across the Item Hierarchy

cffi is strict about pointer types — it will not implicitly convert between
distinct pointer types even when the C++ inheritance relationship makes the
upcast/downcast safe at the ABI level. The C# SDK escapes this entirely
because `IntPtr` is untyped.

## The native ABI convention: everything is `flItem*`

Every item type-specific function in `foundry_local_c.h` takes `flItem*`
and dispatches internally on the item's type tag (`GetType()`):

```c
flStatus Create(int item_type, flItem** out);
flStatus SetText(flItem* item, const flTextData* data);
flStatus SetBytes(flItem* item, const flBytesData* data);
flStatus SetAudio(flItem* item, const flAudioData* data);
// ... etc.
```

`flItemQueue*` is **the one exception** — it has a small set of typed
accessors (`ItemQueue_Create`, `ItemQueue_Push`, `ItemQueue_TryPop`,
`ItemQueue_Size`, `ItemQueue_MarkFinished`, `ItemQueue_IsFinished`,
`ItemQueue_Release`). At the C++ level `ItemQueue : Item`
(see [items/item_queue.h](../../sdk_v2/cpp/src/items/item_queue.h)),
so the upcast is real and well-defined (queue carries item type tag
`FOUNDRY_LOCAL_ITEM_QUEUE = 200`).

## Storage rule: every `Item` subclass stores `flItem*` in `self._ptr`

This includes `ItemQueue`, even though native creation returns
`flItemQueue**`. Cast at construction time:

```python
out = ffi.new("flItemQueue**")
api.check_status(api.item.ItemQueue_Create(out))
super().__init__(ffi.cast("flItem*", out[0]), owns=True)
```

This keeps the storage type uniform across the hierarchy. As a result:

- `Request.add_item(item: Item)` is naturally polymorphic — no cast needed
  at the call site, no duck-typing branch.
- `Item._close` calling `Item_Release(self._ptr)` is well-typed for every
  subclass, including queues (the C++ destructor is virtual).
- `Item.item_type` and other base-class accessors that take `flItem*` work
  for every subclass.

## Where the cast lives: inside `ItemQueue`'s typed accessors

Cast back to `flItemQueue*` only inside the queue's own methods that
must call the typed `ItemQueue_*` ABI:

```python
def push(self, item: Item) -> None:
    api.check_status(
        api.item.ItemQueue_Push(
            ffi.cast("flItemQueue*", self._ptr),
            item._release_ownership(),
        )
    )
```

The casts are localized to the file that owns the typed accessors.
Consumers (Request, Session, OpenAI clients) never need to think about it.

## How to spot a regression

Without the right cast, cffi raises `TypeError` at the call site, not at
native call time. The error message is precise:

> `initializer for ctype 'flItemQueue *' must be a pointer to same type, not cdata 'flItem *'`

If you ever see this from a queue-typed accessor, the fix is to add a
`ffi.cast("flItemQueue*", self._ptr)` at that single call site — never to
change the storage type of `self._ptr`.
