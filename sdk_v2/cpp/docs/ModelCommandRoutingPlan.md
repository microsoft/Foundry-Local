# Model Command Routing (External Service Mode) — C++ Implementation Plan

## Overview

When the SDK is configured with `Configuration::external_service_url`, the three
**model management commands** must be redirected to that remote Foundry Local
service over HTTP instead of being executed against the in-process model loader:

| Command     | External endpoint                          |
|-------------|--------------------------------------------|
| List loaded | `GET {url}/models/loaded`                  |
| Load        | `GET {url}/models/load/{url-encoded id}`   |
| Unload      | `GET {url}/models/unload/{url-encoded id}` |

The OpenAI-compatible endpoints (`/v1/models`, `/v1/chat/completions`, …) are
**out of scope** — callers reach those directly, not through the SDK.

In the v1 SDKs this redirection was reimplemented in every language binding
(`ModelLoadManager.cs`, `model_load_manager.py`, `modelLoadManager.ts`,
`model_load_manager.rs`). In v2 it is handled **once** in the C++ core so every
binding inherits it for free.

**v1 reference implementation:**
- `sdk/cs/src/Detail/ModelLoadManager.cs`
- `sdk/python/src/detail/model_load_manager.py`
- `sdk/js/src/detail/modelLoadManager.ts`
- `sdk/rust/src/detail/model_load_manager.rs`

---

## Why a new component (not augmenting `ModelLoadManager`)

The v2 `ModelLoadManager`
([`inferencing/model_load_manager.h`](../src/inferencing/model_load_manager.h)) is
**not** the v1 `ModelLoadManager`. Despite the shared name they have completely
different responsibilities:

- **v1 `ModelLoadManager`** — a thin *router*: three methods, each branching
  `if (external_url) HTTP else core`.
- **v2 `ModelLoadManager`** — the *local ORT GenAI lifecycle owner*: holds the
  `GenAIModelInstance` map, an `IEpDetector&`, per-model session refcounts,
  `RejectNewLoads()`, and `UnloadAll()` drain-on-shutdown.

