# C# SDK Code Review Report — `sdk_v2/cs`

**Date:** 2025-05-12
**Scope:** Full review of all source, items, OpenAI layer, tests, and samples (~50 files)
**Reviewers:** 3 parallel Copilot agents (interop/core, items/OpenAI, tests/samples)

---

## Summary

| Severity | Count | Resolved | Remaining |
|----------|-------|----------|-----------|
| Critical | 3     | 0        | 3         |
| High     | 9     | 0        | 9         |
| Medium   | 23    | 0        | 23        |
| Low      | 16    | 0        | 16        |
| **Total**| **51**| **0**    | **51**    |

### Positive Observations

- Vtable struct ordering verified against C header — all match ✅
- Calling conventions correct (`StdCall` for vtable, `Cdecl` for callbacks) ✅
- All data struct layouts match C header field order, types, and alignment ✅
- String marshalling correct throughout (`LPUTF8Str`, `StringToCoTaskMemUTF8`) ✅
- PinContext / deleter pattern is well-designed — static delegates prevent GC collection ✅
- Native handle ownership model is architecturally sound ✅
- Test coverage is comprehensive — chat, streaming, tool calling, audio, embeddings, vision, lifecycle ✅

---

## CRITICAL — Must fix

### ~~C1: Callback delegate GC collection during Download~~ — FALSE POSITIVE
**File:** `src/Detail/FoundryLocalApi.cs:585`
**Status:** ~~Dismissed~~ — `Download` is a synchronous P/Invoke; the `nativeCallback` local is rooted on the CLR stack frame for the entire blocking call. Native code only invokes it during this call. No GC collection risk.

---

### C2: Native item handle leak on Request.AddItem failure ❌
**File:** `src/Request.cs:25`
**Status:** Open

`ReleaseOwnership()` transfers ownership from the managed `Item` *before* the native `AddItem` call. If the native call fails, the handle is orphaned — neither managed nor native code owns it. Fix: call `ReleaseOwnership()` only after confirming the native call succeeded, or wrap in try/catch that re-acquires ownership on failure.

---

### C3: `return` instead of `break` — assertions never execute ❌
**File:** `test/FoundryLocal.Tests/ChatCompletionsTests.cs:377`
**Status:** Open

In `DirectTool_Streaming_Succeeds`, the `return` statement inside the streaming loop exits the entire test method before any assertions run. The test always passes regardless of correctness. Should be `break`.

---

### C4: `isFirstChunk` not reset before second streaming pass ❌
**File:** `test/FoundryLocal.Tests/ChatCompletionsTests.cs:369`
**Status:** Open

The `isFirstChunk` flag is set `true` at the top of the test but never reset before the second streaming iteration. Currently masked by C3 (the second pass is never reached). Once C3 is fixed, this will cause the second-pass assertions to fail. Reset `isFirstChunk = true` before the second loop.

---

## HIGH — Should fix

### H1: Dispose during active streaming = use-after-free ❌
**File:** `src/Session.cs:212`
**Status:** Open

`ProcessStreamingRequestAsync` runs on a background thread via `Task.Run`. If the caller disposes the `Session` while streaming is active, `Dispose()` releases the native session handle while the background thread is still using it. This is a use-after-free in native code. Fix: track the streaming task and await it in `Dispose`, or use a ref-counting / cancellation gate.

---

### H2: GCHandle leak in DownloadAndRegisterEps ❌
**File:** `src/Detail/FoundryLocalApi.cs:307`
**Status:** Open

`ptrHandle = GCHandle.Alloc(...)` is freed outside a `try/finally`. If the native call between allocation and free throws, the `GCHandle` leaks permanently. Wrap in `try/finally { ptrHandle.Free(); }`.

---

### H3: Wrong exception-handling overload for async methods ❌
**Files:** `src/OpenAI/ChatClient.cs:85`, `AudioClient.cs`, `EmbeddingClient.cs` (6 call sites)
**Status:** Open

