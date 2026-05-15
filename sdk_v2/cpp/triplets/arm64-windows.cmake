# Overlay triplet for arm64-windows (cross-compile from x64 host).
# Vcpkg dependencies are statically linked so they are absorbed into
# foundry_local.dll and we don't have to ship a forest of transitive DLLs
# (azure-*, spdlog, fmt, openssl, curl, zlib, brotli*) alongside it.
# The CRT stays dynamic so every binary that loads foundry_local.dll shares
# the same MSVCRT instance.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# Allow PKG_CONFIG env var into vcpkg's clean build environment.
# OneBranch CI containers block outbound connections to MSYS2 mirrors,
# so we provide a stub pkg-config and point PKG_CONFIG to it.
list(APPEND VCPKG_ENV_PASSTHROUGH_UNTRACKED PKG_CONFIG)
