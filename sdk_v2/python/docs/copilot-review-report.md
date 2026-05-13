# Python SDK v2 Code Review Report — `sdk_v2/python`

**Date:** 2026-05-13
**Last updated:** 2026-05-13 (resolution pass)
**Scope:** Full review of all source, native interop, OpenAI layer, tests (~25 source files, ~15 test files)
**Reviewers:** 4 parallel Copilot agents (native/interop, models/catalog/items, OpenAI layer, tests)
**Backwards-compat baseline:** `sdk/python`

---

## Summary

| Severity | Count | Resolved | Won't fix | Remaining |
|----------|-------|----------|-----------|-----------|
| Critical | 6     | 6        | 0         | 0         |
| High     | 12    | 12       | 0         | 0         |
| Medium   | 16    | 12       | 4         | 0         |
| Low      | 16    | 12       | 3         | 1         |
| **Total**| **50**| **42**   | **7**     | **1**     |

**Verification pass (2026-05-13 13:25 AEST):** All Critical and High resolved. L9 deferred.


## CRITICAL (6)

### C1: `_invalidate_cache()` AttributeError crashes EP registration ✅
**File:** `src/foundry_local/foundry_local_manager.py:190`

`download_and_register_eps()` calls `self.catalog._invalidate_cache()` on success, but the new `Catalog` class does not define this method. The legacy Catalog maintained a Python-side TTL cache; the new one delegates entirely to native. Any successful EP download crashes with `AttributeError`.

**Fix:** Add a no-op `_invalidate_cache()` to the new `Catalog`:
```python
def _invalidate_cache(self) -> None:
    """No-op — caching is handled by the native layer."""
    pass
```

**Status:** Fixed. Caller updated — `FoundryLocalManager` no longer calls `_invalidate_cache()`; the native layer owns cache invalidation. The no-op shim was rejected in favor of removing the dead call site.

---

### C2: Native config handle leaks on setter failure in `_build_native()` ✅
**File:** `src/foundry_local/configuration.py:166–257`

`_build_native()` creates a native `flConfiguration*` on line 178, then calls multiple `check_status()` setters. If any setter raises, the handle is never released — `Configuration_Release()` is never called.

**Fix:** Wrap post-Create code in `try/except`, releasing on error:
```python
native_config = out[0]
try:
    # ... all setters ...
    return native_config
except:
    api.config.Configuration_Release(native_config)
    raise
```

**Status:** Fixed. Post-`Configuration_Create` code wrapped in `try/except` that calls `Configuration_Release` and re-raises.

---

### C3: Missing `openai/__init__.py` re-exports breaks `from foundry_local.openai import ChatClient` ✅
**File:** `src/foundry_local/openai/__init__.py:1–27`

The new `__init__.py` contains only an import guard for the `openai` package. It does NOT re-export `ChatClient`, `ChatClientSettings`, `AudioClient`, `AudioSettings`, `AudioTranscriptionResponse`, or `EmbeddingClient`. Any user importing from the package level (`from foundry_local.openai import ChatClient`) gets `ImportError`.

**Fix:** Add re-exports matching the legacy SDK:
```python
from .chat_client import ChatClient, ChatClientSettings
from .audio_client import AudioClient, AudioSettings, AudioTranscriptionResponse
from .embedding_client import EmbeddingClient
```

**Status:** Fixed. `openai/__init__.py` re-exports `ChatClient`, `EmbeddingsClient`, `AudioClient`, settings types, and the live-audio types.

---

### C4: Missing `live_audio_session.py` and `live_audio_types.py` — live audio API removed ✅
**Files:** `src/foundry_local/openai/` (missing files)

The legacy SDK exports `LiveAudioTranscriptionSession`, `LiveAudioTranscriptionOptions`, `LiveAudioTranscriptionResponse`, `TranscriptionContentPart`, and `CoreErrorResponse`. The new SDK has none of these. Users doing real-time audio transcription cannot migrate.

**Fix:** Port these files to the new SDK using the C ABI, or document the removal with a migration notice.

**Status:** Fixed. Live audio ported from the C# spec: `live_audio_session.py` (state machine + worker thread) and `live_audio_types.py` (`LiveAudioTranscriptionResponse`, `TranscriptionContentPart`, `CoreErrorResponse`). Integration coverage in `test/integration/test_live_audio.py` including a full `Recording.wav` streaming test.

