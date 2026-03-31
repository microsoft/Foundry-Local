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

Skip native deps (use pre-installed foundry-local-core / ORT / GenAI)::

    python -m build --wheel -C skip-native-deps=true

Environment variable fallback (useful in CI pipelines)::

    FOUNDRY_VARIANT=winml python -m build --wheel
    FOUNDRY_SKIP_NATIVE_DEPS=1 python -m build --wheel
"""

from __future__ import annotations

import contextlib
import os
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

# The exact string in pyproject.toml to patch for the WinML variant.
_STANDARD_NAME = 'name = "foundry-local-sdk"'
_WINML_NAME = 'name = "foundry-local-sdk-winml"'

# Native binary package prefixes to strip when skip-native-deps is active.
_NATIVE_DEP_PREFIXES = (
    "foundry-local-core",
    "onnxruntime-core",
    "onnxruntime-genai-core",
)


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


def _is_skip_native_deps(config_settings: dict | None) -> bool:
    """Return True when native binary dependencies should be omitted.

    When set, ``foundry-local-core``, ``onnxruntime-core``, and
    ``onnxruntime-genai-core`` are stripped from requirements.txt so the
    wheel is built against whatever versions are already installed.
    Useful in CI pipelines that pre-install pipeline-built native wheels.

    Checks ``config_settings["skip-native-deps"]`` first
    (set via ``-C skip-native-deps=true``), then falls back to the
    ``FOUNDRY_SKIP_NATIVE_DEPS`` environment variable.
    """
    if config_settings and str(config_settings.get("skip-native-deps", "")).lower() == "true":
        return True
    return os.environ.get("FOUNDRY_SKIP_NATIVE_DEPS", "").lower() in ("1", "true")


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

        # Swap requirements.txt with the WinML variant
        shutil.copy2(_REQUIREMENTS_WINML, _REQUIREMENTS)

        yield
    finally:
        _PYPROJECT.write_text(pyproject_original, encoding="utf-8")
        _REQUIREMENTS.write_text(requirements_original, encoding="utf-8")


@contextlib.contextmanager
def _strip_native_deps() -> Generator[None, None, None]:
    """Temporarily remove native binary deps from requirements.txt.

    Lines starting with any prefix in ``_NATIVE_DEP_PREFIXES`` (case-
    insensitive) are removed.  The file is restored in the ``finally``
    block.
    """
    requirements_original = _REQUIREMENTS.read_text(encoding="utf-8")
    try:
        filtered = [
            line for line in requirements_original.splitlines(keepends=True)
            if not any(line.lstrip().lower().startswith(p) for p in _NATIVE_DEP_PREFIXES)
        ]
        _REQUIREMENTS.write_text("".join(filtered), encoding="utf-8")
        yield
    finally:
        _REQUIREMENTS.write_text(requirements_original, encoding="utf-8")


def _apply_patches(config_settings: dict | None):
    """Return a context manager that applies the appropriate patches."""
    winml = _is_winml(config_settings)
    skip_native = _is_skip_native_deps(config_settings)

    @contextlib.contextmanager
    def _combined():
        # Stack contexts: WinML swaps requirements first, then strip_native
        # removes native deps from whatever requirements are active.
        if winml and skip_native:
            with _patch_for_winml(), _strip_native_deps():
                yield
        elif winml:
            with _patch_for_winml():
                yield
        elif skip_native:
            with _strip_native_deps():
                yield
        else:
            yield

    return _combined()


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
