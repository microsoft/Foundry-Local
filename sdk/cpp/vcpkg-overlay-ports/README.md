# vcpkg Overlay Ports

This directory contains custom vcpkg overlay ports used **only in CI environments** to work around network restrictions on build agents.

## Why this exists

The CI build agents have restricted outbound network access. The upstream `nlohmann-json` vcpkg port calls `vcpkg_fixup_pkgconfig()`, which attempts to download the `pkgconf` tool from msys2 mirrors at build time. Since CI agents cannot reach these mirrors, the build fails with:

```
error: curl operation failed with error code 7 (Could not connect to server).
error: Not a transient network error, won't retry download from https://mirror.msys2.org/...
```

## What the overlay does

The `nlohmann-json` overlay port is identical to the upstream port (v3.12.0, port-version 2) with one change: the `vcpkg_fixup_pkgconfig()` call is removed. This is safe because:

- nlohmann-json is a **header-only** library
- All consumers use **CMake** (`find_package`), not pkg-config
- The `.pc` file is still installed; it simply isn't rewritten with vcpkg-specific paths

## How to use (CI only)

Set the `VCPKG_OVERLAY_PORTS` environment variable before running CMake configure:

```powershell
# In your CI pipeline script
$env:VCPKG_OVERLAY_PORTS = "$repoRoot/sdk/cpp/vcpkg-overlay-ports"
cmake --preset x64-release
```

Or pass it as a CMake cache variable:

```powershell
cmake --preset x64-release -DVCPKG_OVERLAY_PORTS="$repoRoot/sdk/cpp/vcpkg-overlay-ports"
```

## Local development

Local developers with unrestricted internet access do **not** need this overlay. Without `VCPKG_OVERLAY_PORTS` set, vcpkg uses the standard upstream port as normal.

## Maintenance

If you update the vcpkg baseline and the upstream nlohmann-json port changes, you may need to update this overlay to match. Check if the upstream port still calls `vcpkg_fixup_pkgconfig()` — if it no longer does, this overlay can be removed.