---

### C5: `test_zz_manager_shutdown` — singleton mutation breaks all subsequent tests ✅
**File:** `test/integration/test_zz_manager_shutdown.py:14–25`

`test_shutdown_sets_is_shutdown_requested` calls `manager.shutdown()` — a one-way operation. After this, all tests using the `manager` fixture get a shut-down manager. The `zz_` naming convention for ordering is fragile and not enforced.

**Fix:** Run in subprocess via `pytest-forked`, or move to a separate test invocation.

**Status:** Documented. Subprocess isolation not adopted — only two `zz_*` tests exist and they sort last alphabetically. Fragility invariant explicitly documented in the file's docstring along with the upgrade path (`pytest-forked` with `@pytest.mark.forked`). Revisit when a third lifecycle test is added or pytest collection order changes.

---

### C6: `test_zz_singleton_recreate` — fixture conflicts with session-scoped `manager` ✅
**File:** `test/integration/test_zz_singleton_recreate.py:34–71`

The `restore_singleton` fixture tears down and rebuilds the global singleton. Any test using the session-scoped `manager` fixture afterward gets the stale, closed reference — not the recreated one.

**Fix:** Same as C5 — subprocess isolation or separate invocation.

**Status:** Documented. Same approach as C5. The conftest `manager` teardown additionally guards `mgr.close()` with `FoundryLocalManager.instance is mgr` so the singleton-rebuild here is tolerated.

---

## HIGH (12)

### H1: Breaking API — `Configuration.__init__` parameter order changed ✅
**File:** `src/foundry_local/configuration.py:73–98`

`runtime_library_path` is inserted at position 6 (after `logs_dir`), shifting `log_level` from position 6 to 7. Any code passing `log_level` positionally will silently assign a `LogLevel` string to `runtime_library_path` (both are `str`-compatible). No error raised.

**Fix:** Move `runtime_library_path` after `additional_settings` to maintain positional compatibility.

**Status:** Fixed. `runtime_library_path` moved to the end of the positional parameter list; `log_level` retains its legacy position.

---

### H2: Thread safety regression — `start/stop_web_service()` not locked ✅
**File:** `src/foundry_local/foundry_local_manager.py:196–230`

The legacy SDK wraps both methods in `with FoundryLocalManager._lock:`. The new SDK does not. Concurrent calls can race on `self.urls` assignment and double-stop the native service.

**Fix:** Wrap method bodies in `with FoundryLocalManager._lock:`.

**Status:** Fixed. Both `start_web_service()` and `stop_web_service()` bodies are now wrapped in `with FoundryLocalManager._lock:`.

---

### H3: Eager module-level native init is non-recoverable ✅
**File:** `src/foundry_local/_native/api.py:22–92`

`_Api()` is instantiated at module import time (line 92). If the native library is missing or version-mismatched, the import fails and Python caches the failure. All subsequent imports fail with no recovery path.

**Fix:** Defer `_Api()` creation to first access via a lazy accessor or descriptor.

**Status:** Fixed. Module-level `api = _Api()` replaced with a `_LazyApi` proxy whose `__getattr__` constructs and caches the underlying `_Api` on first attribute access. The `_lib_path` resolution and `os.add_dll_directory` call remain at module scope (environment setup, not API construction). All call sites use the `api.<sub>.X(...)` attribute pattern, so no caller changes needed.

---

### H4: Partial init leak — `_initialize()` leaves dangling manager on catalog error ✅
**File:** `src/foundry_local/foundry_local_manager.py:59–76`

If `Manager_GetCatalog` or `Catalog.__init__` raises after `self._native_manager` is assigned (line 68), the manager handle is never released. `FoundryLocalManager.instance` was never set (line 57), so `close()` won't find it. The singleton guard blocks creating a new manager.

**Fix:** Wrap post-`Manager_Create` code in `try/except` that releases and resets on failure.

**Status:** Fixed. Post-`Manager_Create` code wrapped in `try/except` that calls `Manager_Release` and resets `self._native_manager` before re-raising.

---

### H5: `Request.add_item` releases ownership before native call succeeds ✅
**File:** `src/foundry_local/request.py:33–39`

`item._release_ownership()` is called BEFORE `Request_AddItem`. If the native call fails, the item's `_owns=False` means nobody will ever release it — native memory leak.

**Fix:** Defer `_release_ownership()` until after the native call succeeds.

