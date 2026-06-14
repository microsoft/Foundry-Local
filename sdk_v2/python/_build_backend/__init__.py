# PEP 517 backend shim for foundry-local-sdk.
#
# Delegates every hook to ``setuptools.build_meta``. The only added
# behavior is a temporary patch of ``pyproject.toml`` when the
# ``FL_PYTHON_PACKAGE_NAME`` environment variable is set, so the same
# source tree can produce two differently-named wheels:
#
#   * unset                    -> foundry-local-sdk           (default)
#   * foundry-local-sdk-winml  -> foundry-local-sdk-winml     (WinML SKU)
#
# This is wired into pyproject.toml via::
#
#   [build-system]
#   build-backend = "_build_backend"
#   backend-path  = ["."]
#
# Local dev workflow is unchanged: without the env var, ``python -m build``
# produces ``foundry-local-sdk`` exactly as before.

from __future__ import annotations

import contextlib
import json
import os
import re
from collections.abc import Generator
from pathlib import Path

# Re-export every PEP 517 hook from setuptools so any future additions are
# picked up automatically; only the wheel/metadata hooks need wrapping.
from setuptools.build_meta import *  # noqa: F401,F403
from setuptools.build_meta import (
    build_sdist as _orig_build_sdist,
    build_wheel as _orig_build_wheel,
    get_requires_for_build_sdist as _orig_get_requires_for_build_sdist,
    get_requires_for_build_wheel as _orig_get_requires_for_build_wheel,
    prepare_metadata_for_build_wheel as _orig_prepare_metadata_for_build_wheel,
)

try:  # editable hooks are optional in older setuptools versions.
    from setuptools.build_meta import (
        build_editable as _orig_build_editable,
        get_requires_for_build_editable as _orig_get_requires_for_build_editable,
        prepare_metadata_for_build_editable as _orig_prepare_metadata_for_build_editable,
    )

    _HAS_EDITABLE = True
except ImportError:  # pragma: no cover - newer setuptools only
    _HAS_EDITABLE = False


_ENV_VAR = "FL_PYTHON_PACKAGE_NAME"
_PYPROJECT = Path(__file__).resolve().parent.parent / "pyproject.toml"
_SDK_V2_ROOT = _PYPROJECT.resolve().parent.parent
_DEPS_JSON_STD = _SDK_V2_ROOT / "deps_versions.json"

# Match ``name = "..."`` only inside the [project] table. The regex is
# anchored to the first ``name = "foundry-local-sdk"`` occurrence which
# is the project name (the build backend itself is referenced by module
# name, not by a quoted package name, so there's no ambiguity).
_NAME_PATTERN = re.compile(r'(?m)^name\s*=\s*"foundry-local-sdk"')

# Patterns for rewriting ORT/GenAI version pins in the dependencies list.
# Each captures the package name + ``==`` and we substitute in the version
# read from the appropriate deps_versions JSON.
_ORT_PIN_PATTERN = re.compile(r'("onnxruntime(?:-core|-gpu)==)[^\s";]+')
_GENAI_PIN_PATTERN = re.compile(r'("onnxruntime-genai(?:-core|-cuda)==)[^\s";]+')


def _read_versions(deps_file: Path) -> tuple[str, str]:
    if not deps_file.is_file():
        raise RuntimeError(f"Required versions file not found: {deps_file}")
    data = json.loads(deps_file.read_text(encoding="utf-8"))
    try:
        ort = data["onnxruntime"]["version"]
        genai = data["onnxruntime-genai"]["version"]
    except (KeyError, TypeError) as exc:
        raise RuntimeError(
            f"{deps_file} is missing required keys 'onnxruntime.version' / 'onnxruntime-genai.version'"
        ) from exc
    return str(ort), str(genai)


def _patch_pyproject_text(original: str, *, override_name: str | None, deps_file: Path) -> str:
    """Return *original* with the project name and ORT/GenAI version pins rewritten.

    * ``override_name`` — when set, replaces ``name = "foundry-local-sdk"``.
    * ``deps_file`` — JSON file that supplies the ORT and GenAI version pins.
    """
    patched = original
    if override_name and override_name != "foundry-local-sdk":
        patched, n = _NAME_PATTERN.subn(f'name = "{override_name}"', patched, count=1)
        if n != 1:
            raise RuntimeError(
                f"Could not find canonical project name line in {_PYPROJECT} to patch "
                f"for {_ENV_VAR}={override_name!r}."
            )

    ort_ver, genai_ver = _read_versions(deps_file)
    patched = _ORT_PIN_PATTERN.sub(lambda m: f"{m.group(1)}{ort_ver}", patched)
    patched = _GENAI_PIN_PATTERN.sub(lambda m: f"{m.group(1)}{genai_ver}", patched)
    return patched


@contextlib.contextmanager
def _maybe_patch_name() -> Generator[None, None, None]:
    """Context manager that rewrites pyproject.toml during PEP 517 hook execution.

    Always rewrites ORT/GenAI version pins from deps_versions.json (single
    source of truth). Conditionally rewrites the project name when
    ``FL_PYTHON_PACKAGE_NAME`` selects the WinML variant. The WinML and
    standard flavors share the same ORT/GenAI versions; only the
    distribution name differs.
    """
    override = os.environ.get(_ENV_VAR, "").strip() or None

    original = _PYPROJECT.read_text(encoding="utf-8")
    patched = _patch_pyproject_text(original, override_name=override, deps_file=_DEPS_JSON_STD)

    if patched == original:
        # Nothing to rewrite (e.g. JSON already matches and no name override).
        yield
        return

    try:
        _PYPROJECT.write_text(patched, encoding="utf-8")
        yield
    finally:
        _PYPROJECT.write_text(original, encoding="utf-8")


def get_requires_for_build_wheel(config_settings=None):
    with _maybe_patch_name():
        return _orig_get_requires_for_build_wheel(config_settings)


def prepare_metadata_for_build_wheel(metadata_directory, config_settings=None):
    with _maybe_patch_name():
        return _orig_prepare_metadata_for_build_wheel(metadata_directory, config_settings)


def build_wheel(wheel_directory, config_settings=None, metadata_directory=None):
    with _maybe_patch_name():
        return _orig_build_wheel(wheel_directory, config_settings, metadata_directory)


def get_requires_for_build_sdist(config_settings=None):
    with _maybe_patch_name():
        return _orig_get_requires_for_build_sdist(config_settings)


def build_sdist(sdist_directory, config_settings=None):
    with _maybe_patch_name():
        return _orig_build_sdist(sdist_directory, config_settings)


if _HAS_EDITABLE:

    def get_requires_for_build_editable(config_settings=None):
        with _maybe_patch_name():
            return _orig_get_requires_for_build_editable(config_settings)

    def prepare_metadata_for_build_editable(metadata_directory, config_settings=None):
        with _maybe_patch_name():
            return _orig_prepare_metadata_for_build_editable(metadata_directory, config_settings)

    def build_editable(wheel_directory, config_settings=None, metadata_directory=None):
        with _maybe_patch_name():
            return _orig_build_editable(wheel_directory, config_settings, metadata_directory)
