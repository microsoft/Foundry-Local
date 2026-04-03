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
import re
import shutil
from collections.abc import Generator
from pathlib import Path

import setuptools.build_meta as _sb

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

_PROJECT_ROOT = Path(__file__).parent
_PYPROJECT = _PROJECT_ROOT / "pyproject.toml"
_REQUIREMENTS = _PROJECT_ROOT / "requirements.txt"
_REQUIREMENTS_WINML = _PROJECT_ROOT / "requirements-winml.txt"
_FLC_VERSION_INFO = _PROJECT_ROOT / ".." / ".." / "FLC_VERSION_INFO.json"

# The exact string in pyproject.toml to patch for the WinML variant.
_STANDARD_NAME = 'name = "foundry-local-sdk"'
_WINML_NAME = 'name = "foundry-local-sdk-winml"'


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
# FLC version resolution from FLC_VERSION_INFO.json
# ---------------------------------------------------------------------------


def _read_flc_versions() -> dict[str, str] | None:
    """Read FLC_VERSION_INFO.json and return the version mapping, or None."""
    try:
        return json.loads(_FLC_VERSION_INFO.read_text(encoding="utf-8"))
    except (FileNotFoundError, json.JSONDecodeError):
        return None


def _patch_flc_version_in_requirements(req_text: str, is_winml: bool) -> str:
    """Replace the foundry-local-core version in requirements text with the
    version from FLC_VERSION_INFO.json."""
    versions = _read_flc_versions()
    if versions is None:
        return req_text

    pkg = "foundry-local-core-winml" if is_winml else "foundry-local-core"
    version = versions.get(pkg)
    if not version:
        return req_text

    return re.sub(
        rf"^{re.escape(pkg)}==.+$",
        f"{pkg}=={version}",
        req_text,
        count=1,
        flags=re.MULTILINE,
    )


# ---------------------------------------------------------------------------
# In-place patching context manager
# ---------------------------------------------------------------------------


@contextlib.contextmanager
def _patch_for_winml() -> Generator[None, None, None]:
    """Temporarily patch ``pyproject.toml`` and ``requirements.txt`` for WinML.

    Both files are restored to their original content in the ``finally``
    block, even if the build raises an exception.
    """
    pyproject_original = _PYPROJECT.read_text(encoding="utf-8")
    requirements_original = _REQUIREMENTS.read_text(encoding="utf-8")
    try:
        # Patch package name (simple string replacement — no TOML writer needed)
        patched_pyproject = pyproject_original.replace(_STANDARD_NAME, _WINML_NAME, 1)
        if patched_pyproject == pyproject_original:
            raise RuntimeError(
                f"Could not find {_STANDARD_NAME!r} in pyproject.toml — "
                "WinML name patch failed."
            )
        _PYPROJECT.write_text(patched_pyproject, encoding="utf-8")

        # Swap requirements.txt with the WinML variant, then patch FLC version
        winml_reqs = _REQUIREMENTS_WINML.read_text(encoding="utf-8")
        _REQUIREMENTS.write_text(
            _patch_flc_version_in_requirements(winml_reqs, is_winml=True),
            encoding="utf-8",
        )

        yield
    finally:
        _PYPROJECT.write_text(pyproject_original, encoding="utf-8")
        _REQUIREMENTS.write_text(requirements_original, encoding="utf-8")


@contextlib.contextmanager
def _patch_flc_version_standard() -> Generator[None, None, None]:
    """Temporarily patch the FLC version in requirements.txt for standard builds."""
    requirements_original = _REQUIREMENTS.read_text(encoding="utf-8")
    try:
        _REQUIREMENTS.write_text(
            _patch_flc_version_in_requirements(requirements_original, is_winml=False),
            encoding="utf-8",
        )
        yield
    finally:
        _REQUIREMENTS.write_text(requirements_original, encoding="utf-8")


def _apply_patches(config_settings: dict | None):
    """Return a context manager that applies the appropriate patches."""
    if _is_winml(config_settings):
        return _patch_for_winml()
    return _patch_flc_version_standard()


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
    if _is_winml(config_settings):
        with _patch_for_winml():
            return _sb.build_sdist(sdist_directory, config_settings)
    return _sb.build_sdist(sdist_directory, config_settings)
