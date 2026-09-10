# Java ASR API and JSON Lines contract

Version: `com.microsoft.foundry:foundry-local-java:0.1.0-poc`, Java 17 bytecode.
Launcher: `java -jar foundry-local-java-0.1.0-poc.jar`; main class:
`com.microsoft.foundry.local.cli.AsrCli`. Keep `lib/jna-5.17.0.jar` adjacent.
This is a standalone experimental SDK, not a production release.

## Public Java surface

All types below are in `com.microsoft.foundry.local`. Public records expose the
listed components through their standard Java record accessors.

| Type | Constructors, methods or record components |
|---|---|
| `Configuration` | `(String appName, Path runtimeDirectory, Path modelCacheDirectory, Path appDataDirectory)` |
| `FoundryLocalManager implements AutoCloseable` | `(Configuration)`; `String runtimeVersion()`; `String nativeTarget()`; `Catalog catalog()`; `void close()` |
| `Catalog` | `Model getModel(String exactId)`; `List<ModelInfo> models()` |
| `Model` | `ModelInfo info()`; `boolean isCached()`; `boolean isLoaded()`; `Path path()`; `void download(CancellationToken, DoubleConsumer progress)`; `void load()`; `void unload()`; `AudioSession createAudioSession()` |
| `ModelInfo` | `String id, alias, name`; `int version`; `String uri, executionProvider, task, license, licenseDescription` |
| `CancellationToken` | `()`; `void cancel()`; `boolean isCancelled()` |
| `AudioSession implements AutoCloseable` | `Transcription transcribeWav(Path, Consumer<SpeechEvent>) throws IOException`; `Transcription streamPcm(PcmFormat, Consumer<SpeechEvent>)`; `void close()` |
| `Transcription implements AutoCloseable` | `void writePcm(byte[]) throws InterruptedException`; `void finishInput()`; `void cancel()`; `boolean isDone()`; `boolean isClosed()`; `boolean isCancelled()`; `TranscriptionTiming timing()`; `TranscriptionResult await() throws InterruptedException`; `TranscriptionResult await(Duration) throws InterruptedException, TimeoutException`; `void close()` |
| `PcmFormat` | `(int sampleRate, int channels, int bitsPerSample)`; constant `SPEECH`; `void validateChunk(byte[])` |
| `WavAudio` | `(PcmFormat format, byte[] pcm)`; `static WavAudio read(Path) throws IOException`; `double durationSeconds()`; defensive-copy `byte[] pcm()` |
| `SpeechEvent` | `Kind kind`; `String text`; `Long startTimeMs, endTimeMs`; `boolean utteranceStart`; `Kind` is `TOKEN`, `PARTIAL` or `FINAL` |
| `TranscriptionResult` | `String text, language`; `Long durationMs`; `boolean cancelled`; `int nativeFinishReason`; `long elapsedMillis` |
| `TranscriptionTiming` | nullable `Double firstInputMillis, firstNonemptyMillis, inputClosedMillis, finalizedMillis, cancellationRequestedMillis`; `long submittedBytes` |
| `FoundryLocalException extends RuntimeException` | `(int code, String message)`; `int code()` |

`download` is blocking and explicit. Percentages are the native 0..100 values;
the API provides no byte totals. Cancellation tokens are thread-safe. Callback
exceptions become Java errors after native processing, never exceptions crossing
the C callback boundary.

Only PCM16LE/16000 Hz/mono is accepted. Public PCM writes are 2..32000 bytes;
backpressure limits outstanding native-owned PCM to 64000 bytes. WAV files are
bounded to 64 MiB and fed automatically, without pacing, through the native PCM
queue. Do not call `writePcm` or `finishInput` for an automatic WAV transcription.
Call `finishInput` for a manually fed stream to flush; `cancel` interrupts instead.
Cancellation accepted before terminal result publication discards the final
transcript, including cancellation during native result decoding. Once a result
or error is committed, cancellation is too late and does not change its state.

Close each request before reusing its session. The manager owns sessions, while
catalog/model handles are borrowed. Close cascades, cancels and joins workers,
then releases native objects. A manager has one lifetime per JVM: after native
shutdown, manager reconstruction is rejected. `close` is idempotent but may
block if native code hangs. See README for callback restrictions and the native
catalog/network limitation; neither an empty cache nor a missing model triggers
an implicit model download.

