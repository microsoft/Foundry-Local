# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Utility functions for the Foundry Local SDK.

Includes native library locator logic and helper functions used by
other SDK modules.
"""

from __future__ import annotations

import argparse
import importlib.util
import logging
import os
import sys

from dataclasses import dataclass
from pathlib import Path

from enum import StrEnum
from ..exception import FoundryLocalException

logger = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Platform helpers
# ---------------------------------------------------------------------------

# Maps Python sys.platform to native shared library extension
EXT_MAP: dict[str, str] = {
    "win32": ".dll",
    "linux": ".so",
    "darwin": ".dylib",
}


def _get_ext() -> str:
    """Get the native library file extension for the current platform."""
    for plat_prefix, ext in EXT_MAP.items():
        if sys.platform.startswith(plat_prefix):
            return ext
    raise RuntimeError(f"Unsupported platform: {sys.platform}")


# ---------------------------------------------------------------------------
# Package-based binary discovery
# ---------------------------------------------------------------------------

# On Linux/macOS the ORT shared libraries carry the "lib" prefix while the
# Core library refers to them without it — a symlink "onnxruntime.dll" →
# "libonnxruntime.so/.dylib" is created to bridge the gap (see below).
_ORT_PREFIX = "" if sys.platform == "win32" else "lib"


def _native_binary_names() -> tuple[str, str, str]:
    """Return the expected native binary filenames for the current platform."""
    ext = _get_ext()
    return (
        f"Microsoft.AI.Foundry.Local.Core{ext}",
        f"{_ORT_PREFIX}onnxruntime{ext}",
        f"{_ORT_PREFIX}onnxruntime-genai{ext}",
    )


def _find_file_in_package(package_name: str, filename: str) -> Path | None:
    """Locate a native binary *filename* inside an installed Python package.

    Searches the package root and common sub-directories (``capi/``,
    ``native/``, ``lib/``).  Falls back to a recursive ``rglob`` scan of
    the entire package tree when none of the quick paths match.

    Args:
        package_name: The PyPI package name (hyphens or underscores accepted;
            e.g. ``"onnxruntime-genai-core"`` or ``"onnxruntime_genai_core"``).
        filename: The filename to look for (e.g. ``"onnxruntime-genai.dll"``).

    Returns:
        Absolute ``Path`` to the file, or ``None`` if not found.
    """
    import_name = package_name.replace("-", "_")
    spec = importlib.util.find_spec(import_name)
    if spec is None or spec.origin is None:
        return None

    pkg_root = Path(spec.origin).parent

    # Quick checks for well-known sub-directories first
    for candidate_dir in (pkg_root, pkg_root / "capi", pkg_root / "native", pkg_root / "lib", pkg_root / "bin"):
        candidates = list(candidate_dir.glob(f"*{filename}*"))
        if candidates:
            return candidates[0]

    # Recursive fallback
    for match in pkg_root.rglob(filename):
        return match

    return None


@dataclass
class NativeBinaryPaths:
    """Resolved paths to the three native binaries required by the SDK."""

    core: Path
    ort: Path
    genai: Path

    @property
    def core_dir(self) -> Path:
        """Directory that contains the Core binary."""
        return self.core.parent

    @property
    def ort_dir(self) -> Path:
        """Directory that contains the OnnxRuntime binary."""
        return self.ort.parent

    @property
    def genai_dir(self) -> Path:
        """Directory that contains the OnnxRuntimeGenAI binary."""
        return self.genai.parent

    def all_dirs(self) -> list[Path]:
        """Return a deduplicated list of directories that contain the binaries."""
        seen: list[Path] = []
        for d in (self.core_dir, self.ort_dir, self.genai_dir):
            if d not in seen:
                seen.append(d)
        return seen


def get_native_binary_paths() -> NativeBinaryPaths | None:
    """Locate native binaries from installed Python packages.

    Returns:
        A :class:`NativeBinaryPaths` instance if all three binaries were
        found, or ``None`` if any is missing.
    """
    core_name, ort_name, genai_name = _native_binary_names()

    # Probe WinML packages first; fall back to standard if not installed.
    core_path = _find_file_in_package("foundry-local-core-winml", core_name) or _find_file_in_package("foundry-local-core", core_name)

    # On Linux, ORT is shipped by onnxruntime-gpu (libonnxruntime.so in capi/).
    if sys.platform.startswith("linux"):
        ort_path = _find_file_in_package("onnxruntime-gpu", ort_name)
    else:
        ort_path = _find_file_in_package("onnxruntime-core", ort_name)

    # On Linux, ORTGenAI is shipped by onnxruntime-genai-cuda (libonnxruntime-genai.so in the package root).
    if sys.platform.startswith("linux"):
        genai_path = _find_file_in_package("onnxruntime-genai-cuda", genai_name)
    else:
        genai_path = _find_file_in_package("onnxruntime-genai-core", genai_name)

    if core_path and ort_path and genai_path:
        return NativeBinaryPaths(core=core_path, ort=ort_path, genai=genai_path)

    return None

def create_ort_symlinks(paths: NativeBinaryPaths) -> None:
    """Create compatibility symlinks for ORT in the Core library directory on Linux/macOS.

    Workaround for ORT issue https://github.com/microsoft/onnxruntime/issues/27263.

    On Linux/macOS the native packages ship ORT binaries with a ``lib`` prefix
    (e.g. ``libonnxruntime.dylib``) in their own package directories, while the
    .NET AOT Core library P/Invokes ``onnxruntime.dylib`` / ``onnxruntime-genai.dylib``
    and searches its *own* directory first (matching the JS SDK behaviour where all
    binaries live in a single ``coreDir``).

    This function creates ``onnxruntime{ext}`` and ``onnxruntime-genai{ext}`` symlinks
    in ``paths.core_dir`` pointing at the absolute paths of the respective binaries so
    the Core DLL can resolve them via ``dlopen`` without needing ``DYLD_LIBRARY_PATH``.
    """
    if sys.platform == "win32":
        return

    ext = ".dylib" if sys.platform == "darwin" else ".so"

    # Pairs of (actual binary path, link stem to create in core_dir)
    links: list[tuple[Path, str]] = [
        (paths.ort,   "onnxruntime"),
        (paths.genai, "onnxruntime-genai"),
    ]

    for src_path, link_stem in links:
        link_path = paths.core_dir / f"{link_stem}{ext}"
        if not link_path.exists():
            if src_path.exists():
                os.symlink(str(src_path), link_path)
                logger.info("Created symlink: %s -> %s", link_path, src_path)
            else:
                logger.warning("Cannot create symlink %s: source %s not found", link_path, src_path)

    # Create a libonnxruntime symlink in genai_dir pointing to the real ORT
    # binary so the dynamic linker can resolve GenAI's dependency.
    if paths.genai_dir != paths.ort_dir:
        ort_link_in_genai = paths.genai_dir / paths.ort.name
        if not ort_link_in_genai.exists():
            if paths.ort.exists():
                os.symlink(str(paths.ort), ort_link_in_genai)
                logger.info("Created symlink: %s -> %s", ort_link_in_genai, paths.ort)
            else:
                logger.warning("Cannot create symlink %s: source %s not found",
                               ort_link_in_genai, paths.ort)


# ---------------------------------------------------------------------------
# CLI entry point for verifying native binary installation
# ---------------------------------------------------------------------------


def foundry_local_install(args: list[str] | None = None) -> None:
    """CLI entry point for installing and verifying native binaries.

    Usage::

        foundry-local-install [--winml] [--verbose]

    Installs the platform-specific native libraries required by the SDK via
    pip, then verifies they can be located.  Use ``--winml`` to install the
    WinML variants of the native packages (Windows only).

    Standard variant (default)::

        foundry-local-install
        # installs: foundry-local-core, onnxruntime-core, onnxruntime-genai-core

    WinML variant::

        foundry-local-install --winml
        # installs: foundry-local-core-winml, onnxruntime-core, onnxruntime-genai-core
    """
    import subprocess

    parser = argparse.ArgumentParser(
        description=(
            "Install and verify the platform-specific native libraries required by "
            "the Foundry Local SDK via pip.  Use --winml to install the WinML variants "
            "(Windows only).  Without --winml the standard cross-platform packages are installed."
        ),
        prog="foundry-local-install",
    )
    parser.add_argument(
        "--winml",
        action="store_true",
        help=(
            "Install WinML native package (foundry-local-core-winml) "
            "instead of the standard cross-platform package."
        ),
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print the resolved path for each binary after installation.",
    )
    parsed = parser.parse_args(args)

    if parsed.winml:
        variant = "WinML"
        packages = ["foundry-local-core-winml", "onnxruntime-core", "onnxruntime-genai-core"]
    elif sys.platform.startswith("linux"):
        variant = "Linux (GPU)"
        packages = ["foundry-local-core", "onnxruntime-gpu", "onnxruntime-genai-cuda"]
    else:
        variant = "standard"
        packages = ["foundry-local-core", "onnxruntime-core", "onnxruntime-genai-core"]

    print(f"[foundry-local] Installing {variant} native packages: {', '.join(packages)}")
    subprocess.check_call([sys.executable, "-m", "pip", "install", *packages])

    paths = get_native_binary_paths()
    if paths is None:
        core_name, ort_name, genai_name = _native_binary_names()
        missing: list[str] = []
        if parsed.winml:
            if _find_file_in_package("foundry-local-core-winml", core_name) is None:
                missing.append("foundry-local-core-winml")
        else:
            if _find_file_in_package("foundry-local-core", core_name) is None:
                missing.append("foundry-local-core")
        if sys.platform.startswith("linux"):
            if _find_file_in_package("onnxruntime", ort_name) is None:
                missing.append("onnxruntime-gpu")
        else:
            if _find_file_in_package("onnxruntime-core", ort_name) is None:
                missing.append("onnxruntime-core")
        if sys.platform.startswith("linux"):
            if _find_file_in_package("onnxruntime-genai", genai_name) is None:
                missing.append("onnxruntime-genai-cuda")
        else:
            if _find_file_in_package("onnxruntime-genai-core", genai_name) is None:
                missing.append("onnxruntime-genai-core")
        print(
            "[foundry-local] ERROR: Could not locate native binaries after installation. "
            f"Missing: {', '.join(missing)}",
            file=sys.stderr,
        )
        hint = "pip install foundry-local-sdk-winml" if parsed.winml else "pip install foundry-local-sdk"
        print(f"  Try: {hint}", file=sys.stderr)
        sys.exit(1)

    print(f"[foundry-local] {variant.capitalize()} native libraries installed and verified.")
    if parsed.verbose:
        print(f"  Core    : {paths.core}")
        print(f"  ORT     : {paths.ort}")
        print(f"  GenAI   : {paths.genai}")
