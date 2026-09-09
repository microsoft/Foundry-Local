# Windows JVM / C++ runtime compatibility

## Reproduced cause

The pinned `onnxruntime.dll` fails in consumer JBR 21.0.8+1-b895.146 with
**Win32 error 1114 (`ERROR_DLL_INIT_FAILED`)**, not 126 (module missing) or
127 (export missing). Its SHA-256 is unchanged:
`18370c375f07357fa5874344a9d9ac17e6b6fe1eb18b1dd209d79483b4470257`.

Actual module enumeration shows that the JVM has already loaded its bundled
`msvcp140.dll`, `vcruntime140.dll` and `vcruntime140_1.dll`, version
**14.29.30153.0**, before the ORT load begins. This is not a PATH-only inference.

A controlled fresh non-JVM host initially had no loaded `msvcp140.dll`.
Preloading the existing JBR CRT files and then loading the unchanged ORT bytes
reproduced 1114. A non-handling vectored exception observer captured a
**null-read access violation in that `msvcp140.dll`, RVA `0x13080`** during DLL
initialization. All **187 directly imported CRT symbols** resolved; the old
CRT is present but behaviorally incompatible with this native initialization.
Using the existing Temurin CRT 14.40.33810.0 or installed system CRT
14.50.35719.0 in fresh controlled hosts made the identical ORT load succeed,
without an observed access violation.

This isolates the older CRT as causal independently of Java or JNA. It is
consistent with Microsoft's rule that the C++ Redistributable used by an app
must be at least as new as the newest toolset used by any component:
[Microsoft binary compatibility restrictions](https://learn.microsoft.com/en-us/cpp/porting/binary-compat-2015-2017).
The exact native toolset and the earliest compatible CRT minor version are
not attested here. **14.40 is a tested successful version, not a proven universal
minimum.** No unsupported native patch or CRT replacement was attempted.

## Falsified alternatives

| Existing JVM | Actually loaded bundled CRT | Load ORT, GenAI, Foundry |
|---|---|---|
| Temurin 17.0.20.1+1 | 14.40.33810.0 | All succeed |
| Microsoft OpenJDK 21.0.10+7-LTS | 14.44.35208.0 | All succeed |
| Consumer JBR 21.0.8+1-b895.146 | 14.29.30153.0 | ORT fails, 1114 |
| Separately installed JBR 21.0.9+1-b1038.78 | 14.29.30153.0 | ORT fails, 1114 |
| Separately installed JBR 25.0.4+1-b329.128 | 14.44.35211.0 | All succeed |

These are **load-only** comparisons, not new ASR results. The passing Microsoft
JDK 21 separates Java language/runtime major version from the packaging problem.
The newer failing JBR 21 patch shows that merely updating the Java patch number
does not fix the old bundled CRT. JBR 25 is a separate compatibility candidate;
it does not establish support for the original consumer baseline.

After this diagnosis, the coordinator explicitly authorized native ASR on the
separately installed official IU 2026.1.5 / JBR 25.0.4+1-b329.128 combination.
Targeted native lifecycle, WAV, paced headerless PCM and cancellation/cleanup
passed without skips; see [NATIVE_SMOKE.md](NATIVE_SMOKE.md). It is now the
alternate Windows POC compatibility candidate. Both failing JBR 21 rows remain
failures, and Microsoft OpenJDK 21 still has load-only evidence.

Consumer JBR 21 also failed with all three scoped `LoadLibraryExW` flag choices:
0 (default), 8 (`LOAD_WITH_ALTERED_SEARCH_PATH`), and 0x1100
(`LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS`).
The old CRT was already mapped in every case. Search-flag changes therefore
do not repair this conflict. A previous process-local system-CRT preload after
JVM startup also did not repair the consumer baseline.

## Reproduction without model loading

Compile ordinary test sources with the existing Maven build:

```powershell
.\sdk_v2\java\build.ps1 -MavenArguments @('--offline', 'test-compile')
$cp = 'sdk_v2\java\target\test-classes;sdk_v2\java\target\classes;sdk_v2\java\target\lib\jna-5.17.0.jar'
& $Java -cp $cp com.microsoft.foundry.local.WindowsLoaderProbe sdk_v2\java\target\runtime 0
```

`$Java` must name the explicit JVM executable being compared. Run from the
repository root with the same prepared runtime directory and environment.
The probe verifies the bundled native hashes, reports JVM identity, enumerates
actual loaded module paths before/after each load, invokes `LoadLibraryExW`
once per native library, and records the immediately captured Win32 error.
Success requires `loadOnlySucceeded` and exit 0; failure exits 1. It never
constructs a manager, queries the catalog, loads a model, or runs inference.
Redirect diagnostic paths to ignored local evidence; do not publish workstation
paths. Give each subprocess a finite timeout.

## Deployment choices, not automatic fallbacks

For a standalone consumer, the already installed Microsoft OpenJDK 21 is a
load-only-passing alternative; Temurin 17 already has native ASR evidence.
For an embedded JetBrains consumer, use an officially supported JBR/IDE
distribution carrying a compatible CRT, subject to the consumer's own JVM
requirements. The separately installed JBR 25 passed the subsequently authorized
targeted native-ASR smoke, but switching an existing application's JVM must still
be an explicit supported deployment decision. No extra JVM is bundled or launched
by the SDK.

Do not overwrite IDE/JBR DLLs, unload or replace an in-use CRT, or assume a
system-wide Redistributable update will override a JVM's already loaded app-local
CRT. A native package rebuilt for an older CRT would be a different package/
header/binary lock requiring upstream support, license review and revalidation;
it was not substituted here. Official JVM or Redistributable redistribution
has its own license/notice and servicing obligations, separate from this SDK.
