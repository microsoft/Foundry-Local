set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# Skip pkgconfig fixup to avoid downloading MSYS2 packages, which may be
# unreachable from restricted CI environments.
set(VCPKG_POLICY_SKIP_PKGCONFIG_CHECK enabled)
