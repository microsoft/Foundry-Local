"""Runtime discovery of the foundry_local native library.

Search order:
1. ``FOUNDRY_LOCAL_LIB_DIR`` environment variable
2. Wheel-bundled location: ``<_native>/<platform>/foundry_local.{ext}``
3. Development build: walk up from ``__file__`` looking for ``sdk_v2/cpp/build/<Platform>/<Config>/...``
4. System path fallback — return ``None`` and let the OS resolve the bare library name at dlopen time
"""

from __future__ import annotations

import os
import pathlib
import platform
import sys


def _lib_name() -> str:
    if sys.platform == "win32":
        return "foundry_local.dll"
    if sys.platform == "darwin":
        return "libfoundry_local.dylib"
    return "libfoundry_local.so"


def _platform_subdir() -> str:
    if sys.platform == "win32":
        return "win-x64"
    if sys.platform == "darwin":
        return "osx-arm64" if platform.machine() == "arm64" else "osx-x64"
    return "linux-x64"


def _build_platform_dir() -> str:
    """Return the platform sub-directory under ``sdk_v2/cpp/build/`` (matches the C++ build script)."""
    if sys.platform == "win32":
        return "Windows"
    if sys.platform == "darwin":
        return "macOS"
    return "Linux"


# Build configurations to try, most-likely first.
_BUILD_CONFIGS = ("RelWithDebInfo", "Release", "Debug")


def _dev_build_candidates(sdk_v2: pathlib.Path, name: str) -> list[pathlib.Path]:
    """Enumerate development-build paths to probe under ``sdk_v2/cpp/build/<Platform>/<Config>/`` for *name*.

    On Windows (multi-config MSBuild generators) the binary lands at ``bin/<Config>/<name>``. On Linux and
    macOS (single-config Make/Ninja generators) it lands at ``bin/<name>`` — we probe both layouts so the same
    code path works on every host.
    """
    platform_dir = _build_platform_dir()
    base = sdk_v2 / "cpp" / "build" / platform_dir
    candidates: list[pathlib.Path] = []
    for config in _BUILD_CONFIGS:
        candidates.append(base / config / "bin" / config / name)  # multi-config (MSBuild)
        candidates.append(base / config / "bin" / name)  # single-config (Ninja/Make)
    return candidates


def find_library() -> pathlib.Path | None:
    """Return the path to the foundry_local native library, or ``None`` if only the system path can resolve it.

    The caller (``_native/api.py``) treats a ``None`` result as "don't try to add a DLL directory; just let the
    cffi extension's own dlopen find the library on the system search path."
    """
    name = _lib_name()

    # 1. Explicit directory override via environment variable.
    env_dir = os.environ.get("FOUNDRY_LOCAL_LIB_DIR")
    if env_dir:
        candidate = pathlib.Path(env_dir) / name
        if candidate.exists():
            return candidate
        # Honour the env var even if the file isn't there yet — the error message from dlopen will point at
        # the right path.
        return candidate

    # 2. Wheel-bundled location: _native/<platform>/foundry_local.{ext}
    native_pkg_dir = pathlib.Path(__file__).resolve().parent
    wheel_candidate = native_pkg_dir / _platform_subdir() / name
    if wheel_candidate.exists():
        return wheel_candidate

    # 3. Development build — walk up the directory tree looking for sdk_v2/ then probe explicit per-platform
    #    build layouts. We deliberately avoid recursive globs here: on Linux/macOS a ``**`` glob over the build
    #    tree can be very slow.
    for parent in pathlib.Path(__file__).resolve().parents:
        sdk_v2 = parent / "sdk_v2"
        if sdk_v2.is_dir():
            for dev_candidate in _dev_build_candidates(sdk_v2, name):
                if dev_candidate.exists():
                    return dev_candidate
            break  # Found sdk_v2/ but no library — fall through to system path.

    # 4. System path fallback — let cffi's dlopen use the OS search path. Returning ``None`` (rather than a
    #    relative ``Path(name)``) prevents callers from accidentally treating CWD as the DLL directory.
    return None
