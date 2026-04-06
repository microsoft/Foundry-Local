# Cancelling Model Downloads in Foundry Local SDK

This document describes how to cancel in-progress model downloads across all
Foundry Local SDK language bindings. The cancellation mechanism leverages the
native core's built-in support for cooperative cancellation via callback return
values.

## Architecture Overview

The Foundry Local native core exposes a C-ABI callback interface for streaming
operations like model downloads:

```
callback(data_ptr, length, user_data) -> int
    Returns:  0 = continue
              1 = cancel/stop
```

Each SDK wraps this native callback in a language-idiomatic way. When
cancellation is requested, the SDK's callback wrapper returns `1` at the next
invocation, causing the native core to abort the download cleanly.

**Key design constraint:** Cancellation is _cooperative_. The native core checks
the callback return value periodically during the download. There may be a short
delay between requesting cancellation and the download actually stopping.

## Language-Specific Cancellation APIs

### C# (.NET) — `CancellationToken`

C# has had cancellation support since the initial release.

```csharp
var cts = new CancellationTokenSource();

// Start download
var downloadTask = model.DownloadAsync(
    downloadProgress: progress => Console.Write($"\r{progress:F1}%"),
    ct: cts.Token
);

// Cancel after 3 seconds
cts.CancelAfter(TimeSpan.FromSeconds(3));

try
{
    await downloadTask;
}
catch (FoundryLocalException ex)
{
    Console.WriteLine($"Download cancelled: {ex.Message}");
}
```

**Interface:**
```csharp
Task DownloadAsync(Action<float>? downloadProgress = null,
                   CancellationToken? ct = null);
```

### Python — `threading.Event`

Python uses a `threading.Event` for signalling cancellation. When the event is
set, the next native callback invocation will return `1` (cancel).

```python
import threading, time

cancel = threading.Event()

# Start download on background thread
def do_download():
    try:
        model.download(
            progress_callback=lambda pct: print(f"  {pct:.1f}%", end="\r"),
            cancel_event=cancel,
        )
    except FoundryLocalException as e:
        print(f"Download cancelled: {e}")

t = threading.Thread(target=do_download)
t.start()

# Cancel after 3 seconds
time.sleep(3)
cancel.set()
t.join()
```

**Interface:**
```python
def download(self,
             progress_callback: Callable[[float], None] = None,
             cancel_event: Optional[threading.Event] = None) -> None
```

**Behavior:**
- When `cancel_event` is set, the callback returns `1` to the native core.
- A `FoundryLocalException` with message `"Operation cancelled"` is raised.
- `cancel_event` can be used without `progress_callback` (a no-op callback is
  used internally to engage the native callback mechanism).

### JavaScript / TypeScript — `AbortSignal`

The JS/TS SDK uses the standard Web `AbortController` / `AbortSignal` pattern.

```typescript
const controller = new AbortController();

// Start download
const downloadPromise = model.download(
    (progress) => process.stdout.write(`\r${progress.toFixed(1)}%`),
    controller.signal
);

// Cancel after 3 seconds
setTimeout(() => controller.abort(), 3000);

try {
    await downloadPromise;
} catch (error) {
    if (error.message === 'Operation cancelled') {
        console.log('Download was cancelled');
    }
}
```

**Interface:**
```typescript
download(
    progressCallback?: (progress: number) => void,
    signal?: AbortSignal
): Promise<void>;
```

**Behavior:**
- When `signal` is aborted, the native callback returns `1` (cancel).
- The returned `Promise` rejects with `Error('Operation cancelled')`.
- `signal` can be used without `progressCallback`.

### Rust — `Arc<AtomicBool>`

Rust uses a shared atomic boolean flag for cancellation, avoiding the need for
an additional crate dependency.

```rust
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

let cancel_flag = Arc::new(AtomicBool::new(false));
let flag_clone = Arc::clone(&cancel_flag);

// Spawn cancellation task
tokio::spawn(async move {
    tokio::time::sleep(Duration::from_secs(3)).await;
    flag_clone.store(true, Ordering::Relaxed);
});

// Start download with cancellation
match model.download_cancellable(
    Some(|progress: &str| println!("  {progress}")),
    cancel_flag,
).await {
    Ok(()) => println!("Download completed"),
    Err(e) => println!("Download cancelled: {e}"),
}
```

**Interface:**
```rust
pub async fn download_cancellable<F>(
    &self,
    progress: Option<F>,
    cancel_flag: Arc<AtomicBool>,
) -> Result<()>
where
    F: FnMut(&str) + Send + 'static;
```

**Behavior:**
- When `cancel_flag` is set to `true`, the trampoline returns `1`.
- Returns `Err(FoundryLocalError::CommandExecution { reason: "Operation cancelled" })`.
- The original `download()` method is unchanged for backward compatibility.

## Execution Provider Downloads

Cancellation is also supported for EP downloads via the
`download_and_register_eps` method. The same pattern applies — pass the
cancellation token/signal/event/flag through to the interop layer.

| Language | Parameter |
|----------|-----------|
| C#       | `CancellationToken? ct` |
| Python   | `cancel_event: Optional[threading.Event]` |
| JS/TS    | `signal?: AbortSignal` (via `executeCommandStreaming`) |
| Rust     | `cancel_flag: Arc<AtomicBool>` |

## Design Decisions

### Why cooperative cancellation?

The native core runs the download on a single thread. The callback is the only
safe synchronisation point — interrupting mid-transfer could leave partially
written files. Cooperative cancellation through the callback return value is
the mechanism the native core was designed for.

### Why different primitives per language?

Each language has an established cancellation idiom:

| Language | Idiom | Rationale |
|----------|-------|-----------|
| C# | `CancellationToken` | Standard .NET pattern for async cancellation |
| Python | `threading.Event` | Lightweight, thread-safe signal; no external deps |
| JS/TS | `AbortSignal` | Standard Web/Node.js pattern (used by `fetch`, etc.) |
| Rust | `Arc<AtomicBool>` | Zero-cost, no extra crate; works across threads |

Using language-native patterns ensures the cancellation API feels natural to
developers in each ecosystem.

### Backward compatibility

All changes are backward-compatible:

- **Python:** `cancel_event` defaults to `None`.
- **JS/TS:** `signal` is optional; existing code works unchanged.
- **Rust:** `download()` is unchanged; `download_cancellable()` is a new method.
- **C#:** Already supported; no changes needed.

## Examples

Complete runnable examples are provided for each SDK:

| Language | Path |
|----------|------|
| Python   | `sdk/python/examples/cancellable_download.py` |
| JS/TS    | `sdk/js/examples/cancellable-download.ts` |
| Rust     | `sdk/rust/examples/cancellable_download.rs` |
| C#       | `samples/cs/model-management-example/Program.cs` (existing, already uses `CancellationToken`) |
