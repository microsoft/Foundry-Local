# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
"""
Build the Foundry Local C++ SDK with AddressSanitizer + UBSan and run the
test binaries under sanitizer instrumentation.

This is an opt-in, occasional validation pass — not part of CI. It runs on
Linux / WSL only (the CMake option fails fast on other platforms).

See .github/instructions/cpp-memory-validation.instructions.md for the
intended workflow and how to add suppressions for ORT shutdown noise.
"""

from __future__ import annotations

import argparse
import logging
import os
import re
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
CPP_DIR = SCRIPT_DIR.parent
BUILD_PY = CPP_DIR / "build.py"
ASAN_SUPP = CPP_DIR / "test" / "asan" / "asan.supp"
LSAN_SUPP = CPP_DIR / "test" / "asan" / "lsan.supp"


logging.basicConfig(
    format="%(asctime)s run_sanitizer_tests [%(levelname)s] - %(message)s",
    level=logging.INFO,
)
log = logging.getLogger("run_sanitizer_tests")


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--gtest_filter",
        default=None,
        help="GTest filter expression forwarded to the test binaries (default: run all).",
    )
    parser.add_argument(
        "--config",
        default="RelWithDebInfo",
        choices=["Debug", "RelWithDebInfo"],
        help="Build configuration. Debug gives the cleanest stack traces; "
             "RelWithDebInfo is closer to a realistic run. Default: RelWithDebInfo.",
    )
    parser.add_argument(
        "--parallel",
        nargs="?",
        const=0,
        default=None,
        type=int,
        help="Forward --parallel to build.py (optional job count, 0 = all CPUs).",
    )
    scope = parser.add_mutually_exclusive_group()
    scope.add_argument("--unit-only", action="store_true",
                       help="Run only foundry_local_tests (fast, no model loading).")
    scope.add_argument("--integration-only", action="store_true",
                       help="Run only sdk_integration_tests (slow, loads models).")
    return parser.parse_args()


def _require_linux() -> None:
    if not sys.platform.startswith("linux"):
        log.error("This script must run on Linux (including WSL). Detected: %s", sys.platform)
        sys.exit(2)


def _run_build(args: argparse.Namespace) -> Path:
    cmd: list[str] = [
        sys.executable, str(BUILD_PY),
        "--configure", "--build",
        "--config", args.config,
        "--cmake_extra_defines", "FOUNDRY_LOCAL_ENABLE_ASAN=ON",
    ]
    if args.parallel is not None:
        cmd.extend(["--parallel", str(args.parallel)])

    log.info("Building with sanitizers: %s", " ".join(cmd))
    subprocess.run(cmd, check=True)

    build_dir = CPP_DIR / "build" / "Linux" / args.config
    if not build_dir.is_dir():
        log.error("Expected build output at %s but it was not produced.", build_dir)
        sys.exit(2)
    return build_dir


def _make_env() -> dict[str, str]:
    env = os.environ.copy()
    env["ASAN_OPTIONS"] = ":".join([
        "detect_leaks=1",
        "abort_on_error=0",
        "halt_on_error=0",
        "print_stacktrace=1",
        "strict_string_checks=1",
        "check_initialization_order=1",
        "detect_stack_use_after_return=1",
        f"suppressions={ASAN_SUPP}",
    ])
    env["LSAN_OPTIONS"] = ":".join([
        f"suppressions={LSAN_SUPP}",
        "print_suppressions=0",
    ])
    env["UBSAN_OPTIONS"] = ":".join([
        "print_stacktrace=1",
        "halt_on_error=0",
    ])
    return env


def _run_binary(binary: Path, gtest_filter: str | None, env: dict[str, str], report_path: Path) -> int:
    if not binary.is_file():
        log.error("Test binary not found: %s", binary)
        return 2

    cmd: list[str] = [str(binary)]
    if gtest_filter:
        cmd.append(f"--gtest_filter={gtest_filter}")

    log.info("Running: %s", " ".join(cmd))

    # Stream output live and tee to the report. Merge stderr into stdout so
    # sanitizer messages (which go to stderr by default) interleave correctly
    # in the report file in the same order the user sees them on the console.
    with report_path.open("a", encoding="utf-8", errors="replace") as report:
        report.write(f"\n========== {binary.name} ==========\n")
        report.write(f"Command: {' '.join(cmd)}\n\n")
        report.flush()

        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env=env,
            bufsize=1,
            text=True,
        )
        assert proc.stdout is not None
        for line in proc.stdout:
            sys.stdout.write(line)
            report.write(line)
        proc.wait()
        return proc.returncode


# Patterns that indicate sanitizer findings in the output. UBSan formats vary
# across libc versions, but every report includes "runtime error:".
_ASAN_RE = re.compile(r"ERROR: AddressSanitizer")
_LSAN_RE = re.compile(r"ERROR: LeakSanitizer")
_UBSAN_RE = re.compile(r"runtime error:")


def _summarize(report_path: Path) -> tuple[int, int, int]:
    text = report_path.read_text(encoding="utf-8", errors="replace")
    return (
        len(_ASAN_RE.findall(text)),
        len(_LSAN_RE.findall(text)),
        len(_UBSAN_RE.findall(text)),
    )


def main() -> int:
    args = _parse_args()
    _require_linux()

    build_dir = _run_build(args)
    bin_dir = build_dir / "bin"
    report_path = build_dir / "sanitizer_report.txt"
    # Truncate prior report
    report_path.write_text(
        f"Sanitizer run report\nConfig: {args.config}\nFilter: {args.gtest_filter or '(none)'}\n",
        encoding="utf-8",
    )

    env = _make_env()

    targets: list[Path] = []
    if not args.integration_only:
        targets.append(bin_dir / "foundry_local_tests")
    if not args.unit_only:
        targets.append(bin_dir / "sdk_integration_tests")

    worst_rc = 0
    for binary in targets:
        rc = _run_binary(binary, args.gtest_filter, env, report_path)
        if rc != 0:
            log.warning("%s exited with code %d", binary.name, rc)
            worst_rc = max(worst_rc, rc)

    asan_count, lsan_count, ubsan_count = _summarize(report_path)
    log.info("=" * 60)
    log.info("Sanitizer summary:")
    log.info("  AddressSanitizer errors: %d", asan_count)
    log.info("  LeakSanitizer leaks    : %d", lsan_count)
    log.info("  UBSan reports          : %d", ubsan_count)
    log.info("  Report                 : %s", report_path)
    log.info("=" * 60)

    if asan_count or lsan_count or ubsan_count:
        return max(worst_rc, 1)
    return worst_rc


if __name__ == "__main__":
    sys.exit(main())
