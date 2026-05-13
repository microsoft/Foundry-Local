"""Runtime discovery of the foundry_local native library.

Search order:
1. ``FOUNDRY_LOCAL_LIB_DIR`` environment variable
2. Wheel-bundled location: ``<_native>/<platform>/foundry_local.{ext}``
3. Development build: walk up from ``__file__`` looking for ``sdk_v2/cpp/build/<Platform>/<Config>/...``
4. System path fallback — return ``None`` and let the OS resolve the bare library name at dlopen time
"""

from __future__ import annotations

import importlib.util
import logging
import os
import pathlib
import platform
import sys

logger = logging.getLogger(__name__)


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


# ---------------------------------------------------------------------------
# ORT / GenAI native dependency discovery
#
# foundry_local.{dll|so|dylib} is dynamically linked against onnxruntime and
# onnxruntime-genai. Those libraries ship in separate PyPI packages
# (onnxruntime-{core,gpu}, onnxruntime-genai-{core,cuda}) declared as
# install-time deps in pyproject.toml. At process start we have to:
#   * On Windows: add each package's bin dir to the DLL search path so the
#     loader can resolve onnxruntime.dll / onnxruntime-genai.dll when our
#     foundry_local.dll is loaded.
#   * On Linux/macOS: bridge the "lib" filename prefix mismatch
#     (libonnxruntime.so vs onnxruntime.so the binary was linked against)
#     by symlinking, since dlopen has no equivalent of add_dll_directory
#     and we don't want to mutate LD_LIBRARY_PATH for the whole process.
# ---------------------------------------------------------------------------

# On Linux/macOS the ORT packages ship their shared libs with a "lib" prefix;
# Windows uses no prefix. Our foundry_local binary was linked against the
# unprefixed names on every platform.
_ORT_PREFIX = "" if sys.platform == "win32" else "lib"


def _native_binary_names() -> tuple[str, str]:
    """Return ``(ort_filename, genai_filename)`` for the current platform."""
    ext = _lib_name().rsplit(".", 1)[-1]
    ext = "." + ext
    return (f"{_ORT_PREFIX}onnxruntime{ext}", f"{_ORT_PREFIX}onnxruntime-genai{ext}")


def _find_file_in_package(package_name: str, filename: str) -> pathlib.Path | None:
    """Locate a native binary *filename* inside an installed Python package.

    Probes the package root and well-known sub-dirs (``capi/``, ``native/``,
    ``lib/``, ``bin/``) before falling back to a recursive scan. Accepts
    hyphenated package names (translated to underscores for import lookup).
    """
    import_name = package_name.replace("-", "_")
    spec = importlib.util.find_spec(import_name)
    if spec is None or spec.origin is None:
        return None

    pkg_root = pathlib.Path(spec.origin).parent

    for candidate_dir in (pkg_root, pkg_root / "capi", pkg_root / "native", pkg_root / "lib", pkg_root / "bin"):
        # Glob with a wildcard around the filename to tolerate versioned suffixes
        # (e.g. libonnxruntime.so.1.25.1) but skip debug-info side files.
        candidates = [p for p in candidate_dir.glob(f"*{filename}*") if not p.name.endswith(".dbg")]
        if candidates:
            return candidates[0]

    # Recursive fallback — slow but only hit when the layout is unexpected.
    for match in pkg_root.rglob(filename):
        return match

    return None


def _resolve_ort_package_path(filename: str) -> pathlib.Path | None:
    """Locate ORT shared library, preferring the platform-specific variant."""
    if sys.platform.startswith("linux"):
        primary, fallback = "onnxruntime-gpu", "onnxruntime"
    else:
        primary, fallback = "onnxruntime-core", "onnxruntime"
    return _find_file_in_package(primary, filename) or _find_file_in_package(fallback, filename)


def _resolve_genai_package_path(filename: str) -> pathlib.Path | None:
    """Locate GenAI shared library, preferring the platform-specific variant."""
    if sys.platform.startswith("linux"):
        primary, fallback = "onnxruntime-genai-cuda", "onnxruntime-genai"
    else:
        primary, fallback = "onnxruntime-genai-core", "onnxruntime-genai"
    return _find_file_in_package(primary, filename) or _find_file_in_package(fallback, filename)


