# Overlay triplet for x64-windows.
# Matches vcpkg's default x64-windows settings, plus env passthrough
# needed for CI builds where MSYS2 mirrors are blocked.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

# Allow PKG_CONFIG env var into vcpkg's clean build environment.
# OneBranch CI containers block outbound connections to MSYS2 mirrors,
# so we provide a stub pkg-config and point PKG_CONFIG to it.
list(APPEND VCPKG_ENV_PASSTHROUGH_UNTRACKED PKG_CONFIG)
