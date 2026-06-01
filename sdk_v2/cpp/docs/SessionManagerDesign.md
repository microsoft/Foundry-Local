# SessionManager Design

> Unified session tracking and caching for shutdown safety and Responses API
> continuous decoding.

## Status

**Phase 1 complete** — tracking + drain implemented and tested (668 tests passing).
Phase 2 (session caching) is next.

## Problem Statement

### 1. Shutdown Safety

Sessions hold non-owning `GenAIModelInstance&` references. If `ModelLoadManager`
destroys a model while a session is mid-inference, the result is use-after-free.
Today there is no registry of active sessions — the Manager cannot wait for them
to finish before tearing down models.

The paths that create sessions:

| Path | Owner | Lifetime |
|------|-------|----------|
| C API `Session_Create` | Caller (`unique_ptr` / `delete`) | User-controlled |
| Web handler (non-streaming) | Handler scope (stack local) | Request-scoped |
| Web handler (streaming) | `std::thread` scope (moved into lambda) | Thread-scoped |

All hold dangling references if models are unloaded underneath them.

### 2. Session Caching (Responses API)

The Responses API creates a fresh `ChatSession` per HTTP request. This throws
away the ORT GenAI generator and its KV cache after every turn. For multi-turn
conversations (chained via `previous_response_id`), the entire message history
must be re-processed from scratch each time.

Caching sessions across requests — keyed by response ID — would allow the
generator to accumulate KV cache state and only process new tokens on each
turn. This is a significant latency win for multi-turn Responses API usage.

The existing `ResponseStore` has a TODO acknowledging this:
> "We should instead be using cached Session instances to store conversations
> as that will include the generator as well."

## Design

### Architecture

```
Session ──depends on──► ISessionManager  (Register/Deregister)
                              ▲
                              │ implements
                              │
Manager ──owns──────► SessionManager     (tracking + caching + drain)
                              ▲
                              │ reference
                              │
ServiceContext ───────► SessionManager&   (handlers call caching API)
```

### ISessionManager (Interface)

Minimal contract that `Session` depends on. Lives in a header visible to Session
without pulling in the full SessionManager.

```cpp
class ISessionManager {
 public:
  virtual ~ISessionManager() = default;
  virtual void Register(Session& session) = 0;
  virtual void Deregister(Session& session) = 0;
};
```

The interface is deliberately minimal — just tracking. Shutdown state is not
exposed here; `Manager::IsShutdownRequested()` (via C ABI) is the single source
of truth for "is shutting down". The `shutting_down_` flag inside
`SessionManager` is purely internal, used only by `Register()` to reject new
sessions after `CancelAll()`.

### SessionManager (Concrete)

Owned by Manager. Implements tracking, drain, and (in Phase 2) caching.

```cpp
class SessionManager : public ISessionManager {
 public:
  SessionManager(ILogger& logger);
  ~SessionManager() override;  // calls WaitForDrain

  // --- ISessionManager (tracking) ---
  void Register(Session& session) override;    // throws if shutting down
  void Deregister(Session& session) override;  // throws if not registered (enforces 1:1)

  // --- Shutdown ---
  void CancelAll();
  void WaitForDrain(std::chrono::milliseconds timeout = std::chrono::seconds(10));
  size_t ActiveCount() const;

  // --- Session cache (Phase 2) ---
  // Check-out / check-in model: CheckOut removes from cache and transfers
  // ownership to caller. CheckIn inserts (back) into cache. A checked-out
  // session cannot be evicted or accessed by concurrent requests.
  std::unique_ptr<ChatSession> CheckOut(const std::string& key);
  void CheckIn(const std::string& key, std::unique_ptr<ChatSession> session);
};
```

`Deregister` enforces 1:1 pairing with `Register` — calling it for a session
that was never registered (or already deregistered) throws. This catches
lifetime bugs early.

### Session Integration

Session takes `ISessionManager&` — a non-nullable reference. The contract is
explicit: every Session is tracked.

```cpp
// Session constructor registers
Session::Session(const Model& model, ILogger& logger, ITelemetry& telemetry,
                 ISessionManager& session_manager, bool allow_concurrent)
    : /* existing init */,
      session_manager_(session_manager) {
  session_manager_.Register(*this);
}

// Session destructor deregisters
Session::~Session() {
  session_manager_.Deregister(*this);
}
```

