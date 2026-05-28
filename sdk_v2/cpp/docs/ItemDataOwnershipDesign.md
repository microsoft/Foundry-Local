# Item Data Ownership & Const-Correctness Design

> Design document for the C API data struct redesign (`flBytesData`, `flTensorData`,
> `flImageData`, `flAudioData`). Covers const-correctness, ownership semantics, deleter
> embedding, and the removal of `flItemDeleter` / `SetDeleter`.

## Status

**Approved** — ready to implement. Pre-V1 (no shipped ABI to preserve).

## Problem Statement

The original data structs have three issues:

1. **`void* data` is neither const nor mutable.** Read-only input data (e.g. model weights)
   should be `const void*` so callers never need `const_cast`. But writable output tensors
   (e.g. pre-allocated inference output buffers) genuinely need a mutable pointer.

2. **`SetDeleter` is an item-level antipattern.** The deleter receives `flItem*` and must
   introspect the item to discover *what* to free. This is fragile, couples the deleter
   to item internals, and forces ownership to live in the base `Item` class rather than
   next to the data it governs.

3. **Ownership is implicit.** There's no way to tell from the struct alone whether the
   caller or the item owns the data buffer.

## Design Principles

- **No `const_cast` anywhere.** Read-only data uses `const void*`. Writable data uses
  `void*`. Both paths are first-class.
- **Mutability and ownership are independent axes.** A mutable buffer doesn't imply the
  item owns it. A const buffer with a deleter means the item owns it.
- **Self-documenting.** Deleter + user_data embedded in the data struct make ownership
  visible at the point of construction, not separated across two API calls.
- **Minimal surface.** No `GetMutable*` variants yet — the caller already has their
  mutable pointer. No separate `SetDeleter` call — it's part of the data struct.

## Agreed Design

### Per-Type Deleter Typedefs

Each data struct gets its own deleter typedef. The deleter receives a `const` pointer to
the data struct (giving direct access to all fields) plus an optional `void* user_data`
for additional context (allocator handles, etc.).

```c
typedef void (*flBytesDataDeleter)(const flBytesData* data, void* user_data);
typedef void (*flTensorDataDeleter)(const flTensorData* data, void* user_data);
typedef void (*flImageDataDeleter)(const flImageData* data, void* user_data);
typedef void (*flAudioDataDeleter)(const flAudioData* data, void* user_data);
```

**Why per-type instead of generic `void* user_data`-only?** A generic deleter forces the
caller to heap-allocate a context struct just to remember *what* to free. Per-type
deleters give the deleter direct access to the data pointer, shape, etc. — the common
case (free the buffer) requires zero allocation. The `void* user_data` remains as an
optional escape hatch for exotic cases (GPU allocator handles, etc.).

**Zero const_cast, even in the deleter.** If a deleter is provided, `mutable_data` is
required to be non-NULL (validated at Set time). The destructor populates `mutable_data`
in the temporary struct passed to the deleter, so the deleter can free via
`td->mutable_data` without casting away const. This is a natural invariant: if you're
transferring ownership, you have a mutable pointer to the thing you're giving away.

### Data Struct Changes

All four data-bearing structs (`flBytesData`, `flTensorData`, `flImageData`, `flAudioData`)
get the same pattern. Using `flTensorData` as the representative example:

```c
typedef struct flTensorData {
  uint32_t version;              ///< Set to FOUNDRY_LOCAL_API_VERSION.
  flTensorDataType data_type;    ///< Element data type.
  const void* data;              ///< Read-only data pointer. Always populated on Get.
  void* mutable_data;            ///< Writable data pointer. NULL for read-only data. NULL on Get.
  const int64_t* shape;          ///< Array of dimension sizes. Length is `rank`.
  size_t rank;                   ///< Number of dimensions.
  flTensorDataDeleter deleter;   ///< Optional. Called on item destruction to free owned data.
  void* deleter_user_data;       ///< Context for deleter. Ignored if deleter is NULL.
  /* V2 fields go here. */
} flTensorData;
```

Summary of field changes (same pattern for all four structs):

| Old | New | Notes |
|-----|-----|-------|
| `void* data` | `const void* data` | Now read-only. Always populated on Get. |
| *(new)* | `void* mutable_data` | Writable pointer for output tensors etc. NULL on Get. |
| *(new)* | `fl<Type>DataDeleter deleter` | Per-type embedded deleter. NULL = caller manages lifetime. |
| *(new)* | `void* deleter_user_data` | Context for deleter. |

### Set Rules (validated in `Item_Set*Impl`)

- At least one of `data` or `mutable_data` must be non-NULL.
- **If both are set, they must be equal** (point to the same buffer). Setting both to
  different values is an error — there is one buffer, not two.
- If only `mutable_data` is set (`data == NULL`), the implementation copies it to the
  internal `data` field so all read paths use `data` consistently.
- **If `deleter` is provided, `mutable_data` must be non-NULL.** Ownership transfer
  implies the caller has a mutable pointer to the data they're giving away. This
  eliminates `const_cast` everywhere — the destructor passes `mutable_data` to the
  deleter directly.
- `deleter` is optional regardless of which pointer is set. NULL = caller owns the data.
- If `deleter` is provided, it is called during item destruction. The destructor
  reconstructs the data struct on the stack with `mutable_data` populated, giving the
  deleter a non-const pointer to free.

### Get Rules

- `Get*` returns `data` populated (pointing to the internal read pointer).
- `mutable_data` is always NULL on Get.
- `deleter` and `deleter_user_data` are always NULL on Get (ownership is internal).

### Removals

Since this is pre-V1 (no ABI to preserve):

