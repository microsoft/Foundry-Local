# Foundry Local Java ASR POC

A narrow Java 17+ SDK for **in-process speech recognition** through JNA and the
Foundry Local C ABI. No Node runtime, local HTTP service, JNI shim, FFM requirement,
or inference reimplementation. This is a standalone proof of concept, not a
production release or a Maven Central publication.

**Five-target Java 17 smoke:** the exact binary and metadata pins below completed
real ASR on Windows x64/ARM64, Linux x64/ARM64 and macOS ARM64 in
[run 34411280765](https://github.com/jiec-msft/foundry-local/actions/runs/34411280765).
Each target ran ten WAV transcriptions, ten paced streams and one cancelled
stream with ordered cleanup. These reuse ten public utterances, not 100 unique
recordings, and do not establish product, UI/plugin or microphone readiness.
See [NATIVE_SMOKE.md](NATIVE_SMOKE.md#five-target-hosted-smoke) for exact evidence
and its retention limitations.

**Windows POC compatibility boundary:** native WAV/PCM/cancellation/lifecycle
smoke succeeds with Temurin 17.0.20.1+1 and the explicitly selected official
IU 2026.1.5 / JBR 25.0.4+1-b329.128 combination. The latter is a separately
authorized compatibility candidate, not a repair of the older consumer runtime.
Original JBR 21.0.8+1-b895.146 and JBR 21.0.9+1-b1038.78 **remain incompatible**:
their already loaded CRT 14.29 causes ORT DLL initialization to fail.
No JVM/system DLL was replaced and no fallback JVM is bundled or automatically
selected. Microsoft OpenJDK 21 has load-only evidence, not ASR evidence.
See [WINDOWS_LOADING.md](WINDOWS_LOADING.md) for the explicit matrix and failure
guidance, [NATIVE_SMOKE.md](NATIVE_SMOKE.md) for evidence scope, and
[API.md](API.md) for the API/JSONL contract. The universal minimum JBR/CRT version
is unknown; this JVM-specific boundary is distinct from the hosted Java 17 smoke.

**Binary provenance:** the UTF-8 JSONL and cancellation-publication fixes in
`d0946a0764d9cfa4b3d684940d6d5c66165427b8` have independent local native
regression and hosted five-target Java 17 smoke evidence. Its qualified JAR is
64,000 bytes, SHA-256
`bf644d3127afff912683731094821a8f6a751f003c284a9c15ddceaecebe0863`.
The executed external inventory metadata is separately pinned to
`22ebea63b07addb526a1792e0303ba2f572f444a`. This documentation revision changes
neither pin and is not a new binary build.

**Integrity evidence limitation:** the successful matrix artifacts omitted
`model-verification.json`. Reviewed executed code enforces all sixteen actual
raw file sizes/hashes, the complete manifest and total before transcription;
retained provenance and totals match. The new per-file observations cannot be
independently replayed, and earlier diagnostic rows are not substituted for them.

## Runtime compatibility is pinned, not inferred

External model integrity consumers must use the observed per-target inventory
contract in [MODEL_LOCK.md](MODEL_LOCK.md). The legacy Windows
`model-lock.json` stays unchanged; all five target inventories are now
independently observed, including the non-Windows raw generated marker.
Inventory acceptance alone is not ASR qualification. The separately linked
matrix supplies actual ASR evidence; unknown or unobserved entries still fail closed.
Neither that metadata nor this documentation revision relabels the qualified
`d0946a0` binary.

The source-tree baseline is
`microsoft/foundry-local@afdb275c0b79f77dbbd1c901de235bfea124441d`.
Its C header uses API version 2. **Do not use that header or the `v2.0.1` Git tag
as proof of compatibility with published 2.0.1 binaries.**

This binding instead targets the **API version 1 header included inside**
`Microsoft.AI.Foundry.Local.Runtime.2.0.1.nupkg`, alongside the native binaries:

| Component | Pin |
|---|---|
| Native package | Microsoft.AI.Foundry.Local.Runtime 2.0.1 |
| Native package SHA-256 | `f509b4509f3fd452bfe9dd0721feabc407689b9d105da71c63e5a2f87bd9f03c` |
| Packaged header SHA-256 | `f220cf707a309b805b3d3a6b2084208927cf17151e68da5d991b9af16a44ae6f` |
| Matching public header revision | `450e857c5695085beafe92e701c6f3309e181675` (same text after newline normalization) |
| ONNX Runtime | Microsoft.ML.OnnxRuntime 1.28.0 |
| ONNX Runtime GenAI | Microsoft.ML.OnnxRuntimeGenAI.Foundry 0.15.2 |
| Java binding | JNA 5.17.0, Apache-2.0 license option |

The exact native build source commit is **not attested by the package metadata**.
The historical header revision is not a claim that the binary was built from that
commit. The same-package header/binary tuple, archive and library hashes, C layout
assertions, and real native calls establish this POC's compatibility boundary.
The runtime version/API handshake is an additional check, not layout proof.

The bundled `runtime-lock.json` records package provenance and references
`native-lock.properties` for per-target hashes. Both live under
`src/main/resources/com/microsoft/foundry/local` and are included in the JAR.
The SDK rejects missing or hash-mismatched native files **before loading them**.
Changing the native runtime requires deliberate revalidation and a new binding lock.

Actual published artifact targets: **Windows x64/arm64, Linux x64/arm64,
macOS arm64**. There is **no macOS x64 artifact**. The hosted matrix exercises
these exact target tuples, not every OS/JVM/hardware configuration. Linux
requires the published runtime's glibc baseline (2.28+) and normal loader
dependencies. Windows needs the Microsoft Visual C++ runtime. Use a JVM matching
the native architecture.

## Build

With JDK 17 or later and Maven 3.9:

```powershell
mvn -f sdk_v2\java\pom.xml package
```

On Windows, `build.ps1` can explicitly bootstrap checksum-pinned Maven 3.9.9
inside `target` if Maven is absent:

```powershell
.\sdk_v2\java\build.ps1 -BootstrapMaven
```

Outputs:

* `sdk_v2\java\target\foundry-local-java-0.1.0-poc.jar` - SDK and CLI example
* `sdk_v2\java\target\lib\jna-5.17.0.jar` - separate, unmodified JNA dependency

Keep the JAR and adjacent `lib` directory together. The build never downloads
models or native inference runtimes. Maven resolves declared Java build dependencies.
On JBR 25, launch with `--enable-native-access=ALL-UNNAMED` before `-jar` to
explicitly authorize JNA native access. Older JVMs are not silently replaced.

## Explicit preparation and license review

Initially supply an explicitly prepared, flat runtime directory. The optional
Python 3 standard-library preparation script downloads only pinned public native
packages, verifies their checksums, and retains licenses/notices:

```powershell
python sdk_v2\java\scripts\prepare_runtime.py --runtime-dir sdk_v2\java\target\runtime --cache-dir sdk_v2\java\target\downloads --rid win-x64 --explicit-download --accept-native-licenses
```

Already cached, checksum-matching packages are reused. Omit `--explicit-download`
for offline-only preparation; missing packages then fail without network access.
The helper never loads native code. It retains the exact packaged header under
`include/foundry_local`, and copies ORT's SONAME aliases on Linux/macOS.
`--verify-only` checks an existing preparation without replacing its files.
Conflicting existing files are rejected, not silently overwritten.

Omit `--rid` to detect the host. It is preparation tooling only: Python is not used
by Java inference. ORT is preloaded by absolute path, then GenAI, then Foundry
Local. Native libraries are deliberately pinned for the JVM lifetime; individual
manager, session, request, item and buffer resources still close deterministically.
Use one runtime directory and **one manager lifetime per JVM**. Reuse that manager
for multiple models/sessions. In the pinned package, recreating a native manager
after shutdown causes a native access violation; this binding rejects recreation
before native entry. Start a fresh JVM for another manager lifetime.

Set the native telemetry opt-out **before process startup**. The SDK also disables
nonessential telemetry in native configuration, but that is not equivalent to
the environment-variable hard opt-out.

```powershell
$env:ORT_TELEMETRY_DISABLED = '1'
$jar = 'sdk_v2\java\target\foundry-local-java-0.1.0-poc.jar'
$runtime = 'sdk_v2\java\target\runtime'
$cache = 'sdk_v2\java\target\model-cache'
java -jar $jar identify --runtime-dir $runtime --cache-dir $cache --json
```

`identify` prints runtime/JVM identity and the **exact** model ID, URI, license
and cache/load state. It may fetch public catalog metadata; it does not download
weights or execution providers. `catalog` lists available ASR variants.

Default exact model:
`nemotron-3.5-asr-streaming-0.6b-generic-cpu:3`.
There is no alias fallback and no cross-model selection.
[model-lock.json](model-lock.json) records the observed exact public model files
and their hashes; it is provenance for the smoke model, not a redistributed bundle
or a network-download byte measurement.
Review the catalog license and the
[NVIDIA model card](https://huggingface.co/nvidia/nemotron-3.5-asr-streaming-0.6b)
before opting into a model download. The model card identifies
[OpenMDW-1.1](https://openmdw.ai/license/1-1/), independently of the SDK's MIT license.
The actual Azure variant's catalog reports MIT, but its license-description link
returned HTTP 404 during this smoke run. Its downloaded `LICENSE` is Microsoft's
MIT license; its `NOTICES` additionally references the
[NVIDIA Open Model License](https://www.nvidia.com/en-us/agreements/enterprise-software/nvidia-open-model-license/)
and Silero VAD's MIT license. The upstream card and packaged notices differ;
do not collapse these into an assertion that all weights are simply MIT.
Retain the model's own license and notices if redistributing model materials;
this project does not redistribute them.

```powershell
java -jar $jar prepare --runtime-dir $runtime --cache-dir $cache --explicit-download --accept-model-license --json
```

Downloads delegate to the native `Model.Download` API, with native progress and
cancellation. No model HTTP client is implemented in Java. The same cache is
reused across commands; neither construction, `load`, nor inference downloads
missing model weights. No accelerator download API is invoked.

**Network limitation:** public catalog lookup is a native network-capable operation,
including the lookup performed by a fresh CLI inference command before loading
already cached weights. API 1 does not expose a supported cache-only public-catalog
switch for in-process inference. Class/configuration construction does not fetch
assets; initialization creates native components and is not certified network-free.
No runtime, model or EP download is invoked by load/inference, but this is **not**
a guarantee that the whole CLI makes no network requests. Native telemetry opt-out
is not proof of network isolation. For strict offline deployments, enforce external
network isolation and retain the native catalog snapshot with the model cache.

## WAV, paced PCM and cancellation

Both paths accept **signed PCM16LE, 16 kHz, mono**. WAV input must be RIFF/WAVE
with an integer PCM `fmt` chunk. Unsupported formats, truncated RIFF chunks,
odd PCM byte lengths and oversized inputs fail explicitly. No implicit resampling.
The example limits each file to 64 MiB; streaming callers can feed longer sources
incrementally. PCM writes are bounded to 1 second per chunk and 2 seconds queued.

```powershell
java -jar $jar transcribe --runtime-dir $runtime --cache-dir $cache --wav sdk_v2\testdata\Recording.wav --json
java -jar $jar stream --runtime-dir $runtime --cache-dir $cache --wav sdk_v2\testdata\Recording.wav --chunk-ms 100 --json
java -jar $jar stream --runtime-dir $runtime --cache-dir $cache --pcm sdk_v2\testdata\Recording.pcm --cancel-after-ms 1200 --json
```

`stream` paces samples against a monotonic clock; it is not a fast file upload
disguised as live input. `transcribe` validates/decodes the WAV container and feeds
its unchanged PCM through the same bounded native queue without real-time pacing.
The packaged Nemotron model rejects native single-AUDIO URI requests; no second
inference engine or transcript fallback is used to work around that limitation.
JSON Lines include `runtime`, `model`, `loaded`, `speech` and terminal `result`
events. Errors have an `error` event and a nonzero process exit code. A requested
inference cancellation is a result with `cancelled: true`, never a successful
final transcript. Progress cancellation can also be exercised with
`prepare --cancel-after-ms 1000`.
Timing is a sibling `timing` object on the result event,
with request-local monotonic input/first-nonblank/final timestamps and submitted
PCM bytes. Unknown observations remain `null`. `requestClosed`, `modelUnloaded`
and `managerClosed` events follow successful cleanup; a result alone does not
establish cleanup success.

Speech events preserve native semantics: `TOKEN` is the native `NONE` segment kind
(a text delta); `PARTIAL` is a cumulative hypothesis; `FINAL` is a stable segment.
The pinned runtime currently emits token deltas for Nemotron, **not real partial
utterance revisions or word timestamps**. Clients may concatenate tokens for an
interim display. The final aggregate transcript comes from native `SPEECH_RESULT`;
no canned transcript or success fallback exists. Missing times remain `null`.

## Java API example

```java
var config = new Configuration(
    "my-asr-app", Path.of("runtime"), Path.of("model-cache"), Path.of("app-data"));
try (var manager = new FoundryLocalManager(config)) {
    var model = manager.catalog().getModel("nemotron-3.5-asr-streaming-0.6b-generic-cpu:3");
    // Separately review model.info().license(), then explicitly model.download(token, progress).
    model.load(); // Fails if the model has not already been downloaded.
    try (var session = model.createAudioSession();
         var run = session.streamPcm(PcmFormat.SPEECH, event -> System.out.print(event.text()))) {
        run.writePcm(pcmChunk); // Complete little-endian samples; copy owned by the request.
        run.finishInput();     // Natural end, flushes recognition. run.cancel() is different.
        TranscriptionResult result = run.await(Duration.ofSeconds(30));
        System.out.println(result.text());
    }
    model.unload();
}
```

Imports are from `com.microsoft.foundry.local`, `java.nio.file.Path`, and
`java.time.Duration`; `pcmChunk` is supplied by the caller.

### Ownership and threading

The manager owns catalogs/models. These borrowed Java views reject calls after
manager close and must not release the native model handles. A session owns at
most one active transcription. Close that transcription before starting another.
`close()` is idempotent; manager close cancels and joins outstanding transcription
workers before releasing native sessions and the manager.

Download progress and speech listeners run on native callback threads. They must
return promptly and must not perform blocking UI work or call SDK lifecycle/input
methods. Reentrant calls are rejected; callback exceptions are captured and
propagated to the Java caller, never thrown across the C boundary. Use a separate
thread to cancel a transcription or the thread-safe `CancellationToken` for
downloads. Do not call `close` from a listener.

PCM backing memory and its deleter callbacks remain reachable until native
ownership ends. Response strings are copied before response/item release.
`close()` waits rather than freeing in-flight memory on a deadline. Consequently,
native hangs can make close block; production process isolation/watchdogs are
outside this POC. Native downloads observe cancellation at progress checkpoints,
not at a guaranteed maximum latency.

## Targeted local checks

Ordinary `mvn test` runs offline unit/layout tests. The native ASR test is opt-in,
requires a previously prepared model, and never downloads:

```powershell
mvn -f sdk_v2\java\pom.xml test '-Dtest=NativeAsrTest' '-Dfoundry.test.runtime=<absolute-runtime-dir>' '-Dfoundry.test.cache=<absolute-model-cache>' '-Dfoundry.test.wav=<absolute-public-wav>'
```

It covers repeated sessions, normal finish versus cancel, callback failure,
callback lifetime after close, borrowed ownership, manager cascaded close and
worker termination. `src\test\native\layout.c` can be compiled against
the header extracted from the pinned runtime package to assert actual C layout
and function-table offsets. Never substitute the current source-tree header.

`scripts\measure.ps1` launches one Java process, samples only its process tree,
and saves JSONL output, stderr and actual timing/memory measurements under a
caller-selected build-output prefix. Sampled memory is explicitly a lower bound,
not a claimed platform-independent benchmark.

The upstream speech fixture lives at `sdk_v2\testdata\Recording.wav` in the public
MIT-licensed repository. Its SHA-256 is
`8b3ec34ebf66bc3729841c35064ac2d0a5601b5edc0344d1a7a435d3d1b52f22`;
the corresponding raw PCM SHA-256 is
`8383ed4c0fcfd164a6571aa35f5aec9cec02dc0b1103a8f807d6a7d68a5d37c3`.
The fixture is not copied into the Java artifact. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for dependency notices.
