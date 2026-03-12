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
import json
import logging
import os
import sys

from dataclasses import dataclass
from pathlib import Path

if sys.version_info >= (3, 11):
    from enum import StrEnum
else:
    from enum import Enum

    class StrEnum(str, Enum):
        def __str__(self) -> str:
            return self.value

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

# Mapping from PyPI package name to Python import name.
# These packages ship native binaries as part of their wheel distribution.
_PACKAGE_IMPORT_NAMES: dict[str, str] = {
    "foundry-local-core": "foundry_local_core",
    "onnxruntime-foundry": "onnxruntime_foundry",
    "onnxruntime-genai": "onnxruntime_genai",
}

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


def _find_file_in_package(import_name: str, filename: str) -> Path | None:
    """Locate a native binary *filename* inside an installed Python package.

    Searches the package root and common sub-directories (``capi/``,
    ``native/``, ``lib/``).  Falls back to a recursive ``rglob`` scan of
    the entire package tree when none of the quick paths match.

    Args:
        import_name: The importable Python name of the package (e.g.
            ``"onnxruntime_genai"``).
        filename: The filename to look for (e.g. ``"onnxruntime-genai.dll"``).

    Returns:
        Absolute ``Path`` to the file, or ``None`` if not found.
    """
    spec = importlib.util.find_spec(import_name)
    if spec is None or spec.origin is None:
        return None

    pkg_root = Path(spec.origin).parent

    # Quick checks for well-known sub-directories first
    for candidate_dir in (pkg_root, pkg_root / "capi", pkg_root / "native", pkg_root / "lib"):
        candidate = candidate_dir / filename
        if candidate.exists():
            return candidate

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

    core_path = _find_file_in_package("foundry_local_core", core_name)
    ort_path = _find_file_in_package("onnxruntime_foundry", ort_name)
    genai_path = _find_file_in_package("onnxruntime_genai", genai_name)

    if core_path and ort_path and genai_path:
        return NativeBinaryPaths(core=core_path, ort=ort_path, genai=genai_path)

    return None


def create_ort_symlinks(paths: NativeBinaryPaths) -> None:
    """Create an ``onnxruntime.dll`` compatibility symlink on Linux/macOS.

    Workaround for ORT issue https://github.com/microsoft/onnxruntime/issues/27263.
    The native Core library expects ``onnxruntime.dll`` but on Linux/macOS the
    actual file is named ``libonnxruntime.so`` / ``libonnxruntime.dylib``.
    The symlink is placed in the same directory as the ORT binary.
    """
    if sys.platform == "win32":
        return

    ext = ".dylib" if sys.platform == "darwin" else ".so"
    lib_name = f"libonnxruntime{ext}"
    link_name = "onnxruntime.dll"
    link_path = paths.ort_dir / link_name

    if not link_path.exists():
        ort_file = paths.ort_dir / lib_name
        if ort_file.exists():
            os.symlink(lib_name, link_path)
            print(f"  Created symlink: {link_path} -> {lib_name}")


# ---------------------------------------------------------------------------
# CLI entry point for verifying native binary installation
# ---------------------------------------------------------------------------


def verify_native_install(args: list[str] | None = None) -> None:
    """CLI entry point for verifying the native binary installation.

    Usage::

        foundry-local-install [--verbose]

    The native libraries are provided by the Python packages
    ``foundry-local-core``, ``onnxruntime-foundry``, and
    ``onnxruntime-genai`` which are installed automatically as SDK
    dependencies.  This command reports where those binaries were found.
    """
    parser = argparse.ArgumentParser(
        description=(
            "Verify that the platform-specific native libraries required by "
            "the Foundry Local SDK are present.  The libraries are provided "
            "by the 'foundry-local-core', 'onnxruntime-foundry', and "
            "'onnxruntime-genai' Python packages."
        ),
        prog="foundry-local-install",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print the resolved path for each binary.",
    )
    parsed = parser.parse_args(args)

    paths = get_native_binary_paths()
    if paths is None:
        core_name, ort_name, genai_name = _native_binary_names()
        missing: list[str] = []
        if _find_file_in_package("foundry_local_core", core_name) is None:
            missing.append("foundry-local-core")
        if _find_file_in_package("onnxruntime_foundry", ort_name) is None:
            missing.append("onnxruntime-foundry")
        if _find_file_in_package("onnxruntime_genai", genai_name) is None:
            missing.append("onnxruntime-genai")
        print(
            "[foundry-local] ERROR: Could not locate native binaries. "
            f"Ensure the following packages are installed: {', '.join(missing)}",
            file=sys.stderr,
        )
        print(
            "  Install them with: pip install foundry-local-core onnxruntime-foundry onnxruntime-genai",
            file=sys.stderr,
        )
        sys.exit(1)

    print("[foundry-local] Native libraries found.")
    if parsed.verbose:
        print(f"  Core    : {paths.core}")
        print(f"  ORT     : {paths.ort}")
        print(f"  GenAI   : {paths.genai}")