- **Remove `flItemDeleter` typedef** — replaced by per-type `fl<Type>DataDeleter` typedefs.
- **Remove `SetDeleter` from `flItemApi`** — deleter is now part of the Set call.
- **Remove `Item::SetDeleter()` and `deleter_`/`deleter_user_data_` from base `Item`** —
  ownership moves to each concrete item type.

### Internal Item Type Changes

Each concrete item type (`BytesItem`, `TensorItem`, `ImageItem`, `AudioItem`):

- `void* data` → `const void* data` + `void* mutable_data`
- Adds per-type deleter (e.g. `flTensorDataDeleter deleter_`) + `void* deleter_user_data_`.
- Destructor reconstructs the data struct on the stack, sets `mutable_data` from the
  internal field, and calls `deleter_(&data, deleter_user_data_)`. This gives the deleter
  a non-const pointer to free without any `const_cast`.
- `SetXxxData()` handles dual-pointer logic: if only `mutable_data` provided, copies to `data`.
- `GetApiData()` zeroes out `mutable_data`, `deleter`, and `deleter_user_data`.
- Constructors updated to accept the new fields.
- Copy deleted (unchanged — items with external data can't be safely copied).

## Usage Examples

### Read-only input (common case)

```c
float weights[] = { 1.0f, 2.0f, 3.0f };
int64_t shape[] = { 3 };
flTensorData td = {
    .version = FOUNDRY_LOCAL_API_VERSION,
    .data_type = FOUNDRY_LOCAL_TENSOR_FLOAT,
    .data = weights,          // const — no cast needed
    .mutable_data = NULL,
    .shape = shape,
    .rank = 1,
    .deleter = NULL,          // caller owns weights
    .deleter_user_data = NULL,
};
item_api->SetTensor(item, &td);
```

### Owned input — simple free (no heap allocation needed)

```c
void* buf = malloc(3 * sizeof(float));
// ... fill buf ...

void free_tensor(const flTensorData* td, void* /*user_data*/) {
    free(td->mutable_data);  // non-const — no cast needed
}

flTensorData td = {
    .version = FOUNDRY_LOCAL_API_VERSION,
    .data_type = FOUNDRY_LOCAL_TENSOR_FLOAT,
    .data = buf,
    .mutable_data = buf,     // required when deleter is set
    .shape = shape,
    .rank = 1,
    .deleter = free_tensor,
    .deleter_user_data = NULL,
};
item_api->SetTensor(item, &td);
// Item now owns buf — deleter fires on Item_Release.
```

### Owned input — GPU allocator via user_data

```c
void* gpu_buffer = allocate_gpu(1024);

void free_gpu_tensor(const flTensorData* td, void* user_data) {
    GpuAllocator* alloc = (GpuAllocator*)user_data;
    alloc->free(td->mutable_data);  // non-const — no cast needed
}

flTensorData td = {
    .version = FOUNDRY_LOCAL_API_VERSION,
    .data_type = FOUNDRY_LOCAL_TENSOR_FLOAT,
    .data = gpu_buffer,
    .mutable_data = gpu_buffer,  // required when deleter is set
    .shape = shape,
    .rank = 1,
    .deleter = free_gpu_tensor,
    .deleter_user_data = allocator,
};
item_api->SetTensor(item, &td);
```

### Writable output tensor (pre-allocated buffer)

```c
float output[256];
int64_t shape[] = { 256 };
flTensorData td = {
    .version = FOUNDRY_LOCAL_API_VERSION,
    .data_type = FOUNDRY_LOCAL_TENSOR_FLOAT,
    .data = NULL,              // will be set internally from mutable_data
    .mutable_data = output,    // writable — inference writes here
    .shape = shape,
    .rank = 1,
    .deleter = NULL,           // caller owns the buffer
    .deleter_user_data = NULL,
};
item_api->SetTensor(output_item, &td);
// After inference, output[] contains results. No const_cast needed.
```

## Alternatives Considered

### Single `void* data` (status quo)

Rejected — requires `const_cast` for read-only data, which the team strongly opposes.

### Single `const void* data` only

Handles the common read-only case cleanly but forces `const_cast` for writable output
tensors. Rejected because the writable output tensor use case is real and important.

### Separate `SetDeleter` API (status quo)

Deleter receives `flItem*`, requiring introspection to find what to free. Embedding
the deleter in the data struct co-locates ownership with the data it governs.

### Generic `flDataDeleter(void* user_data)` (single typedef)

Simpler (one typedef), but forces heap allocation for the common "free a buffer" case
because the deleter has no access to the data pointer. Per-type deleters avoid this by
giving the deleter direct read access to all struct fields.

### `const_cast` in Get for mutable data

Providing `GetMutableTensor` or similar was considered but rejected — the caller already
has their mutable pointer and doesn't need to retrieve it through the API.

## Impact

- **C header** (`foundry_local_c.h`): Data struct changes, new typedef, removal of
  `flItemDeleter` and `SetDeleter`.
- **C API implementation** (`c_api.cc`): Updated Set/Get implementations, removal of
  `Item_SetDeleterImpl`, updated `g_item_api` table.
- **Internal item types** (`bytes_item.h`, `tensor_item.h`, `image_item.h`, `audio_item.h`):
  Dual-pointer storage, per-item deleter, destructor changes.
- **Base Item class** (`item.h`, `item.cc`): Remove deleter members and destructor logic.
- **C# bindings** (`NativeMethods.cs`): Update `FlItemApi` struct layout and delegates.
- **Tests**: Update deleter tests to use new embedded deleter pattern.
- **C++ wrapper**: TODOs for deleter ctors become actionable with per-type deleters.