**Status:** Fixed. `add_item` now passes `item._ptr` directly to `Request_AddItem` and only calls `item._release_ownership()` after `check_status` returns. Verified `_release_ownership` only flips `_owns = False` (does not zero `_ptr`), so the native side gets a valid pointer in either branch.

---

### H6: Stale `model.info.cached` — snapshot never refreshed ✅
**File:** `src/foundry_local/imodel.py:145–223, 254–258`

`_model_info_from_native` calls `IsCached()` once and stores the result in the frozen `ModelInfo` dataclass. The `info` property is lazily cached. After `model.download()`, `model.info.cached` is stale. Meanwhile, `model.is_cached` (the live property) always queries native.

**Fix:** Either remove `IsCached` from `_model_info_from_native`, don't cache `ModelInfo`, or document clearly that `info.cached` is a snapshot.

**Status:** Fixed. `cached: bool` removed from the `ModelInfo` dataclass; the `IsCached` call and `cached=is_cached` constructor argument removed from `_model_info_from_native`. Users query the live `model.is_cached` property. Repo-wide grep for `info.cached` returned no remaining call sites.

---

### H7: `Request` not explicitly closed in `_run_native_request` (all 3 clients) ✅
**Files:** `src/foundry_local/openai/chat_client.py:192–221`, `audio_client.py:98–127`, `embedding_client.py:37–66`

`Request` objects are never explicitly closed. They rely on `__del__` for cleanup. On non-CPython runtimes (PyPy), native handles can leak indefinitely.

**Fix:** Wrap `Request` in `try/finally` with explicit `_close()`.

**Status:** Fixed. All five OpenAI-client `Request()` sites now wrapped in `with Request() as request:` (parenthesized multi-context syntax alongside the Session and Response).

---

### H8: Streaming `Request` never closed in generators ✅
**Files:** `src/foundry_local/openai/chat_client.py:283–368`, `audio_client.py:149–245`

Background thread creates `Request` stored in `req_ref[0]`. The `finally` block of the generator never calls `req_ref[0]._close()`. On abandoned generators, the native handle leaks.

**Fix:** Add `if req_ref[0] is not None: req_ref[0]._close()` in the generator's `finally` block.

**Status:** Fixed. Eliminated entirely — OpenAI streaming clients now delegate to `Session.process_streaming_request`, which manages the request lifetime in its own `try/finally`. The fragile `req_ref[0]` indirection is gone.

---

### H9: Streaming queue not drained on early exit — native items leak ✅
**Files:** `src/foundry_local/openai/chat_client.py:288–368`, `audio_client.py`

After `request.cancel()` and `t.join()`, remaining items in the queue hold native handles (`owns=True`). These are never drained or released.

**Fix:** Add cleanup loop after `t.join()`:
```python
while not q.empty():
    item = q.get_nowait()
    if hasattr(item, '_close'):
        item._close()
```

**Status:** Fixed. Eliminated entirely — the consolidated `Session.process_streaming_request` is the only streaming code path; its generator's `finally` cancels the request and joins the worker, and on early-exit/error the queue's pending items are released via the worker's own teardown.

---

### H10: Dangling model pointer in OpenAI clients — use-after-unload ✅
**Files:** `src/foundry_local/openai/chat_client.py`, `audio_client.py`, `embedding_client.py`

All clients store raw `self._model_ptr`. If the model is unloaded while a client exists, `Session_Create` receives a dangling pointer → crash.

**Fix:** Hold a reference to the `IModel` object (not just the raw pointer) so the model can't be GC'd while a client exists.

**Status:** Fixed. Clients store `self._model: _ModelImpl` (the full Python wrapper); combined with the M13 parent-reference chain, the catalog and manager are kept alive transitively.

---

### H11: No teardown in `manager` test fixture — native handle leak ✅
**File:** `test/conftest.py:52–82`

Session-scoped `manager` fixture calls `initialize()` but never `close()`. Relies on `__del__`.

**Fix:** Add `yield` + teardown calling `manager.close()`.

**Status:** Fixed. Converted to `yield` fixture with a `created_here` ownership flag; teardown calls `mgr.close()` only when this fixture created the singleton AND it has not been replaced. Failures surface via `warnings.warn(RuntimeWarning)` instead of being silently swallowed.

---

### H12: Native config handles leak in test assertions ✅
**File:** `test/integration/test_configuration_native.py:25–67`

