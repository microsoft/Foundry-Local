"""Runtime discovery of the foundry_local native library.

Search order:
1. ``FOUNDRY_LOCAL_LIB_DIR`` environment variable
2. Wheel-bundled location: ``<_native>/<platform>/foundry_local.{ext}``
3. Development build: walk up from ``__file__`` looking for
   ``sdk_v2/cpp/build/Windows/RelWithDebInfo/bin/RelWithDebInfo/<name>``
4. System path fallback — return the bare library name and let the OS resolve it
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


def find_library() -> pathlib.Path:
    """Return the path to the foundry_local native library.

    Raises ``FileNotFoundError`` only when the system-path fallback is
    explicitly known to be absent; for all other cases the caller will receive
    an OS error when it tries to dlopen the returned path.
    """
    name = _lib_name()

    # 1. Explicit directory override via environment variable.
    env_dir = os.environ.get("FOUNDRY_LOCAL_LIB_DIR")
    if env_dir:
        candidate = pathlib.Path(env_dir) / name
        if candidate.exists():
            return candidate
        # Honour the env var even if the file isn't there yet — the error
        # message from dlopen will point at the right path.
        return candidate

    # 2. Wheel-bundled location: _native/<platform>/foundry_local.{ext}
    native_pkg_dir = pathlib.Path(__file__).resolve().parent
    wheel_candidate = native_pkg_dir / _platform_subdir() / name
    if wheel_candidate.exists():
        return wheel_candidate

    # 3. Development build — walk up the directory tree looking for sdk_v2/.
    #    The Windows build output lands at:
    #      sdk_v2/cpp/build/Windows/RelWithDebInfo/bin/RelWithDebInfo/
    for parent in pathlib.Path(__file__).resolve().parents:
        sdk_v2 = parent / "sdk_v2"
        if sdk_v2.is_dir():
            dev_candidate = (
                sdk_v2
                / "cpp"
                / "build"
                / "Windows"
                / "RelWithDebInfo"
                / "bin"
                / "RelWithDebInfo"
                / name
            )
            if dev_candidate.exists():
                return dev_candidate
            # Also check Linux/macOS build layouts if we're not on Windows.
            for linux_candidate in sdk_v2.glob(f"cpp/build/**/{name}"):
                return linux_candidate
            break  # Found sdk_v2/ but no library — fall through to system path.

    # 4. System path fallback — let cffi's dlopen use the OS search path.
    return pathlib.Path(name)