Note: `GenAIModelInstance&` is held by the derived classes (`ChatSession`,
`AudioSession`), not the `Session` base. This allows future session types
that use different model backends.

Every destruction path — `delete`, stack unwind, `unique_ptr` reset, cache
eviction — triggers `~Session` → `Deregister`. No special cases.

### Ownership Model

| Session kind | Who owns | How deregistered |
|-------------|----------|-----------------|
| C API | Caller (`delete` / `Session_Release`) | `~Session` → Deregister |
| Web handler (non-streaming) | Handler scope | `~Session` → Deregister |
| Web handler (streaming) | Thread scope | `~Session` → Deregister |
| Cached (Responses API) | SessionManager (cache map) | `~Session` → Deregister |
| Checked-out (Responses API, mid-request) | Handler (`unique_ptr`) | `~Session` → Deregister |

Tracking and caching are independent axes:
- **All** sessions are tracked (registered/deregistered via `ISessionManager`)
- **Some** sessions are additionally cached (owned by SessionManager in a keyed map)

A cached session being evicted is just the SessionManager dropping its
`unique_ptr`. The destructor fires, `Deregister` runs. The tracking layer
doesn't know or care whether the session was cached.

### Manager Member Ordering

```
config_              — trivial data
logger_              — everything logs through this, destroyed last
ep_detector_         — HW acceleration detection
telemetry_           — telemetry throughout
catalog_             — owns Model instances
download_manager_    — uses ModelInfo from catalog
model_load_manager_  — owns loaded GenAI model instances
session_manager_     — tracks/owns sessions referencing loaded models
shutdown_requested_  — atomic flag
web_service_         — HTTP layer using all of the above
```

Destruction order (reverse):
1. `web_service_` — drains HTTP connections, joins streaming threads
2. `session_manager_` — `WaitForDrain()` blocks until active sessions finish,
   then destroys cached sessions. All sessions gone before models freed.
3. `model_load_manager_` — safely destroys models (no active sessions)
4. Everything else

### ServiceContext

`ServiceContext` gains `SessionManager&` (concrete type, not interface). Web
service handlers need the caching API, which is on `SessionManager`, not
`ISessionManager`.

```cpp
struct ServiceContext {
  ICatalog& catalog;
  ILogger& logger;
  std::string model_cache_dir;
  std::vector<std::string> bound_urls;
  ModelLoadManager& model_load_manager;
  SessionManager& session_manager;
  ResponseStore& response_store;
  ITelemetry& telemetry;
  StreamingThreadTracker& thread_tracker;
};
```

### Shutdown Sequence

```
Manager::Shutdown()
  shutdown_requested_ = true
  model_load_manager_->RejectNewLoads()   // already implemented
  session_manager_->CancelAll()           // Phase 1

Manager::~Manager()                       // called by Destroy()
  Shutdown()                              // idempotent if already called
  web_service_->Stop()                    // drains HTTP, joins threads
  // implicit destruction order:
  //   ~session_manager_  → WaitForDrain() → destroy cached sessions
  //   ~model_load_manager_ → destroy models (safe, no sessions)
```

`CancelAll()`:
1. Sets internal `shutting_down_` flag → `Register()` throws on new sessions
2. Future (Phase 3): iterates tracked sessions and calls `Session::Cancel()` for faster drain

`WaitForDrain()`:
- Blocks on a condition variable until the active session count reaches zero
- Condition is signaled by `Deregister()` when it decrements to zero
- Has a timeout (default 10s) after which it logs a warning and proceeds

### Test Strategy

Tests that don't use Manager create a `NullSessionManager` (test utility):

```cpp
// In test/ only
class NullSessionManager : public ISessionManager {
 public:
  void Register(Session&) override {}
  void Deregister(Session&) override {}
};
```

This is a test helper, not shipped in the library. Production code always has
a real `SessionManager` because Manager creates one.

Tests that use `SharedTestEnv` (which creates a Manager) automatically get the
real `SessionManager` — no test changes needed for those.

## Implementation Plan

### Phase 1: Tracking + Drain (Shutdown Safety) ✅

**New files:**
- `src/inferencing/session/session_manager.h` — `ISessionManager` + `SessionManager`
- `src/inferencing/session/session_manager.cc` — implementation

