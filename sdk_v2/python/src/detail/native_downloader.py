# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
"""Native library downloader for Foundry Local SDK.

Downloads the native Microsoft.AI.Foundry.Local.Core libraries and their
dependencies (OnnxRuntime, OnnxRuntimeGenAI) from NuGet feeds.

Can be invoked as:
    foundry-local-install [--nightly] [--winml]

Or programmatically:
    from foundry_local_sdk.detail.native_downloader import download_native_binaries
    download_native_binaries()
    download_native_binaries(use_winml=True)  # for WinML binaries

Native binaries are also downloaded automatically on first SDK use,
so manual installation is only needed to pre-download for offline
scenarios or CI.
"""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path
from typing import Optional

import requests

# ---------------------------------------------------------------------------
# Platform / RID mapping
# ---------------------------------------------------------------------------

# Maps Python (sys.platform, platform.machine()) to NuGet Runtime Identifier
PLATFORM_MAP: dict[str, str] = {
    "win32-AMD64": "win-x64",
    "win32-ARM64": "win-arm64",
    "linux-x86_64": "linux-x64",
    "darwin-arm64": "osx-arm64",
}

# Maps Python sys.platform to native shared library extension
EXT_MAP: dict[str, str] = {
    "win32": ".dll",
    "linux": ".so",
    "darwin": ".dylib",
}


def _get_process_arch() -> str:
    """Return the architecture of the running Python process.

    On Windows, ``platform.machine()`` prefers the ``PROCESSOR_ARCHITEW6432``
    environment variable, which contains the *hardware* architecture even when
    an x64 Python process is running under ARM64 emulation.  We use
    ``PROCESSOR_ARCHITECTURE`` instead, which always reflects the process.
    """
    if sys.platform == "win32":
        return os.environ.get("PROCESSOR_ARCHITECTURE", platform.machine())
    return platform.machine()


def _get_platform_key() -> str:
    """Get the current platform key (e.g. 'win32-AMD64').

    Uses the Python **process** architecture (not the underlying hardware)
    because ctypes can only load libraries that match the process bitness
    (e.g. x64 Python needs x64 DLLs, even on ARM64 hardware).

    To get native ARM64 performance, install an ARM64 Python interpreter.
    """
    return f"{sys.platform}-{_get_process_arch()}"


def _get_rid() -> str | None:
    """Get the NuGet Runtime Identifier for the current platform."""
    return PLATFORM_MAP.get(_get_platform_key())


def _get_ext() -> str:
    """Get the native library file extension for the current platform."""
    for plat_prefix, ext in EXT_MAP.items():
        if sys.platform.startswith(plat_prefix):
            return ext
    raise RuntimeError(f"Unsupported platform: {sys.platform}")


def _get_project_root() -> Path:
    """Get the Python SDK project root (sdk_v2/python/)."""
    # __file__ is src/detail/native_downloader.py
    return Path(__file__).resolve().parent.parent.parent


def _get_native_dir() -> Path:
    """Get the directory where native binaries should be stored.

    Binaries are placed under ``packages/{platform-key}/`` relative to the
    Python SDK project root (e.g. ``sdk_v2/python/packages/win32-ARM64/``).
    """
    return _get_project_root() / "packages" / _get_platform_key()


def _get_required_files() -> list[str]:
    """Get the list of required native library files."""
    ext = _get_ext()
    # On Linux/macOS the ORT libraries are shipped with a "lib" prefix
    ort_prefix = "" if sys.platform == "win32" else "lib"
    return [
        f"Microsoft.AI.Foundry.Local.Core{ext}",
        f"{ort_prefix}onnxruntime{ext}",
        f"{ort_prefix}onnxruntime-genai{ext}",
    ]


# ---------------------------------------------------------------------------
# Artifact definitions
#
# To update a package version, just change the value here.
# ---------------------------------------------------------------------------

