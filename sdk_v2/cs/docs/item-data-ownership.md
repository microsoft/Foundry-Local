# Item Data Ownership Design

## Overview

Data-bearing items (`AudioItem`, `ImageItem`, `BytesItem`, `TensorItem`) carry binary data that
lives in native (unmanaged) memory. The C# SDK must bridge between managed `byte[]` / `Memory<byte>`
buffers and the native C API's pointer-based data model.

This document explains the four ownership/mutability states, how they map to the C API, and the
pinning infrastructure that makes it work.

## The Four States

| # | Mutability | Ownership | C# Constructor / Factory | Use Case |
|---|-----------|-----------|-------------------------|----------|
| 1 | Read-only | Borrowed  | `new AudioItem("wav", readOnlyMemory)` | Passing a `byte[]` or `ReadOnlyMemory<byte>` for a short-lived request. Caller keeps the data alive. |
| 2 | Mutable   | Borrowed  | `new AudioItem("wav", memory)` | Passing a `Memory<byte>` that native code may write into. Caller keeps the data alive. |
| 3 | Mutable   | Owned     | `AudioItem.CreateOwned("wav", memory)` | Streaming: push into `ItemQueue`, transfer ownership. Native frees (unpins) on item destruction. |
| 4 | Read-only | Owned     | `AudioItem.CreateOwned("wav", readOnlyMemory)` | Same as #3 but data is read-only. C#-only — works because "free" is just unpin, not write. |

### Why `Memory<T>` / `ReadOnlyMemory<T>`?

- `Span<T>` is stack-only — cannot be stored in a field. Items are heap objects with a lifetime, so
  they need `Memory<T>` to hold the reference.
- `byte[]` implicitly converts to both `Memory<byte>` and `ReadOnlyMemory<byte>`, so the common
  case is simple: `new AudioItem("wav", myByteArray)`.
- `Memory<T>.Pin()` returns a `MemoryHandle` with a stable pointer. The GC cannot move the data
  while pinned.

### Why not just copy?

Copying is simple but wrong for streaming scenarios. A real-time audio pipeline might push thousands
of small chunks per second into an `ItemQueue`. Copying each one doubles memory pressure. The pin
approach is zero-copy: the managed buffer IS the native buffer.

## C API Contract

The native C API uses versioned data structs with these fields:

```c
struct flAudioData {
    uint32_t version;
    const void* data;           // Read-only pointer. Always set.
    void* mutable_data;         // Writable pointer. NULL for read-only. NULL on Get.
    size_t data_size;
    const char* format;
    const char* uri;
    flAudioDataDeleter deleter; // Called on item destruction. NULL = not owned.
    void* deleter_user_data;    // Context passed to deleter.
};
```

Rules:
- `data` is always set (read-only view).
- `mutable_data` is set when the native code may write to the buffer, OR when a deleter is provided.
  The C API requires `mutable_data != NULL` when `deleter != NULL` because the deleter receives
  the struct and uses `mutable_data` to free.
- `deleter` is called exactly once when the native item is destroyed (`flItem_Release`).
- Deleters use Cdecl calling convention (plain C function pointers, not `FL_API_CALL`).

## How Each State Maps to the C API

### State 1: Read-only Borrowed

```
Memory<byte>.Pin() → stable pointer
Native struct: data = pinned_ptr, mutable_data = NULL, deleter = NULL
Item.Dispose() → unpin
```

The item pins the memory on construction. `Dispose()` unpins it. Native code can only read.

### State 2: Mutable Borrowed

```
Memory<byte>.Pin() → stable pointer
Native struct: data = pinned_ptr, mutable_data = pinned_ptr, deleter = NULL
Item.Dispose() → unpin
```

Same as state 1 but `mutable_data` is also set, allowing native code to write into the buffer.

### State 3: Mutable Owned

```
Memory<byte>.Pin() → PinContext → GCHandle.Alloc(PinContext)
Native struct: data = pinned_ptr, mutable_data = pinned_ptr,
               deleter = static_deleter_thunk, deleter_user_data = GCHandle ptr
Item wrapper does NOT hold the PinContext — it's held only by the GCHandle.
Item.ReleaseOwnership() → C# wrapper stops calling flItem_Release in Dispose.
                          Native item survives (in queue, request, etc.)
Native flItem_Release → deleter called → PinContext.ReleaseFromNative(userData) → unpin + free GCHandle
```

