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

The `vcpkg_check_features` call in the overlay's `portfile.cmake` maps
`device-id` to `MATSDK_ENABLE_DEVICE_ID`. That helper emits an explicit `OFF`
for an absent feature, not just an `ON` for a selected feature. Consequently,
`${FEATURE_OPTIONS}` passed to `vcpkg_cmake_configure` contains
`-DMATSDK_ENABLE_DEVICE_ID=OFF` and `-DMATSDK_DISABLE_LOGGING=ON` on every
Foundry platform. The SDK turns the former into the private compile definition
`MATSDK_DISABLE_DEVICE_ID`; these settings do not propagate to consumer code.

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

This patch is provider selection, not a functionality-disabling feature:
vcpkg's additive-feature policy does not prohibit it. However, applying it
upstream unconditionally would change the current vcpkg provider choice.
An explicit Apple OS-library provider would preserve that choice while making
the size-oriented policy available without a downstream patch.

ONNX Runtime already supplies Apple `SQLite3::SQLite3` and `ZLIB::ZLIB`
targets before adding telemetry, so it does not need this patch. GenAI's
normal non-vcpkg source build also uses the existing Apple helpers; a
caller-supplied telemetry package carries its own provider policy.
GenAI's Apple source fallback under a vcpkg toolchain would need the same
explicit OS targets or an upstream provider fix if they are not already defined.

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

## Whole-SDK size considerations

Apple's SDKs provide SQLite/zlib headers and link stubs, and the OS supplies
the runtime libraries. No Homebrew or separately shipped SQLite/zlib package
is needed for telemetry. This does not remove those packages from Foundry's
entire dependency graph: the root manifest requests zlib, and the registry
libarchive and curl ports also depend on it. Removing only the root dependency
would not eliminate bundled zlib.

Azure Core 1.16.4 provides curl and Windows WinHTTP transports, not an
Apple-native transport. Its non-Windows hashing implementation also requires
OpenSSL independently of curl; Azure Storage Common additionally depends on
OpenSSL. Switching curl's TLS backend alone would therefore not remove
OpenSSL and could add a second TLS implementation. Removing these Apple
dependencies requires a native custom HTTP transport and changes to the
Azure hashing providers, not just a different manifest feature.

Using OS zlib across the Apple dependency graph is a separate size
optimization. Linux system libraries can reduce the shipped binary too,
but introduce distro ABI/runtime requirements. Keep the existing reproducible
Linux OpenSSL policy unless that distribution trade-off is explicitly chosen.
Provider changes require native size and throughput measurements; smaller
packaged binaries do not by themselves establish faster execution.

Run `python sdk_v2/cpp/tools/test_telemetry_port.py` from the repository root
to check feature mapping, platform policy, and manifest consistency without
downloading dependencies. These checks do not replace native package builds
and installed-package consumer tests.

## Baseline compatibility

The advanced baseline's Azure Storage dependency uses nanoarrow IPC. Foundry's
nanoarrow overlay repairs its installed package's bare `flatccrt` link dependency
with the configuration-specific `nanoarrow::flatcc_runtime` imported target.
This avoids missing library errors on Unix, selects the matching Debug library
on Windows, and does not collide with Azure Storage's own `flatccrt` target.

The manifest retains the previous `libiconv` port, **1.18#3**. Port 1.19 adds
an unconditional `vcpkg-make` host dependency, whose Automake download is
unreachable from the CI agents. That dependency is resolved even on Linux
and macOS, where both libiconv ports use OS iconv and build no GNU library.
The compatibility pin avoids the unnecessary build-tool download without
changing those platforms' runtime provider; platforms that build GNU libiconv
retain the previously used version. The new baseline and telemetry 3.10.281.1
pin remain unchanged.