NUGET_FEED = "https://api.nuget.org/v3/index.json"
ORT_NIGHTLY_FEED = "https://pkgs.dev.azure.com/aiinfra/PublicPackages/_packaging/ORT-Nightly/nuget/v3/index.json"


def _get_artifacts(use_winml: bool = False) -> list[dict[str, str]]:
    """Build the list of NuGet packages to download.

    Args:
        use_winml: When True, download WinML-specific packages instead of
            the default Foundry packages.  WinML packages use the DirectML
            execution provider and are only supported on Windows.

    Returns:
        List of artifact dicts with 'name', 'version', and 'feed' keys.
    """
    linux = sys.platform.startswith("linux")

    if use_winml:
        return [
            {
                "name": "Microsoft.AI.Foundry.Local.Core.WinML",
                "version": "0.9.0.6-rc2",
                "feed": ORT_NIGHTLY_FEED,
            },
            {
                "name": "Microsoft.ML.OnnxRuntime.Foundry",
                "version": "1.23.2.3",
                "feed": NUGET_FEED,
            },
            {
                "name": "Microsoft.ML.OnnxRuntimeGenAI.WinML",
                "version": "0.12.1",
                "feed": NUGET_FEED,
            },
        ]

    return [
        {
            "name": "Microsoft.AI.Foundry.Local.Core",
            "version": "0.9.0.6-rc2",
            "feed": ORT_NIGHTLY_FEED,
        },
        {
            "name": "Microsoft.ML.OnnxRuntime.Foundry" if not linux else "Microsoft.ML.OnnxRuntime.Gpu.Linux",
            "version": "1.24.1" if linux else "1.24.1.1",
            "feed": NUGET_FEED,
        },
        {
            "name": "Microsoft.ML.OnnxRuntimeGenAI.Foundry",
            "version": "0.12.1",
            "feed": NUGET_FEED,
        },
    ]


# ---------------------------------------------------------------------------
# NuGet V3 API helpers
# ---------------------------------------------------------------------------

# Modified from js sdk's approach to downloading nuget files via http
_service_index_cache: dict[str, dict] = {}


def _get_base_address(feed_url: str) -> str:
    """Get the PackageBaseAddress from a NuGet V3 service index.

    Args:
        feed_url: The NuGet V3 feed service index URL.

    Returns:
        The base address URL for package content.
    """
    if feed_url not in _service_index_cache:
        resp = requests.get(feed_url, timeout=30)
        resp.raise_for_status()
        _service_index_cache[feed_url] = resp.json()

    service_index = _service_index_cache[feed_url]
    resources = service_index.get("resources", [])

    for r in resources:
        rtype = r.get("@type", "")
        if rtype.startswith("PackageBaseAddress/3.0.0"):
            base = r["@id"]
            return base if base.endswith("/") else base + "/"

    raise RuntimeError(f"Could not find PackageBaseAddress/3.0.0 in NuGet feed: {feed_url}")


def _resolve_latest_version(feed_url: str, package_name: str) -> str:
    """Resolve the latest version of a package from a NuGet feed.

    Used for nightly builds where the version is not pinned.

    Args:
        feed_url: NuGet V3 feed URL.
        package_name: Package name to look up.

    Returns:
        The latest version string.
    """
    base_address = _get_base_address(feed_url)
    name_lower = package_name.lower()
    versions_url = f"{base_address}{name_lower}/index.json"

    resp = requests.get(versions_url, timeout=30)
    resp.raise_for_status()
    data = resp.json()

    versions = data.get("versions", [])
    if not versions:
        raise RuntimeError(f"No versions found for {package_name} at {versions_url}")

    # Sort descending — lexicographic sort picks latest date-stamped dev versions
    versions.sort(reverse=True)
    latest = versions[0]
    print(f"  Resolved latest version: {latest}")
    return latest


