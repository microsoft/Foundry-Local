# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""PEP 517 build backend shim for foundry-local-sdk.

Delegates all hooks to ``setuptools.build_meta`` after optionally
patching ``pyproject.toml`` and ``requirements.txt`` in-place for the
WinML variant build.

Usage
-----
Standard (default)::

    python -m build --wheel

WinML variant::

    python -m build --wheel -C winml=true

Environment variable fallback (useful in CI pipelines)::

    FOUNDRY_VARIANT=winml python -m build --wheel

CI usage (install without pulling dependencies)::

    pip install --no-deps <wheel>
"""

from __future__ import annotations

import contextlib
import json
import os
from collections.abc import Generator
from pathlib import Path

import setuptools.build_meta as _sb

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

_PROJECT_ROOT = Path(__file__).parent
_PYPROJECT = _PROJECT_ROOT / "pyproject.toml"
_REQUIREMENTS = _PROJECT_ROOT / "requirements.txt"
_REQUIREMENTS_BASE = _PROJECT_ROOT / "requirements-base.txt"

# The exact string in pyproject.toml to patch for the WinML variant.
_STANDARD_NAME = 'name = "foundry-local-sdk"'
_WINML_NAME = 'name = "foundry-local-sdk-winml"'


# ---------------------------------------------------------------------------
# Requirements generation from deps_versions.json
# ---------------------------------------------------------------------------


def _load_deps_versions(*, winml: bool) -> dict:
    """Load the appropriate deps_versions JSON file.

    Standard and WinML each have their own file with identical key structure,
    so callers never need variant-specific key names.
    """
    filename = "deps_versions_winml.json" if winml else "deps_versions.json"
    filepath = _PROJECT_ROOT.parent / filename
    with open(filepath, encoding="utf-8-sig") as f:
        return json.load(f)


def _to_pep440(version: str) -> str:
    """Convert a NuGet-style prerelease version to PEP 440.

    Example: '1.25.0-dev-20260402-0015-6bbcde989a' -> '1.25.0.dev202604020015'
    Stable versions pass through unchanged.
    """
    import re
    m = re.match(r'^(\d+\.\d+\.\d+)-dev-(\d{8})-(\d{4})', version)
    if m:
        return f"{m.group(1)}.dev{m.group(2)}{m.group(3)}"
    return version


def _generate_requirements(*, winml: bool) -> str:
    """Generate requirements.txt content from base deps + deps_versions.json."""
    base = _REQUIREMENTS_BASE.read_text(encoding="utf-8").rstrip("\n")
    deps = _load_deps_versions(winml=winml)

    ort_ver = _to_pep440(deps['onnxruntime']['version'])
    genai_ver = deps['onnxruntime-genai']['version']

    if winml:
        requirement_lines = [
            f"foundry-local-core-winml=={deps['foundry-local-core']['python']}",
            f"onnxruntime-core=={ort_ver}",
            f"onnxruntime-genai-core=={genai_ver}",
        ]
    else:
        requirement_lines = [
            f"foundry-local-core=={deps['foundry-local-core']['python']}",
            f"""onnxruntime-gpu=={ort_ver}; platform_system == "Linux" """.rstrip(),
            f"""onnxruntime-core=={ort_ver}; platform_system != "Linux" """.rstrip(),
            f"""onnxruntime-genai-cuda=={genai_ver}; platform_system == "Linux" """.rstrip(),
            f"""onnxruntime-genai-core=={genai_ver}; platform_system != "Linux" """.rstrip(),
        ]
    return f"{base}\n" + "\n".join(requirement_lines) + "\n"


# ---------------------------------------------------------------------------
# Variant detection
# ---------------------------------------------------------------------------


def _is_winml(config_settings: dict | None) -> bool:
    """Return True when the WinML variant should be built.

    Checks ``config_settings["winml"]`` first (set via ``-C winml=true``),
    then falls back to the ``FOUNDRY_VARIANT`` environment variable.
    """
    if config_settings and str(config_settings.get("winml", "")).lower() == "true":
        return True
    return os.environ.get("FOUNDRY_VARIANT", "").lower() == "winml"


# ---------------------------------------------------------------------------
# In-place patching context manager
# ---------------------------------------------------------------------------


@contextlib.contextmanager
def _patch_for_winml() -> Generator[None, None, None]:
    """Temporarily patch ``pyproject.toml`` and generate ``requirements.txt`` for WinML.

    ``pyproject.toml`` is restored in the ``finally`` block.
    ``requirements.txt`` is left in place (generated from deps_versions.json).
    """
    pyproject_original = _PYPROJECT.read_text(encoding="utf-8")
    try:
        # Patch package name (simple string replacement — no TOML writer needed)
        patched_pyproject = pyproject_original.replace(_STANDARD_NAME, _WINML_NAME, 1)
        if patched_pyproject == pyproject_original:
            raise RuntimeError(
                f"Could not find {_STANDARD_NAME!r} in pyproject.toml — "
                "WinML name patch failed."
            )
        _PYPROJECT.write_text(patched_pyproject, encoding="utf-8")
        _REQUIREMENTS.write_text(_generate_requirements(winml=True), encoding="utf-8")
        yield
    finally:
        _PYPROJECT.write_text(pyproject_original, encoding="utf-8")


@contextlib.contextmanager
def _patch_standard_deps() -> Generator[None, None, None]:
    """Generate ``requirements.txt`` from base deps + ``deps_versions.json``."""
    _REQUIREMENTS.write_text(_generate_requirements(winml=False), encoding="utf-8")
    yield


def _apply_patches(config_settings: dict | None):
    """Return a context manager that applies the appropriate patches."""
    if _is_winml(config_settings):
        return _patch_for_winml()
    return _patch_standard_deps()


# ---------------------------------------------------------------------------
# PEP 517 hook delegation
# ---------------------------------------------------------------------------


def get_requires_for_build_wheel(config_settings=None):
    with _apply_patches(config_settings):
        return _sb.get_requires_for_build_wheel(config_settings)


def prepare_metadata_for_build_wheel(metadata_directory, config_settings=None):
    with _apply_patches(config_settings):
        return _sb.prepare_metadata_for_build_wheel(metadata_directory, config_settings)


def build_wheel(wheel_directory, config_settings=None, metadata_directory=None):
    with _apply_patches(config_settings):
        return _sb.build_wheel(wheel_directory, config_settings, metadata_directory)


def get_requires_for_build_editable(config_settings=None):
    with _apply_patches(config_settings):
        return _sb.get_requires_for_build_editable(config_settings)


def prepare_metadata_for_build_editable(metadata_directory, config_settings=None):
    with _apply_patches(config_settings):
        return _sb.prepare_metadata_for_build_editable(metadata_directory, config_settings)


def build_editable(wheel_directory, config_settings=None, metadata_directory=None):
    with _apply_patches(config_settings):
        return _sb.build_editable(wheel_directory, config_settings, metadata_directory)


def get_requires_for_build_sdist(config_settings=None):
    with _apply_patches(config_settings):
        return _sb.get_requires_for_build_sdist(config_settings)


def build_sdist(sdist_directory, config_settings=None):
    with _apply_patches(config_settings):
        return _sb.build_sdist(sdist_directory, config_settings)
