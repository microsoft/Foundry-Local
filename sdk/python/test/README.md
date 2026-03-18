# Foundry Local Python SDK – Test Suite

This test suite mirrors the structure of the JS (`sdk_v2/js/test/`) and C# (`sdk_v2/cs/test/`) SDK test suites.

## Prerequisites

1. **Python 3.9+** (tested with 3.12/3.13)
2. **SDK installed in editable mode** from the `sdk/python` directory:
   ```bash
   pip install -e .
   ```
3. **Test dependencies**:
   ```bash
   pip install -r requirements-dev.txt
   ```
4. **Test model data** – the `test-data-shared` folder must exist as a sibling of the git repo root
   (e.g. `../test-data-shared` relative to the repo). It should contain cached models for
   `qwen2.5-0.5b` and `whisper-tiny`.

## Running the tests

From the `sdk/python` directory:

```bash
# Run all tests
python -m pytest test/

# Run with verbose output
python -m pytest test/ -v

# Run a specific test file
python -m pytest test/test_catalog.py

# Run a specific test class or function
python -m pytest test/test_catalog.py::TestCatalog::test_should_list_models

# List all collected tests without running them
python -m pytest test/ --collect-only
```

## Test structure

```
test/
├── conftest.py                        # Shared fixtures & config (equivalent to testUtils.ts)
├── test_foundry_local_manager.py      # FoundryLocalManager initialization (2 tests)
├── test_catalog.py                    # Catalog listing, lookup, error cases (9 tests)
├── test_model.py                      # Model caching & load/unload lifecycle (2 tests)
├── detail/
│   └── test_model_load_manager.py     # ModelLoadManager core interop & web service (5 tests)
└── openai/
    ├── test_chat_client.py            # Chat completions, streaming, error validation (7 tests)
    └── test_audio_client.py           # Audio transcription (7 tests)
```

**Total: 32 tests**

## Key conventions

| Concept | Python (pytest) | JS (Mocha) | C# (TUnit) |
|---|---|---|---|
| Shared setup | `conftest.py` (auto-discovered) | `testUtils.ts` (explicit import) | `Utils.cs` (`[Before(Assembly)]`) |
| Session fixture | `@pytest.fixture(scope="session")` | manual singleton | `[Before(Assembly)]` static |
| Teardown | `yield` + cleanup in fixture | `after()` hook | `[After(Assembly)]` |
| Skip in CI | `@skip_in_ci` marker | `IS_RUNNING_IN_CI` + `this.skip()` | `[SkipInCI]` attribute |
| Expected failure | `@pytest.mark.xfail` | N/A | N/A |
| Timeout | `@pytest.mark.timeout(30)` | `this.timeout(30000)` | `[Timeout(30000)]` |

## CI environment detection

Tests that require the web service are skipped when either `TF_BUILD=true` (Azure DevOps) or
`GITHUB_ACTIONS=true` is set.

## Test models

| Alias | Use | Variant |
|---|---|---|
| `qwen2.5-0.5b` | Chat completions | `qwen2.5-0.5b-instruct-generic-cpu:4` |
| `whisper-tiny` | Audio transcription | `openai-whisper-tiny-generic-cpu:2` |