def find_ort_native_dirs() -> list[pathlib.Path]:
    """Return the deduplicated directories that contain the ORT and GenAI shared libs.

    Empty list if neither package is importable — the caller decides how to react
    (typically: log a warning and let the OS loader produce a clear error).
    """
    ort_name, genai_name = _native_binary_names()
    found: list[pathlib.Path] = []
    for path in (_resolve_ort_package_path(ort_name), _resolve_genai_package_path(genai_name)):
        if path is None:
            continue
        parent = path.parent.resolve()
        if parent not in found:
            found.append(parent)
    return found


def _create_ort_symlinks(foundry_local_dir: pathlib.Path) -> None:
    """POSIX workaround for ORT issue #27263.

    foundry_local was linked against the unprefixed names ``onnxruntime{.so|.dylib}``
    and ``onnxruntime-genai{.so|.dylib}`` but the ORT/GenAI packages ship the libs
    with a ``lib`` prefix. Create symlinks next to foundry_local so its dlopen
    resolves them, plus a bridge symlink in the GenAI dir so GenAI's own RPATH
    finds ORT.
    """
    ort_name, genai_name = _native_binary_names()
    ort_path = _resolve_ort_package_path(ort_name)
    genai_path = _resolve_genai_package_path(genai_name)

    ext = ".dylib" if sys.platform == "darwin" else ".so"
    targets: list[tuple[pathlib.Path | None, str]] = [
        (ort_path, "onnxruntime"),
        (genai_path, "onnxruntime-genai"),
    ]

    for src_path, link_stem in targets:
        link_path = foundry_local_dir / f"{link_stem}{ext}"
        if link_path.exists() or link_path.is_symlink():
            continue
        if src_path is None or not src_path.exists():
            logger.warning("Cannot create symlink %s: source binary not found", link_path)
            continue
        try:
            os.symlink(str(src_path), link_path)
            logger.info("Created native dep symlink: %s -> %s", link_path, src_path)
        except OSError as exc:
            logger.warning("Failed to create symlink %s -> %s: %s", link_path, src_path, exc)

    # GenAI is linked against libonnxruntime via its own RPATH; if ORT lives in a
    # different directory we drop a symlink alongside GenAI to bridge the lookup.
    if ort_path is not None and genai_path is not None and ort_path.parent != genai_path.parent:
        bridge = genai_path.parent / ort_path.name
        if not (bridge.exists() or bridge.is_symlink()):
            try:
                os.symlink(str(ort_path), bridge)
                logger.info("Created GenAI->ORT bridge symlink: %s -> %s", bridge, ort_path)
            except OSError as exc:
                logger.warning("Failed to create bridge symlink %s -> %s: %s", bridge, ort_path, exc)


def prepare_native_dependencies(foundry_local_dir: pathlib.Path) -> None:
    """Best-effort wiring so the OS loader can resolve ORT and GenAI when our binary loads.

    Idempotent and never raises; on failure the OS loader's own error is the source of truth.
    Call this BEFORE importing the cffi extension that links against foundry_local.
    """
    try:
        dirs = find_ort_native_dirs()
    except Exception as exc:  # noqa: BLE001 — best-effort wiring, never propagate
        logger.warning("ORT/GenAI discovery failed: %s", exc)
        return

    if not dirs:
        logger.info("ORT/GenAI native packages not found via importlib; relying on OS search path.")
        return

    if sys.platform == "win32":
        add_dll_directory = getattr(os, "add_dll_directory", None)
        if add_dll_directory is None:
            return
        for d in dirs:
            try:
                add_dll_directory(str(d))
            except OSError as exc:
                logger.warning("os.add_dll_directory(%s) failed: %s", d, exc)
        return

    # POSIX: create the lib*-prefix bridge symlinks. Best effort.
    try:
        _create_ort_symlinks(foundry_local_dir)
    except Exception as exc:  # noqa: BLE001 — best-effort wiring, never propagate
        logger.warning("Creating ORT symlinks failed: %s", exc)