Key: the `PinContext` is kept alive by the `GCHandle`, not by the C# `Item` wrapper. This means
ownership can transfer (e.g., push to `ItemQueue`) and the C# `Item` can be disposed/GC'd, but
the pin survives until the native item is destroyed.

### State 4: Read-only Owned (C#-only)

Same as state 3 but `mutable_data` is set to the pinned pointer even though the data is logically
read-only. This is safe because:
1. The native `mutable_data` field is only used by the deleter to get a freeable pointer.
2. Our deleter never writes — it only unpins and frees the GCHandle.
3. The `data` pointer (read-only) is what native inference code actually reads from.

This state is impossible in C++ because the deleter receives `void*` and the caller typically
calls `free()` / `delete[]` which requires a non-const pointer to owned data. In C#, "freeing"
means unpinning — a metadata operation, not a write.

## PinContext

`PinContext` is an internal helper that encapsulates the pin lifetime:

```csharp
internal sealed class PinContext : IDisposable
{
    private MemoryHandle _handle;  // from Memory.Pin()
    public IntPtr Pointer { get; }
    public int Length { get; }

    // Pin memory and return stable pointer
    static PinContext Pin(ReadOnlyMemory<byte> data);
    static PinContext Pin(Memory<byte> data);

    // For owned path: alloc GCHandle so native deleter can find us
    IntPtr AllocForNativeDeleter();

    // Called by native deleter thunk — unpin + free GCHandle
    static void ReleaseFromNative(IntPtr userData);

    // For borrowed path: unpin in Item.Dispose()
    void Dispose();
}
```

### Borrowed path lifetime

```
new AudioItem("wav", data)
  └─ PinContext.Pin(data) → stored as field on AudioItem
       └─ _pinContext.Dispose() called from AudioItem.Dispose() → unpin
```

### Owned path lifetime

```
AudioItem.CreateOwned("wav", data)
  └─ PinContext.Pin(data)
       └─ pinCtx.AllocForNativeDeleter() → GCHandle keeps PinContext alive
       └─ AudioItem does NOT store pinCtx (it's held by GCHandle only)
       └─ Native struct gets deleter + deleter_user_data

ItemQueue.Push(audioItem)
  └─ audioItem.ReleaseOwnership() → C# says "I don't own the native handle anymore"
  └─ audioItem.Dispose() → no-op (ownership transferred)

... later, native destroys item in queue ...
  └─ flItem_Release → calls deleter(audioData, userData)
       └─ PinContext.ReleaseFromNative(userData) → unpin + free GCHandle
```

## Deleter Thunks

Each data type needs a static deleter function pointer. These are created once and reused:

```csharp
// In AudioItem.cs
[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
private delegate void AudioDeleterDelegate(ref FlAudioData data, IntPtr userData);

private static readonly AudioDeleterDelegate s_deleter = AudioDeleter;
private static readonly IntPtr s_deleterPtr = Marshal.GetFunctionPointerForDelegate(s_deleter);

private static void AudioDeleter(ref FlAudioData data, IntPtr userData)
{
    PinContext.ReleaseFromNative(userData);
}
```

The `s_deleter` field prevents the delegate from being GC'd. `s_deleterPtr` is the unmanaged
function pointer passed to the native struct's `Deleter` field.

## Data Property

All data-bearing items expose data via `ReadOnlySpan<byte>`:

```csharp
public ReadOnlySpan<byte> Data
{
    get
    {
        unsafe { return new ReadOnlySpan<byte>((void*)_data, _dataSize); }
    }
}
```

`_data` and `_dataSize` are read from the native item after `Set*()` (for user-created items)
or from `Get*()` (for native-returned items). The span is valid for the item's lifetime.

## Usage Examples

```csharp
// Simple: pass a byte array (borrowed, read-only)
byte[] wav = File.ReadAllBytes("audio.wav");
using var item = new AudioItem("wav", wav);  // byte[] → ReadOnlyMemory<byte>

// Streaming: owned, push to queue
byte[] chunk = GetNextAudioChunk();
var item = AudioItem.CreateOwned("pcm", (Memory<byte>)chunk);
queue.Push(item);  // ownership transfers — do not use item after this

// Mutable: native inference writes results into your buffer
var buffer = new byte[4096];
using var item = new AudioItem("pcm", (Memory<byte>)buffer);
// After inference, buffer contains the output data

// From URI (no data)
using var item = new AudioItem("path/to/audio.wav");
```