Async methods are wrapped in the synchronous `CallWithExceptionHandling` overload. Exceptions thrown after the first `await` inside the lambda bypass the catch block because the sync overload returns before the async work completes. Need an `async Task<T> CallWithExceptionHandlingAsync(...)` overload that awaits the lambda.

---

### H4: Unobserved task + handle leak on abandoned enumeration ❌
**File:** `src/Session.cs:182`
**Status:** Open

If a caller breaks out of a streaming `await foreach` early, the background `Task.Run` producing items is never awaited. The unobserved task exception (if any) crashes the process or is swallowed depending on `TaskScheduler.UnobservedTaskException` settings. Native handles held by the task also leak.

---

### H5: CancellationToken not passed to Task.Run ❌
**Files:** `src/OpenAI/ChatClient.cs:125`, `AudioClient.cs:111`
**Status:** Open

Non-streaming `CompleteChatAsync` and `TranscribeAudioAsync` accept a `CancellationToken` parameter but never pass it to `Task.Run(...)`. The native blocking call is uncancellable. Even passing the token only prevents the task from *starting* if already cancelled — but that's strictly better than ignoring it entirely.

---

### H6: Race condition on shared Settings.Stream ❌
**Files:** `src/OpenAI/ChatClient.cs:120,150`
**Status:** Open

`Settings` is a public mutable object on the client. The non-streaming path sets `Settings.Stream = false` and the streaming path sets `Settings.Stream = true`. Concurrent callers overwrite each other's flag, causing requests to be sent with the wrong streaming mode. Fix: pass `stream` as a local parameter or clone `Settings` before mutation.

---

### H7: Stale CancellationToken captured by live audio callback ❌
**File:** `src/OpenAI/LiveAudioTranscriptionClient.cs:136`
**Status:** Open

The streaming callback closure captures the `CancellationToken` from `StartAsync`. But `StartAsync` completes immediately, and the caller typically uses a different token for `GetStream()` / `StopAsync()`. If `StartAsync` is called with `default` (common), the callback can *never* return 1 to signal cancellation. Fix: store a `CancellationTokenSource` on the session that `StopAsync` can trigger.

---

### H8: Silent test passes when model unavailable ❌
**Files:** Multiple test files
**Status:** Open

Tests that check `if (model == null) return` report as passed even when the model wasn't available and no logic was tested. Should throw `SkipTestException` (TUnit's skip mechanism) so CI dashboards accurately reflect what was actually tested.

---

### H9: Sync-over-async in assembly hook ❌
**File:** `test/FoundryLocal.Tests/Utils.cs:103`
**Status:** Open

`GetAwaiter().GetResult()` in a test assembly initialization hook. If the test host has a `SynchronizationContext`, this deadlocks. Use `Task.Run(() => ...).GetAwaiter().GetResult()` or restructure as an async hook if TUnit supports it.

---

## MEDIUM (23 findings)