Bolting `external_service_url` + HTTP into the v2 class would conflate two
unrelated reasons-to-change ("own in-process ORT GenAI instances" vs. "talk to a
remote server") and its signatures fight it: `LoadModel` returns
`LoadResult{ GenAIModelInstance* }` and `GetLoadedModel` returns a live local
instance — neither exists in external mode. `UnloadAll`, `RejectNewLoads`, EP
detection, and session drain are all meaningless remotely.

**Decision:** introduce a higher-level façade, `ModelCommandRouter`, that owns the
local-vs-external decision for only the three management commands. The v2
`ModelLoadManager` stays a pure local lifecycle owner.

---

## Architecture

```
flModel.Load/Unload          flCatalog.GetLoadedModels
        │                              │
        ▼                              ▼
   Model::Load/Unload/IsLoaded   BaseModelCatalog::GetLoadedModels
        │                              │
        └──────────────┬───────────────┘
                       ▼
              ModelCommandRouter            ← owns local-vs-external decision
              ├── local  ──► ModelLoadManager (LoadModel / UnloadModel / GetLoadedModelIds)
              └── external ─► http_client → {external_service_url}/models/...
```

### Router interface

```cpp
class ModelCommandRouter {
 public:
  ModelCommandRouter(std::optional<std::string> external_service_url,
                     ModelLoadManager& load_manager,
                     std::string app_name,
                     ILogger& logger);

  // Local: ModelLoadManager::LoadModel(local_path, model_id, ep)
  // External: GET /models/load/{id}
  void Load(std::string_view model_id, std::string_view local_path, ExecutionProvider ep);

  // Local: ModelLoadManager::UnloadModel(model_id)
  // External: GET /models/unload/{id}
  void Unload(std::string_view model_id);

  // Local: ModelLoadManager::GetLoadedModel(model_id) != nullptr
  // External: membership test against ListLoadedModelIds()
  bool IsLoaded(std::string_view model_id);

  // Local: ModelLoadManager loaded-map keys
  // External: GET /models/loaded  (single round-trip, JSON array of ids)
  std::vector<std::string> ListLoadedModelIds();
};
```

### Key design principles

1. **`Model` and the catalog become mode-agnostic.** They call the router; they no
   longer know external mode exists. `Model` swaps its `ModelLoadManager*` member for
   a `ModelCommandRouter*`, wired in the same `Model::Create` / `Manager::CreateModel`
   factory that wires it today.
2. **`ModelLoadManager` gains no mode logic.** The router *holds a
   `ModelLoadManager&`* and delegates the local branch to it. The only addition to
   `ModelLoadManager` is one in-character method, `GetLoadedModelIds()`, that reads
   its own loaded map.
3. **List is a single batch call — no N-calls.**
   `BaseModelCatalog::GetLoadedModels()` is rewritten to call
   `router.ListLoadedModelIds()` **once** and filter `models_` by set membership,
   instead of N per-model `IsLoaded()` calls. This also fixes a latent bug: in
   external mode the current per-model `IsLoaded()` path always returns empty.
4. **Local inference is untouched.** External mode means "run inference via the
   external HTTP endpoints." The inference path fetches `GenAIModelInstance*` directly
   in the session layer, never through `Model`/the router, so it needs no change.
5. **Reuse existing infrastructure.**
   [`http/http_client.h`](../src/http/http_client.h) (`HttpGetWithResponse`,
   `DescribeFailure`) for HTTP + errors, the shared `UrlEncode` helper in
   [`utils.h`](../src/utils.h) for the `{id}` path segment, and `Configuration::app_name` for the
   `User-Agent`.

---

## Wiring (`Manager`)

`Manager` owns a `std::unique_ptr<ModelCommandRouter>`, declared **after**
`model_load_manager_` so it is destroyed first. Construction order:

```
model_load_manager_  →  model_command_router_  →  catalog_
```

The router is constructed with `config_.external_service_url`,
`*model_load_manager_`, `config_.app_name`, and `*logger_`. It is injected into the
catalog ctor and forwarded into each `Model` via the existing `CreateModel`
factory (replacing the `ModelLoadManager&` argument).

---

## Error handling

External calls use `http::HttpGetWithResponse` and throw on non-2xx / transport
failure via `FL_THROW(FOUNDRY_LOCAL_ERROR_NETWORK, http::DescribeFailure(resp))`,
matching v1 semantics (which threw `FoundryLocalException` on `!IsSuccessStatusCode`).
`ListLoadedModelIds` parses the response body as a JSON array of strings; an empty
body yields an empty list.

---

## Open questions — decisions

1. **Load of an un-cached model in external mode.**
   *Decision: accept the cache-only constraint.* The v2 API is model-object-centric
   and the catalog is cache-only in external mode, so callers can only `Load()` a model
   already present in their local cache. We ship the model-object path now and **defer**
   any `Manager::LoadModelById(id)` escape hatch until there is a concrete need. (v1
   took raw id strings and could load anything the remote had; that capability is
   intentionally not reproduced yet.)

2. **Session-creation guard in external mode.**
   *Decision: add an explicit guard.* [`configuration.h`](../src/configuration.h)
   documents that session creation is blocked in external mode, but `Session_CreateImpl`
   currently has no explicit check — it only fails indirectly (no local instance). Add
   an explicit `FL_THROW(FOUNDRY_LOCAL_ERROR_INVALID_USAGE, …)` guard on the
   session-creation path when `external_service_url` is set, mirroring the existing
   `StartWebService` guard, so the failure is clear and intentional.

3. **Endpoint timeout.**
   *Decision: drop the v1 `timeout` query param; set a deliberate client-side timeout
   in the core instead.* Investigation of the v1 bindings showed the `timeout` query
   param was **never implemented** — it exists only as a commented-out TODO in C#
   (`// { "timeout", ... }`) and Python (`# "timeout": "30"`), and is absent in JS/Rust.
   So there is no real capability to preserve. The client-side HTTP timeout was also
   inconsistent and accidental rather than designed: Python hard-coded **10s** on all
   three calls (effectively a latent bug — a real model load routinely exceeds 10s),
   C# used the `HttpClient` **default 100s**, and JS/Rust were unbounded.

   Moving this into the C++ core gives every binding a *single, deliberate* value for
   the first time. Use a generous `HttpRequestOptions.timeout` for the load call
   (target ~5 minutes — model load can be slow) and the default for list/unload. We do
   **not** reproduce Python's 10s. A future server-side `timeout` query param can be
   added later if the remote service ever supports one.

---

## Work breakdown

**Implementation (`@CppCoder`)**
- New `src/model_command_router.{h,cc}`.
- Add `ModelLoadManager::GetLoadedModelIds()` (reads its own loaded map).
- Swap `Model`'s member `ModelLoadManager* → ModelCommandRouter*`; update
  `Model::Create` / `Manager::CreateModel` wiring.
- Rewrite `BaseModelCatalog::GetLoadedModels()` to the single-batch path; thread the
  router through the catalog ctor.
- Construct/own the router in `Manager` with correct declaration order.
- Add the external-mode session-creation guard (open question #2).

**Tests (`@Tester`)**
- Router unit tests covering both branches (mock/loopback the HTTP side).
- External-mode integration test mirroring v1 `TestModelLoadManagerExternalService`:
  start a real local web service, point a second `Manager` at it via
  `external_service_url`, verify load / unload / list round-trip.
- Regression test that `GetLoadedModels()` is correct in external mode.
- Test that session creation throws in external mode.

**Review (`@Reviewer`)**
- Confirm `ModelLoadManager` gained no mode logic.
- Confirm the router holds no inference/EP concerns.
- Confirm error/ownership semantics match SDK conventions.
