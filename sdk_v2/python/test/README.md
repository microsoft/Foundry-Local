# Foundry Local Python SDK – Test Suite

This test suite mirrors the structure of the C++ (`sdk_v2/cpp/test/`), C# (`sdk_v2/cs/test/`),
and JS (`sdk_v2/js/test/`) SDK test suites.

## Prerequisites

1. **Python 3.11 – 3.14** (see `requires-python` in `pyproject.toml`).
2. **SDK installed in editable mode** from the `sdk_v2/python` directory, with the `dev`
   extras (pulls in `pytest`, `pytest-cov`, `mypy`, `ruff`):
   ```bash
   cd sdk_v2/python
   pip install -e ".[dev]"
   ```
3. **Native library built** — the cffi extension `foundry_local_sdk._native.api` must be
   importable. Build the C++ SDK first (`python sdk_v2/cpp/build.py`); the Python build
   discovers it via the standard repo layout.
4. **(Integration only) Test model data** — `automatic-speech-recognition`, `chat-completion`,
   and `embeddings` models pre-cached. Either:
   - Point `FOUNDRY_TEST_DATA_DIR` at a pre-staged cache (the CI pattern, mirrors the C++
     `FOUNDRY_TEST_DATA_DIR` env var), **or**
   - Locally run `foundry model download <alias>` to populate the default cache.

   Integration tests **never download** — they skip with a clear reason when no suitable
   model is cached.

## Running the tests

From the `sdk_v2/python` directory:

```bash
# Everything (unit + integration)
python -m pytest test/

# Unit tests only — no native lib, no manager, no models required
python -m pytest test/unit/

# Integration tests only — require native lib + cached models
python -m pytest test/integration/

# Verbose
python -m pytest test/ -v

# Single file / test
python -m pytest test/integration/test_catalog.py
python -m pytest test/integration/test_chat_client.py::test_basic_completion

# List collected tests without running
python -m pytest test/ --collect-only
```

## Test structure

```
test/
├── conftest.py                          # Session fixtures: manager, chat_model,
│                                        # embedding_model, audio_model,
│                                        # whisper_audio_model, native_api
├── unit/                                # Pure unit tests — no native lib required
│   ├── test_chat_settings.py
│   ├── test_configuration.py
│   ├── test_imports.py
│   ├── test_items.py
│   ├── test_live_audio_types.py
│   └── test_session_types.py
└── integration/                         # Native lib + live manager required
    ├── test_audio_client.py
    ├── test_catalog.py
    ├── test_chat_client.py
    ├── test_configuration_native.py
    ├── test_embedding_client.py
    ├── test_ep_lifecycle.py
    ├── test_items.py
    ├── test_live_audio.py
    ├── test_model_lifecycle.py
    ├── test_native_layer.py
    ├── test_session.py
    ├── test_session_validation.py
    ├── test_use_after_close.py
    ├── test_web_service_and_eps.py
    ├── test_zz_manager_shutdown.py      # Runs last (alphabetical) — tears down
    └── test_zz_singleton_recreate.py    #   the manager singleton
```

The `test_zz_*` files intentionally sort last so the singleton-mutating tests don't
poison other integration tests that share the session-scoped `manager` fixture.

## Key conventions

| Concept | Python (pytest) | JS (Mocha) | C# (TUnit) |
|---|---|---|---|
| Shared setup | `conftest.py` (auto-discovered) | `testUtils.ts` (explicit import) | `Utils.cs` (static ctor) |
| Singleton | `@pytest.fixture(scope="session") manager` | manual singleton | `FoundryLocalManager.Instance` |
| Teardown | `yield` + `mgr.close()` in fixture | `after()` hook | `[After(Assembly)]` |
| Integration gating | fixture-driven `pytest.skip(...)` when no cached model | inline `this.skip()` | `[SkipUnlessIntegration]` |
| Native availability | `native_api` fixture skips if `.pyd` unloadable | N/A | manager init fails → all integration skipped |
| Expected failure | `@pytest.mark.xfail` | N/A | N/A |
| Timeout | `@pytest.mark.timeout(30)` | `this.timeout(30000)` | `[Timeout(30000)]` |

## CI behaviour

`conftest.py` detects CI via `TF_BUILD` (Azure DevOps) or `GITHUB_ACTIONS`
(case-insensitive). In CI:

- Models are **never downloaded**. Tests that need a model use only what is already
  in the cache.
- `FOUNDRY_TEST_DATA_DIR` points at the pre-staged cache assembled by the pipeline.
- Tests that find no suitable cached model **skip** with a reason naming the task and
  pointing at the fix (`foundry model download <alias>` locally, or pre-stage in CI).

This matches the C++ integration test policy in
`sdk_v2/cpp/test/sdk_api/shared_test_env.h`.

## Fixtures (in `conftest.py`)

| Fixture | Scope | Notes |
|---|---|---|
| `manager` | session | Initializes `FoundryLocalManager` singleton. Hard failure on init error — a broken manager isn't a "skip", it's a build break. |
| `chat_model` | session | Smallest cached `chat-completion` model, loaded. Skips if none. |
| `embedding_model` | session | Smallest cached `embeddings` model, loaded. Skips if none. |
| `audio_model` | session | Smallest cached `automatic-speech-recognition` model, loaded. Used by streaming live-audio tests (any ASR variant). |
| `whisper_audio_model` | session | Smallest cached whisper-family ASR model. Required by the one-shot `AudioClient.transcribe` path (only whisper decoders implement it today). |
| `native_api` | session | Skips if the cffi extension `foundry_local_sdk._native.api` is unloadable. |
