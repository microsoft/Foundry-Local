# Telemetry dependency policy

The C++ manifest pins `cpp-client-telemetry` to **3.10.281.1** and uses the
vcpkg baseline containing [microsoft/vcpkg#54342](https://github.com/microsoft/vcpkg/pull/54342).
The Foundry overlay downloads that same release archive with the registry's
SHA512; it never substitutes a local telemetry source checkout.

The overlay remains necessary: the merged registry recipe does not expose
logging, device-ID, minimal SQLite, or Android curl controls. Foundry selects
`minimal-sqlite`, `curl-openssl`, and `no-logging`, disables default features,
and omits `device-id`. Internal SDK diagnostics and native device-ID collection
are therefore compiled out, while telemetry event collection remains available.
Features are additive: another dependency selecting `device-id` would re-enable
native collection and must not be added to Foundry's dependency graph.

| Platform | HTTP transport | SQLite | zlib |
| --- | --- | --- | --- |
| Windows | OS WinHTTP | Private minimal SQLite | vcpkg |
| Linux | vcpkg curl with OpenSSL | Private minimal SQLite | vcpkg |
| macOS / iOS | OS Apple HTTP | OS SQLite | OS zlib |
| Android | vcpkg curl with OpenSSL | Private minimal SQLite | vcpkg |

`SYSTEM` in the telemetry CMake API normally means dependency discovery, not
necessarily an OS library. In particular, 3.10.281.1 bypasses its Apple system
library helpers when a vcpkg toolchain is active. The overlay applies
`apple-system-libraries.patch` so Apple builds and installed static-package
exports use OS `sqlite3` and `z`, without undeclared vcpkg dependencies.
Both macOS and iOS provide these libraries; iOS uses a deployment target of 13.0,
matching the release registry recipe.

Windows does not provide the required public SQLite/zlib development libraries.
Android's private platform SQLite is not a public NDK dependency. Linux has
distro SQLite/zlib packages, but Foundry deliberately keeps private minimal
SQLite and reuses its existing vcpkg zlib/curl dependencies for reproducibility.
The overlay builds static telemetry with exception support retained, disables
optional proprietary modules and wrappers, and disables Android Room and the
alternative C API HTTP client.

This follows ONNX Runtime and GenAI's source-embedded telemetry policy for
internal diagnostics, WinHTTP, minimal SQLite, and Apple system libraries.
Intentional differences are OpenSSL-backed vcpkg curl instead of fetched
mbedTLS curl on Linux, native curl instead of Java/JNI HTTP on Android, reuse
of vcpkg zlib instead of a second vendored copy on non-Apple platforms, and
disabled native device-ID collection on mobile as well as desktop.

Run `python sdk_v2/cpp/tools/test_telemetry_port.py` from the repository root
to check feature mapping, platform policy, and manifest consistency without
downloading dependencies. These checks do not replace native package builds
and installed-package consumer tests.