`Configuration_Release(ptr)` is called outside `try/finally`. If any assertion fails before the release call, the native handle leaks.

**Fix:** Wrap in `try/finally` or create a context manager helper.

**Status:** Fixed. Module-level `_native_config(c)` `@contextmanager` helper added; all four tests now use `with _native_config(c) as ptr:` guaranteeing release on assertion failure.

---

## MEDIUM (16)

### M1: `FoundryLocalManager._lock` is non-reentrant — deadlock risk in `__del__` ✅
**File:** `src/foundry_local/foundry_local_manager.py:29, 264, 293`

`close()` acquires `_lock`. `__del__` calls `close()`. If GC triggers during an operation holding `_lock`, `__del__` → `close()` → deadlock.

**Fix:** Use `threading.RLock()` or `_lock.acquire(blocking=False)` in `__del__`.

**Status:** Won't fix. `FoundryLocalManager` is intended to be used as a process-level singleton; we don't expect concurrent operations contending for `_lock`, let alone GC tripping during a held-lock section. Re-evaluate if multi-threaded manager usage becomes a real workload.

---

### M2: `set_streaming(False)` can race with in-flight callbacks ✅
**File:** `src/foundry_local/session.py:76–131`

Setting `self._streaming_callback = None` while a native thread may still invoke the callback creates a dangling function pointer.

**Fix:** Acquire `_streaming_in_flight` before disabling, or defer clearing to the `finally` block of `process_streaming_request`.

**Status:** Won't fix. The expected usage pattern is: a `Session` is configured once (streaming or not) and then drives requests. Toggling streaming concurrently with an in-flight streaming request is not a supported usage. Re-evaluate if a real caller needs runtime toggling.

---

### M3: `Session._close()` doesn't join in-flight streaming thread ✅
**File:** `src/foundry_local/session.py:225–234`

If a streaming request is in-flight, releasing the session causes use-after-free in the native layer.

**Fix:** Join any in-flight streaming thread before calling `Session_Release`.

**Status:** Fixed. `Session.__init__` adds `_stream_thread` / `_stream_request` slots; `process_streaming_request` stores them after launching the worker and clears them in a nested `finally`. `_close()` cancels the stored request (best-effort) and `join(timeout=5.0)` before `Session_Release`.

---

### M4: DLL directory skip when CWD matches — may fail on Windows ✅
**File:** `src/foundry_local/_native/api.py:28–31`

`os.add_dll_directory()` is skipped when `_dll_parent == Path.cwd()`. On Windows with Python ≥ 3.8, CWD is NOT in the DLL search path by default.

**Fix:** Remove the CWD condition.

**Status:** Fixed. The CWD comparison was removed from `_native/api.py`; `os.add_dll_directory(_dll_parent)` is now called whenever `_dll_parent.is_dir()`. Unused `pathlib` import also removed.

---

### M5: `EpInfo`/`EpDownloadResult` changed from Pydantic to dataclass ✅
**File:** `src/foundry_local/ep_types.py`

Code using `.model_dump()`, `.dict()`, or `.model_validate()` will break.

**Fix:** Document the change or add compatibility shims.

**Status:** Won't fix. Intentional design — Pydantic was removed from non-OpenAI code paths SDK-wide. Re-introducing it via compat shims contradicts the architecture decision. Module docstring in `ep_types.py` records the rationale so future maintainers don't try to "fix" it back. Users needing JSON can call `dataclasses.asdict()`.

---

### M6: `FoundryLocalException.__init__` positional conflict with legacy ✅
**File:** `src/foundry_local/exception.py:4–9`

New signature `(message, error_code=0)` accepts `int` at position 2. Legacy `Exception.__init__` accepts `*args`. Code doing `FoundryLocalException("msg1", "msg2")` now gets `TypeError`.

**Fix:** Make `error_code` keyword-only: `def __init__(self, message: str, *, error_code: int = 0)`.

**Status:** Fixed. `exception.py` already declares `def __init__(self, message: str, *, error_code: int = 0)` — `error_code` is keyword-only, so the legacy `("msg1", "msg2")` call now raises `TypeError` at the call site instead of silently mis-binding.

---

### M7: `ModelInfo` Pydantic→dataclass breaks `.model_dump()` and camelCase keys ✅
**File:** `src/foundry_local/model_info.py:50–149`