**Modified files:**
- `src/inferencing/session/session.h` — add `ISessionManager&` member
- `src/inferencing/session/session.cc` — Register in `Create()`, pass through constructors
- `src/inferencing/generative/chat/chat_session.h` — constructor gains `ISessionManager&`
- `src/inferencing/generative/chat/chat_session.cc` — pass to base
- `src/inferencing/generative/audio/audio_session.h` — constructor gains `ISessionManager&`
- `src/inferencing/generative/audio/audio_session.cc` — pass to base
- `src/manager.h` — add `unique_ptr<SessionManager>` member
- `src/manager.cc` — create SessionManager, wire into Shutdown()
- `src/service/web_service.h` — add `SessionManager&` to `ServiceContext`
- `src/service/web_service.cc` — pass through to ServiceContext
- `src/service/chat_completions_handler.cc` — pass `session_manager` to ChatSession
- `src/service/responses_handler.cc` — pass `session_manager` to ChatSession
- `src/service/audio_transcriptions_handler.cc` — pass `session_manager` to AudioSession
- `test/` — add `NullSessionManager` helper, update tests that construct sessions directly

**Outcome:** Every session is tracked. `Manager::Shutdown()` calls `CancelAll()`.
`~SessionManager` waits for drain before models are destroyed.

**Implemented.** All 668 tests passing (661 unit + 7 cache-only).

### Phase 2: Session Caching (Responses API Performance)

**Modified files:**
- `src/inferencing/session/session_manager.h` — add LRU cache map + `CheckOut`, `CheckIn`
- `src/inferencing/session/session_manager.cc` — LRU cache implementation (capacity 5)
- `src/service/responses_handler.cc` — check-out/check-in pattern for cached sessions

**Cache semantics (check-out / check-in):**

`CheckOut(key)` removes the session from the cache and returns `unique_ptr`
ownership to the caller. While checked out, the session cannot be evicted or
accessed by concurrent requests — it's not in the cache.

`CheckIn(key, session)` inserts the session into the cache under the given key.
May evict the LRU entry if at capacity (only idle/parked sessions can be evicted
since in-use sessions are checked out).

**Responses API flow:**
```
1. Handler receives request with previous_response_id = "resp-abc"
2. session = CheckOut("resp-abc")            → cache miss: nullptr, create fresh
                                             → cache hit: session removed from cache
3. ... run inference ...
4a. Success:    CheckIn("resp-xyz", session) → parked under new response ID
4b. Rewind OK: CheckIn("resp-abc", session)  → parked under original ID
4c. Corrupted:  session dropped              → destroyed, Deregister fires
```

No `GetOrCreateCached` needed — the handler creates sessions on cache miss,
and always does `CheckIn` on success. Creation uses the normal `ChatSession`
constructor.

**Outcome:** Responses API reuses KV cache across chained requests.
`ResponseStore` can be simplified to just store the JSON response objects
without being involved in conversation state.

### Phase 3: Session Cancellation (Faster Drain)

**Modified files:**
- `src/inferencing/session/session.h` — add `Cancel()` virtual method + `cancelled_` atomic
- `src/inferencing/generative/chat/chat_session.cc` — check cancelled flag in generation loop
- `src/inferencing/session/session_manager.cc` — `CancelAll()` iterates and calls `Cancel()`

**Outcome:** `Shutdown()` actively interrupts in-flight generation instead of
waiting for natural completion. Critical for large context / long generation
requests.

## Resolved Design Questions

1. **Cache key for Responses API:** Keyed by response ID. When
   `previous_response_id` is set, the handler looks up the cached session by
   the previous response's ID, runs the new turn, then **re-keys** the session
   under the new response ID (rename-in-place). Conversation branching (looking
   up a previous response ID that has already been re-keyed to a newer ID) is
   **not supported** — it would require duplicating the ORT GenAI generator and
   KV cache, which may not even be possible. The handler returns an error if
   the previous response's session has already been superseded.

2. **Cache capacity:** Default capacity of **5**. Sessions are heavy (OGA
   generator + GPU memory for KV cache), so a small limit is appropriate.
   LRU eviction. Future work: ensure eviction does not remove a session that
   is currently mid-inference (check active tracking state before evicting).

3. **Chat Completions caching:** **Not supported.** Chat Completions sessions
   remain stateless per-request. Users who need cross-request session caching
   should use the Responses API.