def _resolve_nupkg_url(feed_url: str, package_name: str, version: str) -> str:
    """Construct the direct download URL for a NuGet package.

    Args:
        feed_url: NuGet V3 feed URL.
        package_name: Package name.
        version: Package version.

    Returns:
        Direct URL to the .nupkg file.
    """
    base_address = _get_base_address(feed_url)
    name_lower = package_name.lower()
    ver_lower = version.lower()
    return f"{base_address}{name_lower}/{ver_lower}/{name_lower}.{ver_lower}.nupkg"


def _download_file(url: str, dest: Path):
    """Download a file from a URL, following redirects.

    Args:
        url: URL to download.
        dest: Destination file path.
    """
    resp = requests.get(url, stream=True, timeout=120, allow_redirects=True)
    resp.raise_for_status()

    with open(dest, "wb") as f:
        for chunk in resp.iter_content(chunk_size=8192):
            f.write(chunk)


def _extract_native_binaries(nupkg_path: Path, rid: str, ext: str, bin_dir: Path) -> list[str]:
    """Extract native binaries from a .nupkg (ZIP) file.

    Extracts files matching: runtimes/{rid}/native/*.{ext}

    Args:
        nupkg_path: Path to the .nupkg file.
        rid: NuGet Runtime Identifier (e.g. 'win-x64').
        ext: File extension to look for (e.g. '.dll').
        bin_dir: Directory to extract files into.

    Returns:
        List of extracted file names.
    """
    target_prefix = f"runtimes/{rid}/native/".lower()
    extracted = []

    with zipfile.ZipFile(nupkg_path, "r") as zf:
        for entry in zf.namelist():
            entry_lower = entry.lower()
            if entry_lower.startswith(target_prefix) and entry_lower.endswith(ext):
                # Extract just the filename (flat, no directory structure)
                filename = Path(entry).name
                target_path = bin_dir / filename
                with zf.open(entry) as src, open(target_path, "wb") as dst:
                    shutil.copyfileobj(src, dst)
                extracted.append(filename)
                print(f"    Extracted {filename}")

    return extracted


def _create_ort_symlinks(bin_dir: Path):
    """Create OnnxRuntime symlinks on Linux/macOS.

    Workaround for ORT issue https://github.com/microsoft/onnxruntime/issues/27263.
    The native Core library expects 'onnxruntime.dll' but on Linux/macOS
    the actual file is named 'libonnxruntime.so/.dylib'.
    """
    if sys.platform == "win32":
        return

    ext = ".dylib" if sys.platform == "darwin" else ".so"
    lib_name = f"libonnxruntime{ext}"
    link_name = "onnxruntime.dll"
    lib_path = bin_dir / lib_name
    link_path = bin_dir / link_name

    if lib_path.exists() and not link_path.exists():
        os.symlink(lib_name, link_path)
        print(f"  Created symlink: {link_name} -> {lib_name}")


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------


def get_native_path() -> Path | None:
    """Check if native libraries are already downloaded and return their path.

    Returns:
        Path to the native libraries directory, or None if not found.
    """
    native_dir = _get_native_dir()
    required = _get_required_files()

    if native_dir.exists() and all((native_dir / f).exists() for f in required):
        return native_dir

    return None