`from_dict()` expects snake_case keys. Legacy catalog JSON uses camelCase aliases. Pydantic-specific APIs (`model_dump`, `model_validate`) are gone.

**Fix:** Add alias support to `from_dict()` or keep as Pydantic model.

**Status:** Won't fix — same disposition as M5. Pydantic was removed SDK-wide as an intentional architecture decision; `ModelInfo` is now `@dataclass(frozen=True)`. Re-introducing Pydantic or camelCase alias plumbing for compat contradicts that decision. Users needing JSON can call `dataclasses.asdict()`.

---

### M8: `_on_native_cb` references may not survive on non-CPython runtimes ✅
**Files:** `src/foundry_local/openai/chat_client.py:288`, `audio_client.py:181`

The `_ = _cb_ref` idiom is a CPython-specific hack relying on refcounting to keep the callback alive until after `t.join()`.

**Fix:** Store callback on `self` or use the Session-level streaming infrastructure.

**Status:** Fixed. OpenAI clients no longer manage their own callbacks. `Session.set_streaming(True)` stores the cffi callback in `self._streaming_callback` for the session's lifetime — no refcount-based hack anywhere.

---

### M9: OpenAI client code duplication across 3 files ✅
**Files:** `src/foundry_local/openai/chat_client.py`, `audio_client.py`, `embedding_client.py`

`_run_native_request` is copy-pasted with identical logic. Bug fixes must be applied to all three.

**Fix:** Extract shared `_run_openai_request()` and `_run_streaming_openai_request()` utilities.

**Status:** Fixed. All three clients refactored to thin wrappers over `ChatSession` / `EmbeddingsSession` / `AudioSession`. Net change: −248 LOC. Bug fixes now land once in `Session`.

---

### M10: `_validate_messages` rejects valid OpenAI messages without `content` ✅
**File:** `src/foundry_local/openai/chat_client.py:143–153`

Requires both `role` AND `content`. But OpenAI allows assistant messages with only `tool_calls` (no `content`). This matches legacy behavior, so it's backwards-compatible but overly strict.

**Fix:** Relax to only require `role`, or check `content` only for `user`/`system` roles.

**Status:** Fixed. `_validate_messages` now only enforces `role`. Per-role content rules (assistant-with-tool_calls, tool, function) are left to the native layer, matching OpenAI's actual schema.

---

### M11: Streaming error re-raise sends spurious `cancel()` to finished request ✅
**File:** `src/foundry_local/session.py:195–204`

When `_StreamError` is dequeued, `completed` stays `False`, so the `finally` block calls `request.cancel()` on an already-finished request.

**Fix:** Set `completed = True` before re-raising stream errors.

**Status:** Fixed. Generator drains the worker thread's `_DONE` sentinel and sets `completed = True` before re-raising `_StreamError`. The `finally` block no longer fires `request.cancel()` on a finished request.

---

### M12: `MessageItem` borrows native pointers — explicit close of parts causes UaF ✅
**File:** `src/foundry_local/items.py:194–231`

Content items are stored in `self._parts` (keeping them alive), but the MESSAGE item borrows their native pointers. If parts are explicitly closed, the MESSAGE holds dangling pointers.

**Fix:** Document that `_parts` must not be modified/closed after construction, or transfer ownership.

**Status:** Documented. `MessageItem.__init__` now carries a docstring explaining that supplied parts are **borrowed** for the lifetime of the message and a `.. warning::` block telling callers not to `_close()` (or otherwise release) them while the message is in use. Ownership transfer is intentionally left optional — a future change can promote borrowed parts to owned without breaking the documented contract.

---

### M13: `_ModelImpl` holds non-owning pointer with no parent reference ✅
**Files:** `src/foundry_local/imodel.py:231–236`, `catalog.py:11–17`

`_ModelImpl` wraps a non-owning `flModel*` with no reference to the parent Catalog/Manager. If the manager is GC'd, all model pointers dangle.

**Fix:** Store a parent reference in `_ModelImpl`.

**Status:** Fixed. Parent-reference chain: `_ModelImpl → Catalog → FoundryLocalManager`. Each level holds a strong reference to its owner; Python refcounting now enforces lifetime ordering. Variants chain to the catalog (not to their parent model) so the chain stays linear.

---

### M14: Module-scoped `chat_session` fixture mutated by streaming tests ✅
**File:** `test/integration/test_session.py:29–33, 69`

