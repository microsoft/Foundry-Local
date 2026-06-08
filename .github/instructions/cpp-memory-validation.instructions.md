---
description: Use when validating memory management of the C++ SDK with AddressSanitizer/UBSan, or when running run_sanitizer_tests.py.
applyTo: sdk_v2/cpp/**
---

# C++ SDK memory validation (ASan + UBSan)

Opt-in path for validating that code under `sdk_v2/cpp/src` manages memory
correctly. Runs the C++ test binaries with AddressSanitizer + Undefined
Behavior Sanitizer instrumentation. **Linux / WSL only**, and **not part of
CI** — this is an occasional manual validation pass.

## When to run it

- Before merging non-trivial changes to anything that handles raw pointers,
  manual lifetimes, threading state, or interop with ORT / ORT-GenAI.
- When triaging a suspected memory bug that doesn't reproduce under normal
  Debug builds.
- After a dependency bump (ORT, ORT-GenAI, oat++) to confirm we're not
  inheriting new issues from a third-party library.

It is **not** a CI gate. It is **not** automatic. Run it deliberately.

## Prerequisites

- Linux or WSL (Ubuntu 22.04+ recommended). The CMake option fails fast on
  any other platform.
- A clang or gcc toolchain with ASan + UBSan support (default Ubuntu
  toolchains are fine; clang gives slightly nicer reports).
- `python sdk_v2/cpp/build.py --configure --build` already works on the
  machine. ASan reuses the same toolchain and dependencies.

## One-command invocation

Fast pass — unit tests only, no model loading (~seconds after build):

```bash
python sdk_v2/cpp/scripts/run_sanitizer_tests.py --unit-only
```

Full integration run, scoped to a single fixture (recommended — the full
integration suite is slow because of model loading):

```bash
python sdk_v2/cpp/scripts/run_sanitizer_tests.py \
    --gtest_filter "ChatSessionFixture.*"
```

Use `--config Debug` for the cleanest stack traces during triage; the
default `RelWithDebInfo` is closer to a realistic optimized run.

The script invokes `build.py --configure --build
--cmake_extra_defines FOUNDRY_LOCAL_ENABLE_ASAN=ON` for you, so you do not
need to remember the CMake flag.

## Reading the output

- Live output is streamed to the console.
- A complete tee'd copy lands at
  `sdk_v2/cpp/build/Linux/<config>/sanitizer_report.txt`.
- The script prints a final summary with three counts:
  - **AddressSanitizer errors** — heap/stack corruption, use-after-free,
    double-free, etc. **Always investigate.**
  - **LeakSanitizer leaks** — memory still reachable at process exit. Most
    real bugs are here. ORT shutdown noise can also land here; suppress as
    described below.
  - **UBSan reports** — signed overflow, misaligned loads, invalid enum
    values, null deref, etc. **Always investigate.**
- Exit code is non-zero if any of the three counts is non-zero or if a
  test binary itself returned non-zero.

## Adding a suppression

When a leak comes from ORT / ORT-GenAI / another third-party library and
you've confirmed it isn't us, add an entry to
[sdk_v2/cpp/test/asan/lsan.supp](../../sdk_v2/cpp/test/asan/lsan.supp):

```text
leak:onnxruntime::Environment
leak:Generators::OgaHandle
```

Each line is `leak:<substring>`; LeakSanitizer matches the substring against
any frame in the leak's stack trace. Prefer the narrowest fully-qualified
symbol that matches the noisy frame — avoid suppressing a whole library
(`leak:libonnxruntime.so`) unless you have to.

For ASan-specific suppressions (very rare) edit
[sdk_v2/cpp/test/asan/asan.supp](../../sdk_v2/cpp/test/asan/asan.supp).
Memory-corruption errors should almost never be suppressed — fix instead.

## Non-goals

- **Does not validate Windows-specific code paths.** Anything under
  `#ifdef _WIN32` or in `src/util/*_windows.cc` is not exercised here. Run
  the Windows build / tests separately for those.
- **Does not catch concurrency races.** ThreadSanitizer is incompatible
  with ASan and is a separate effort that is not currently wired up.
- **Does not replace the regular test suite.** Run normal
  `python sdk_v2/cpp/build.py --build --test` for functional correctness;
  use this script only for memory validation.