## CLI options

Commands: `identify`, `catalog`, `prepare`, `transcribe`, `stream`.
All take `--runtime-dir DIR --cache-dir DIR`; optional
`--app-data-dir DIR`, `--model EXACT_ID`, `--json`. Output is JSONL even without
`--json`. `--help` intentionally prints human-readable usage.

`prepare` requires `--explicit-download --accept-model-license`.
`transcribe` requires `--wav FILE`. `stream` requires exactly one of `--wav FILE`
or `--pcm FILE`; `--chunk-ms N` defaults to 100 and accepts 1..1000. Measurement
callers should choose 20..100. `--cancel-after-ms N` schedules cancellation of a
download/request, not of model loading. Exit code 1 indicates an error; requested
inference cancellation exits 0 with `cancelled: true` and no final transcript.

Default exact model: `nemotron-3.5-asr-streaming-0.6b-generic-cpu:3`. No alias,
version or execution-provider fallback is performed.

## JSON Lines schema (v1)

The machine-readable schema is [cli.schema.json](cli.schema.json). It describes
one event per line, not a JSON array or the separate loader diagnostic output.

Each stdout line is one UTF-8 JSON object regardless of the platform's default
encoding. Whole lines are serialized across callback threads. Field order is
unspecified. Strings use JSON escaping, timestamps are milliseconds, byte counts
are integers, and unavailable
observations are JSON `null`, never invented zeros. Do not treat error messages
as a stable machine interface.

| `event` | Required fields besides `event` |
|---|---|
| `runtime` | `version: string`, `apiVersion: 1`, `nativeTarget: string`, `javaVersion: string`, `javaVendor: string`, `pid: integer` |
| `model` | `model: ModelInfo`; exact-model commands also include `cached: boolean`, `loaded: boolean`; catalog listing omits these state fields |
| `download` | `percent: number`; deliberately no downloaded-byte or total-byte fields |
| `prepared` | `modelId: string`, `cached: boolean`, `elapsedMillis: integer` |
| `loaded` | `elapsedMillis: integer` (model load only, not catalog/preparation time) |
| `speech` | `speech: SpeechEvent`, `elapsedMillis: integer` (CLI request-start origin) |
| `result` | `modelId: string`, `result: TranscriptionResult`, `mode: "transcribe" or "stream"`, `audioSeconds: number`, `timing: TranscriptionTiming`, `chunkMillis: integer`, `processChildren: integer` |
| `requestClosed` | `elapsedMillis: integer`; request close plus cancellation-timer termination duration |
| `modelUnloaded` | no other fields; the session has closed and unload completed |
| `managerClosed` | no other fields; emitted after the native manager closes |
| `error` | `errorType: string`, `code: integer` (native error code or -1), `message: string` |

Record-valued objects serialize exactly the record components in the API table.
The `result` event's sibling `timing` object uses one **request-local** monotonic
origin; do not mix it with CLI `speech.elapsedMillis` or model timestamps.
`firstNonemptyMillis` records the first nonblank native speech callback,
`inputClosedMillis` records natural end-of-input (null if cancelled before input close), and
`finalizedMillis` records final native response receipt. `submittedBytes` counts
PCM actually handed to the native queue, not file/container bytes or network
traffic. Paced inference wall time is finalized minus first input; finalization
latency is finalized minus input close. Paced RTF includes deliberate waits.
Cancellation acknowledgment latency is finalized minus cancellation requested;
both values must be observed, not inferred from the requested CLI delay.

Native `TOKEN` events are deltas, not revisable partial utterances. In the pinned
Nemotron smoke, they arrive before input closes. The aggregate native result is
the authoritative final text. Missing/unfinished responses without observed
cancellation raise errors. A terminal result is emitted before cleanup; consume
the later lifecycle events or process failure to establish cleanup outcome.

Native package/header and per-platform library locks are bundled in resources.
Model/artifact hashes and actual platform/JVM measurements belong to a specific
run; neither this contract nor the existence of a platform artifact claims that
all platforms or JVMs have passed native inference.