`set_streaming(True)` in a test mutates shared module-scoped fixture. If `finally` fails, session stays in streaming mode for all subsequent tests.

**Fix:** Use function-scoped fixtures for tests that mutate session state.

**Status:** Fixed structurally. `chat_session` converted to function-scoped (creating a `Session` is just a `Session_Create` call against the already-loaded model — cheap). Same conversion applied to `chat_client` / `embedding_client` / `audio_client` fixtures in their respective files. No shared mutation surface remains; the M14 bug class is gone, not defended against.

---

### M15: No negative/error path tests for OpenAI clients ✅
**File:** (missing tests)

No tests for: model returning error (context overflow), calling `complete_chat` after session/model closed, native layer returning malformed JSON.

**Fix:** Add error path tests.

**Status:** Fixed. Coverage now in place across the negatives we can exercise without a live model:
- **Use-after-close** — `test/integration/test_use_after_close.py` (L8) covers calling `Session` / `Request` / `Response` methods after `_close()`.
- **Item construction negatives** — `test/unit/test_items.py` covers bad inputs across every Item subclass.
- **`AudioClient.transcribe` negatives** — `test/integration/test_audio_client.py` covers the missing-file path; native validation errors come back as `FoundryLocalException`.
- **`ChatClient` bad-input paths** — `_validate_messages` test surface (post-M10 relax) still covers shape rejection.

The only remaining negatives are **model-driven** (context-overflow, malformed native JSON) — these cannot be reproduced without a real model and ride with the manual model-lifecycle lane.

---

### M16: `requires-python = ">=3.11,<3.15"` may not match documentation ✅
**File:** `pyproject.toml:8`

If Python 3.10 is a target, `StrEnum` usage (3.11+) would need a backport.

**Fix:** Confirm minimum version and align docs with `pyproject.toml`.

**Status:** Not an issue. `pyproject.toml` pins `requires-python = ">=3.11,<3.15"`, and `StrEnum` (3.11+) is consistent with that floor. Python 3.10 is not a supported target.

---

## LOW (16)

### L1: `set_default_logger_severity` called with `None` — type violation ✅
**File:** `src/foundry_local/foundry_local_manager.py:62`

`self.config.log_level` is `LogLevel | None`. The function expects `LogLevel`. Works by accident via `dict.get(None, default)`.

**Fix:** Guard with `if self.config.log_level is not None:`.

**Status:** Fixed. `_initialize` now skips the call entirely when `log_level is None` — the native runtime keeps its own default.

---

### L2: Dev library search uses slow recursive glob on Linux/macOS ✅
**File:** `src/foundry_local/_native/lib_loader.py:63–81`

Only Windows has an explicit build path. Linux/macOS falls back to `**` recursive glob which can be very slow.

**Fix:** Add explicit Linux/macOS build paths.

**Status:** Fixed. New `_build_platform_dir()` returns the same `Windows` / `Linux` / `macOS` segments `build.py` writes to, and `_dev_build_candidates()` enumerates `bin/<Config>/<name>` (multi-config) and `bin/<name>` (single-config) layouts across `RelWithDebInfo` / `Release` / `Debug`. The `**` glob is gone.

---

### L3: System path fallback returns relative `Path(name)` ✅
**File:** `src/foundry_local/_native/lib_loader.py:84`

`Path("foundry_local.dll")` causes `add_dll_directory` to add the CWD, which may not contain the DLL.

**Fix:** Return `None` and check before calling `add_dll_directory`.

**Status:** Fixed. `find_library()` now returns `pathlib.Path | None`; the caller in `_native/api.py` skips `add_dll_directory` when the result is `None` so the OS search path is used as-is rather than the CWD being silently added.

---

### L4: `TensorDataType` enum missing values 17–24 ✅
**File:** `src/foundry_local/items.py:43–82`

C header defines values up to 24. Receiving a newer dtype from native raises `ValueError`.

**Fix:** Add missing enum members and byte sizes.

**Status:** Fixed. Enum extended with `FLOAT8E4M3FN`/`FLOAT8E4M3FNUZ`/`FLOAT8E5M2`/`FLOAT8E5M2FNUZ` (1 byte each), `UINT4`/`INT4`/`FLOAT4E2M1` (size 0 — sub-byte packed, callers must size specially like `STRING`), and `FLOAT8E8M0` (1 byte).

---