| #   | File | Issue | Status |
|-----|------|-------|--------|
| M1  | `FoundryLocalApi.cs:34` | `EnsureInitialized()` — non-volatile `_initialized` flag, no thread safety | ❌ |
| M2  | `FoundryLocalManager.cs:369` | Potential deadlock in `Dispose()` on `asyncLock` | ❌ |
| M3  | `FoundryLocalManager.cs:400` | Race on static `instance` field (nulled outside lock) | ❌ |
| M4  | `FoundryLocalManager.cs:363` | `Dispose()` not thread-safe — double-dispose can double-release | ❌ |
| M5  | `LiveAudioTranscriptionClient.cs` | No thread-safety on `_state` transitions | ❌ |
| M6  | `FoundryLocalManager.cs:184` | EP callback delegate GC risk (partially mitigated) | ❌ |
| M7  | Multiple files | Duplicate `Request`/`Response`/`Item` hierarchies (internal vs public) | ❌ |
| M8  | `Response.cs:42` | Non-owning items from enumerator — no lifetime docs | ❌ |
| M9  | `Item.cs:42` | `FromNative` never creates `JsonItem` for OpenAI JSON payloads | ❌ |
| M10 | `Item.cs:70` | `OnDisposing()` runs after native handle released | ❌ |
| M11 | `MessageItem.cs:55` | Constructor leaks native handles if `SetNativeMessage` throws | ❌ |
| M12 | `AudioItem/ImageItem/BytesItem` | Constructors leak handles if Set* throws | ❌ |
| M13 | `AudioItem.cs:123` | `ReadOnlyMemory` pointer passed as `MutableData` (C API constraint) | ❌ |
| M14 | `ChatClient.cs:188` | Streaming callback continues after error (should return 1) | ❌ |
| M15 | `ChatClient.cs:137` | Unsafe cast to `TextItem` — `InvalidCastException` if non-text | ❌ |
| M16 | `ChatClient.cs:72` | Non-standard `CancellationToken?` (should be `ct = default`) | ❌ |
| M17 | `ToolCallingExtensions.cs:14` | `ResponseFormatExtended` polymorphic serialization risk with AOT | ❌ |
| M18 | `ChatClient.cs:37` | Eager `FoundryLocalManager.Instance` in field initializer — NPE | ❌ |
| M19 | `AudioClientTests.cs:86` | Inconsistent audio file path resolution | ❌ |
| M20 | `Utils.cs:19` | Fragile dependency on `TestHost.Program` type | ❌ |
| M21 | `OperatingSystemConverter.cs` | No namespace; appears to be unused dead code | ❌ |
| M22 | `.csproj:52-53` | `Moq` and `MockHttp` referenced but unused | ❌ |
| M23 | Solution file | `samples/TestApp` not in solution — won't build in CI | ❌ |

---

## LOW (16 findings)

| #   | File | Issue | Status |
|-----|------|-------|--------|
| L1  | `AsyncLock.cs:48` | `Releaser.Dispose()` not idempotent — double-dispose corrupts semaphore | ❌ |
| L2  | `AsyncLock.cs:36` | Faulted `WaitAsync` task returns releaser without lock | ❌ |
| L3  | Throughout | `CancellationToken?` forces unnecessary boxing/null-checks | ❌ |
| L4  | `Item.cs:66` | `Dispose` not thread-safe (double-free via `_disposed` flag) | ❌ |
| L5  | `ItemQueue.cs:46` | `Count` truncates `ulong` to `int` without `checked` | ❌ |
| L6  | `ItemQueue.cs:52` | Missing `GC.SuppressFinalize` | ❌ |
| L7  | `PinContext.cs:71` | `Dispose` not guarded against double-call | ❌ |
| L8  | `TensorItem.cs:14` | Raw `IntPtr Data` exposed publicly | ❌ |
| L9  | `FoundryModelInfo.cs:15` | `JsonStringEnumConverter<DeviceType>` may not be AOT-compatible | ❌ |
| L10 | `LiveAudioTranscriptionClient.cs:63` | `StartAsync` is synchronous despite name | ❌ |
| L11 | `AudioTranscriptionTypes.cs:35` | Language/Temperature duplicated in metadata (dead code) | ❌ |
| L12 | `AudioTranscriptionTypes.cs:74` | `ToJson` ignores `Metadata` dictionary | ❌ |
| L13 | `EndToEnd.cs:56` | `progressValues[0]` without empty check | ❌ |
| L14 | `SkipInCIAttribute.cs:13` | Doesn't check generic `CI` environment variable | ❌ |
| L15 | `EndToEnd.cs:71` | Web service started but never stopped | ❌ |
| L16 | `samples/Program.cs` | No CancellationToken or error handling demonstrated | ❌ |

---

## Change Log

| Date | Change |
|------|--------|
| 2025-05-12 | Initial review — 52 findings (4C, 9H, 23M, 16L) |
| 2025-05-12 | C1 dismissed as false positive — sync P/Invoke roots delegate on stack. Now 51 (3C, 9H, 23M, 16L) |