def download_native_binaries(
    use_nightly: bool = False,
    target_dir: Path | None = None,
    use_winml: bool = False,
) -> Path:
    """Download native libraries from NuGet feeds.

    Args:
        use_nightly: Whether to use nightly builds.
        target_dir: Override target directory. Defaults to package-local packages/ dir.
        use_winml: Download WinML-specific packages (DirectML execution provider).
            Only supported on Windows.

    Returns:
        Path to the directory containing the downloaded binaries.

    Raises:
        RuntimeError: If the current platform is not supported or download fails.
    """
    if use_winml and sys.platform != "win32":
        raise RuntimeError("WinML packages are only supported on Windows.")
    rid = _get_rid()
    if not rid:
        raise RuntimeError(
            f"Unsupported platform: {_get_platform_key()}. "
            f"Supported platforms: {', '.join(PLATFORM_MAP.keys())}"
        )

    ext = _get_ext()
    bin_dir = target_dir or _get_native_dir()
    required = _get_required_files()

    # Check if already installed
    if bin_dir.exists() and all((bin_dir / f).exists() for f in required):
        if use_nightly:
            print("[foundry-local] Nightly requested. Forcing reinstall...")
            shutil.rmtree(bin_dir)
        else:
            print("[foundry-local] Native libraries already installed.")
            return bin_dir

    variant = "winml" if use_winml else "cross-plat"
    print(f"[foundry-local] Installing native libraries for {rid} ({variant})...")
    bin_dir.mkdir(parents=True, exist_ok=True)

    artifacts = _get_artifacts(use_winml=use_winml)
    with tempfile.TemporaryDirectory(prefix="foundry-install-") as temp_dir:
        temp_path = Path(temp_dir)
        for artifact in artifacts:
            if use_nightly and artifact["feed"] == ORT_NIGHTLY_FEED:
                artifact = {**artifact, "version": None}
            _install_package(artifact, rid, ext, bin_dir, temp_path)

    _create_ort_symlinks(bin_dir)

    # Verify required files
    missing = [f for f in required if not (bin_dir / f).exists()]
    if missing:
        raise RuntimeError(
            f"Installation incomplete. Missing files: {', '.join(missing)}. "
            f"Directory contents: {[f.name for f in bin_dir.iterdir()]}"
        )

    print("[foundry-local] Installation complete.")
    return bin_dir


def _install_package(
    artifact: dict[str, str | None],
    rid: str,
    ext: str,
    bin_dir: Path,
    temp_dir: Path,
):
    """Download and extract a single NuGet package.

    Args:
        artifact: Dict with 'name', 'version', 'feed' keys.
        rid: NuGet Runtime Identifier.
        ext: Library file extension.
        bin_dir: Directory to extract binaries into.
        temp_dir: Temporary directory for downloads.
    """
    pkg_name = artifact["name"]
    feed_url = artifact["feed"]
    pkg_ver = artifact.get("version")

    # Resolve version if not specified (nightly)
    if not pkg_ver:
        print(f"  Resolving latest version for {pkg_name}...")
        pkg_ver = _resolve_latest_version(feed_url, pkg_name)

    print(f"  Downloading {pkg_name} {pkg_ver}...")
    download_url = _resolve_nupkg_url(feed_url, pkg_name, pkg_ver)

    nupkg_path = temp_dir / f"{pkg_name}.{pkg_ver}.nupkg"
    _download_file(download_url, nupkg_path)

    print(f"  Extracting {pkg_name}...")
    extracted = _extract_native_binaries(nupkg_path, rid, ext, bin_dir)

    if not extracted:
        print(f"    Warning: No files found for RID {rid} in {pkg_name}")


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------


def main(args: list[str] | None = None):
    """CLI entry point for downloading native binaries.

    Usage:
        foundry-local-install [--nightly] [--winml]
    """
    parser = argparse.ArgumentParser(
        description=(
            "Download platform-specific native libraries (Foundry Local Core, "
            "OnnxRuntime, OnnxRuntimeGenAI) required by the Foundry Local SDK. "
            "This is optional — libraries are also downloaded automatically on first use."
        ),
        prog="foundry-local-install",
    )
    parser.add_argument(
        "--nightly",
        action="store_true",
        help="Download latest nightly build (resolves latest version from ORT-Nightly feed)",
    )
    parser.add_argument(
        "--target",
        type=str,
        default=None,
        help="Override target directory for native libraries",
    )
    parser.add_argument(
        "--winml",
        action="store_true",
        help="Download WinML-specific packages (DirectML execution provider, Windows only)",
    )

    parsed = parser.parse_args(args)

    target = Path(parsed.target) if parsed.target else None

    try:
        path = download_native_binaries(
            use_nightly=parsed.nightly,
            target_dir=target,
            use_winml=parsed.winml,
        )
        print(f"[foundry-local] Native libraries installed at: {path}")
    except Exception as e:
        print(f"[foundry-local] Installation failed: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