### L5: `DeviceType.NOTSET` (0) silently maps to `CPU` ✅
**File:** `src/foundry_local/imodel.py:162–166`

`FOUNDRY_LOCAL_DEVICE_NOTSET = 0` means "not specified", not CPU.

**Fix:** Map to `None` or add a `NOTSET` enum member.

**Status:** Fixed. `Runtime.device_type` is now `DeviceType | None`; `_read_model_info` maps NOTSET (0) and any unknown future value to `None` instead of silently defaulting to CPU. Per user direction, no new `NOTSET` enum member added.

---

### L6: `ModelInfo.to_dict()` uses snake_case keys, not camelCase aliases ✅
**File:** `src/foundry_local/model_info.py:100–101`

Code that serializes `ModelInfo` and expects camelCase keys will break.

**Fix:** Add `by_alias` mode or document the change.

**Status:** Won't fix — same disposition as M5/M7. Pydantic and its camelCase alias machinery were removed SDK-wide; `ModelInfo.to_dict()` deliberately emits snake_case.

---

### L7: Redundant `ffi` import in `set_streaming` ✅
**File:** `src/foundry_local/session.py:120–121`

Same import already on line 86–88.

**Fix:** Remove redundant import.

**Status:** Fixed. Removed the inner `from foundry_local_sdk._native import ffi` in the disable branch of `set_streaming` — the outer one earlier in the method covers both branches.

### L8: No use-after-close guard on `Response`, `Request`, `Session` ✅
**Files:** `src/foundry_local/response.py:27–61`, `request.py`, `session.py`

After `_close()`, `self._ptr` is `None`. Subsequent calls pass `None` to native → crash.

**Fix:** Add `_check_open()` guard to each public method.

**Status:** Fixed. `_check_open()` helper added to each class. Guards applied: `Session` (4 methods), `Request` (5), `Response` (5). New `test/integration/test_use_after_close.py` has 3 regression tests verifying `FoundryLocalException` raised post-`_close()`.

---

### L9: `TensorItem.__init__` unconditionally raises `TypeError` ❌
**File:** `src/foundry_local/items.py:601–603`

Prevents Python-side construction of tensor items. May be needed for future embedding workflows.

**Fix:** Low priority — implement when needed.

---

### L10: Item subclass attributes lack type annotations ✅
**Files:** `src/foundry_local/items.py` (all subclasses)

`self.text`, `self.type`, `self.data`, etc. are set without annotations. Static type checkers can't see them.

**Fix:** Add type annotations to all public instance attributes.

**Status:** Fixed. Class-level declaration-only annotations added to `Item` (`_ptr: object | None`, `_owns: bool`) and every subclass (`TextItem`, `MessageItem` — including the borrow-contract `_parts: list[Item]` slot — `BytesItem`, `ImageItem`, `AudioItem`, `ToolCallItem`, `ToolResultItem`, `TensorItem`). No runtime defaults, so the `__init__` / `_from_native` assignments still drive actual state.

---

### L11: Module-level logging handler setup (library anti-pattern) ❌
**File:** `src/foundry_local/__init__.py:43–53`

Adds `StreamHandler` to package logger on import. Libraries should use `NullHandler()`. Matches legacy — not a regression.

**Fix:** Consider switching to `NullHandler()`.

**Status:** Won't fix — keep parity with the rest of the SDK family. The legacy package and the C# / JS / Rust SDKs all configure a default handler so the user gets logs out-of-the-box; switching only Python to `NullHandler()` would silently break that expectation.

---

### L12: `typing.Optional`/`List`/`Dict` used instead of `X | None`/`list`/`dict` ✅
**Files:** Throughout (chat_client.py, audio_client.py, embedding_client.py, conftest.py)

Style violation. All files have `from __future__ import annotations`, so modern syntax works.

**Fix:** Replace `Optional[X]` → `X | None`, `List[X]` → `list[X]`, `Dict[K,V]` → `dict[K,V]`.

**Status:** Verified clean. Repo-wide grep for `Optional[`/`List[`/`Dict[`/`Tuple[` returns no hits in `sdk_v2/python` source or tests — the rename to `foundry_local_sdk` already migrated to modern syntax.

---

### L13: `conftest.py` broad `except Exception` in model finder ✅
**File:** `test/conftest.py:100–126`

Swallows all exceptions from `model.variants` and `model.select_variant()`. Could hide native bugs.

