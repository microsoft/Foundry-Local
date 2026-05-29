# Copilot Instructions — Foundry Local SDK

## Persistent Knowledge

Prefer `.github/instructions/*.instructions.md` files to capture durable architectural facts, conventions, and domain knowledge. Each file should have a `description` (for on-demand discovery) and optionally `applyTo` (for file-glob auto-attachment). Keep facts concise and actionable.

## File Hygiene

- **Temporary and scratch files** (test logs, coverage reports, build artifacts, diagnostic outputs) must be created inside the build output directory (e.g., `sdk_v2/cpp/build/`) so they are ignored by `.gitignore`. Never create them in the repo root or source directories.
- If a command produces output files, direct them to the build directory or a `TestResults/` subdirectory (also gitignored).

## Repository Structure

This is a multi-language SDK monorepo. The primary SDKs live under:

- `sdk_v2/cpp/` — C++ SDK (active development, the focus of most agent work)
- `sdk_v2/cs/` — C# SDK
- `sdk_v2/js/` — JavaScript/TypeScript SDK
- `sdk_v2/python/` — Python SDK
- `sdk/` — Legacy v1 SDKs (do not modify unless explicitly asked)

## C++ SDK Build

- Build system: CMake + vcpkg
- **Always build via `python sdk_v2/cpp/build.py`.** Never invoke `cmake --build` directly and never pass `--build_dir`. Either bypass produces a non-platform-segmented output path (`build/Debug/...` instead of `build/Windows/Debug/...`) that breaks the C# tests, which load the native via an absolute path baked into `foundry_local.native.cfg`. See [.github/instructions/cpp-build.instructions.md](.github/instructions/cpp-build.instructions.md).
- Build output: `sdk_v2/cpp/build/<Windows|Linux|macOS>/<Config>/` (gitignored)
- Tests: Google Test, discovered via `gtest_discover_tests`

## C++ SDK Testing

- **Always use `--gtest_filter`** when verifying specific test fixes. The full integration suite takes ~10 minutes due to model loading.
  - Single test: `.\sdk_integration_tests.exe --gtest_filter="ModelFixture.TestName"`
  - Multiple tests: `--gtest_filter="Test1:Test2:Test3"`
- Test binaries are in `sdk_v2/cpp/build/Windows/RelWithDebInfo/bin/RelWithDebInfo/`
- `sdk_integration_tests.exe` — integration tests (loads models via SharedTestEnv, slow startup)
- `foundry_local_tests.exe` — unit tests (no model loading, fast)

## C++ Code Style

- **Line length limit: 120 characters.** Use the full width for code and comments.