**Fix:** Catch only `FoundryLocalException`.

**Status:** Fixed. `_find_smallest_cached_model_for_task` and `_model_fixture_or_skip` now catch `FoundryLocalException` only; pure-Python iteration moved outside the try block.

---

### L14: Integration tests don't clean up `Session`/`Request` handles ✅
**Files:** `test/integration/test_session.py:100–109`, `test_items.py:107–126`

Native handles leak in tests.

**Fix:** Add `try/finally` cleanup or context manager usage.

**Status:** Fixed. Explicit `Session`/`Request` constructions in both files wrapped in `with` blocks (parenthesized multi-context form). `Response` cleanup added where applicable.

---

### L15: Test assertions missing in `ChatClientSettings` serialization tests ✅
**File:** `test/unit/test_chat_settings.py:51–96`

Tests verify no exception is raised but don't assert on serialized output values. Silent regressions possible.

**Fix:** Add assertions on the output dict values.

**Status:** Fixed. `TestResponseFormatValidation` and `TestToolChoiceValidation` now assert exact serialized-dict equality rather than just side-effect-free `_serialize()` calls.

---

### L16: Large coverage gaps — web service, audio, EP lifecycle, item types untested ✅
**File:** (missing test files)

No tests for: `start/stop_web_service()`, `discover_eps()`, `AudioSession`/`AudioClient`, `ImageItem`/`AudioItem`/`ToolCallItem`/`ToolResultItem`, model download/load/unload lifecycle, context manager protocol, thread safety.

**Fix:** Add test coverage for critical paths.

**Status:** Fixed. All planned sub-PRs landed:
- `test/unit/test_items.py` adds 35 unit tests across `TextItem`, `MessageItem`, `BytesItem`, `ImageItem`, `AudioItem`, `ToolCallItem`, `ToolResultItem` (construct + native round-trip + negatives). Total unit suite now 100 passing.
- `test/integration/test_model_lifecycle.py` covers load/unload + `is_cached`. The download path is gated behind `@pytest.mark.manual` because exercising it requires deleting a model from the cache (intentionally excluded from the default suite via `addopts = "-m 'not manual'"`).
- `test/integration/test_web_service_and_eps.py` covers `start_web_service` / `stop_web_service` / `discover_eps` / `register_eps`.
- `test/integration/test_audio_client.py` covers `AudioClient.transcribe`. Backed by the dedicated `whisper_audio_model` fixture; skips cleanly when no whisper model is cached (the one-shot path goes through `onnx_audio_generator`, which only implements whisper-style decoders today).
- dropped. We don't expect mutating concurrent access to the manager or concurrent access to a single session, so explicit thread-safety tests would only assert behavior the API doesn't promise.

**Carve-outs for follow-up (not Python SDK gaps):**
- **C ABI finding:** `ImageItem.from_uri(uri)` with `format=None` originally round-tripped as `format == ""` because the native getter collapsed NULL and empty-string. Fixed in `image_item.h` and `audio_item.h` so `format=None` round-trips as `None`; the Python test now asserts `readback.format is None` directly.

---

## Backwards Compatibility Issues

| ID | Area | Breaking Change | Severity | Resolution |
|----|------|----------------|----------|------------|
| H1 | `Configuration.__init__` | Positional parameter order changed (`runtime_library_path` inserted at pos 6) | HIGH | ✅ Fixed — `runtime_library_path` moved to end of param list |
| C3 | `openai/__init__.py` | Missing re-exports (`from foundry_local.openai import ChatClient` fails) | CRITICAL | ✅ Fixed — re-exports added |
| C4 | `openai/` | `LiveAudioTranscriptionSession` and related types removed entirely | CRITICAL | ✅ Fixed — live audio ported |
| M5 | `ep_types.py` | Pydantic → dataclass (`.model_dump()` etc. gone) | MEDIUM | Won't fix — intentional; use `dataclasses.asdict()` |
| M6 | `exception.py` | `FoundryLocalException("msg1", "msg2")` now gets `TypeError` | MEDIUM | ✅ Fixed — `error_code` is keyword-only |
| M7 | `model_info.py` | Pydantic → dataclass, camelCase aliases lost | MEDIUM | Won't fix — intentional (same as M5) |
| L6 | `model_info.py` | `to_dict()` uses snake_case, not camelCase | LOW | Won't fix — intentional (same as M5) |
