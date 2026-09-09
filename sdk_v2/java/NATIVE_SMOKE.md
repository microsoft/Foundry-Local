# Native smoke evidence and boundaries

## Five-target hosted smoke

The second and final standard
[hosted matrix](https://github.com/jiec-msft/foundry-local/actions/runs/34411280765)
completed successfully at **2026-09-09T22:26:06Z**, workflow `354315764`, attempt 1.
Its execution was `07e40f066997d326c189f492225bc5a7bb193c0a`, not the later
documentation-only reporting commit. The
[immutable complete report](https://github.com/jiec-msft/foundry-local/blob/76706e3f8aa84a6500936a14d31eefa3fe96ce4c/sdk_v2/java/evaluation/SECOND_MATRIX_EVALUATION.md)
contains per-target archive/extracted hashes, provenance and unrounded metrics.
No evaluator implementation or history is incorporated into this SDK branch.

| Pin | Exact identity |
|---|---|
| Binary source | `d0946a0764d9cfa4b3d684940d6d5c66165427b8` |
| Thin Java 17 JAR | 64,000 bytes; SHA-256 `bf644d3127afff912683731094821a8f6a751f003c284a9c15ddceaecebe0863` |
| External metadata source | `22ebea63b07addb526a1792e0303ba2f572f444a` |
| Runtime / ABI | Microsoft.AI.Foundry.Local.Runtime 2.0.1 / packaged API 1 |
| ORT / GenAI.Foundry | 1.28.0 / 0.15.2 |
| Model / provider | `nemotron-3.5-asr-streaming-0.6b-generic-cpu:3` / `CPUExecutionProvider` |

Every target completed **10 non-cancelled WAV transcriptions, 10 non-cancelled
20 ms paced streams and 1 early cancelled stream**. Each of the 105 ASR results
has ordered `result`, `requestClosed`, `modelUnloaded`, `managerClosed`, zero
result-time child processes and process exit 0. Identify and prepare also exit
0, but their early-return path does not provide a `managerClosed` event.
No native lane, preparation or inference was skipped, failed or timed out.

| Native RID | Standard runner | Actual Java 17 distribution | Retained native artifact |
|---|---|---|---|
| `win-x64` | windows-2022 | Eclipse Adoptium 17.0.20.1+1 | [10127451632](https://github.com/jiec-msft/foundry-local/actions/runs/34411280765/artifacts/10127451632) |
| `win-arm64` | windows-11-arm | Microsoft 17.0.20.1+1-LTS | [10127448980](https://github.com/jiec-msft/foundry-local/actions/runs/34411280765/artifacts/10127448980) |
| `linux-x64` | ubuntu-24.04 | Eclipse Adoptium 17.0.20.1+1 | [10127544789](https://github.com/jiec-msft/foundry-local/actions/runs/34411280765/artifacts/10127544789) |
| `linux-arm64` | ubuntu-24.04-arm | Eclipse Adoptium 17.0.20.1+1 | [10127535699](https://github.com/jiec-msft/foundry-local/actions/runs/34411280765/artifacts/10127535699) |
| `osx-arm64` | macos-15 | Eclipse Adoptium 17.0.20.1+1 | [10127655814](https://github.com/jiec-msft/foundry-local/actions/runs/34411280765/artifacts/10127655814) |

Host, JVM and native architectures agree in every lane. macOS x64 remains
unsupported; ARM64 does not establish a particular Apple chip.
There are **115 process wrappers**, **6,763 nested SDK events**, **105 ASR cleanup
sequences**, and **100 complete non-cancelled measurement/scorer samples**.
The wrappers are not SDK events, and the samples are not 100 unique recordings:
all modes/platforms reuse the same ten attributed LibriSpeech utterances,
151 reference words and 60.28 seconds of audio.

Each target and each mode has **8/151 edits (5 substitutions, 0 deletions,
3 insertions), WER 5.298%**. The clean subset is 3/62 and other is 5/89.
Independent review reproduced the raw/normalized text and scorer counts.
This small binding smoke set is not product-quality or cross-model evidence.

The following are min-max ranges over the same ten paced requests, except
cancellation, which is one observation per target. First nonempty is measured
from first admitted input, and finalization from natural input close, using
request-local monotonic timestamps. They are not p95 or latency guarantees.

| Native RID | Paced first nonempty ms | Paced finalization ms | Single cancel acknowledgment ms | Root JVM RSS bytes |
|---|---|---|---:|---:|
| `win-x64` | 1265.882-2378.700 | 167.161-248.385 | 64.854 | 1040883712 |
| `win-arm64` | 1273.926-2406.847 | 183.809-259.385 | 64.993 | 1013121024 |
| `linux-x64` | 1260.222-2381.965 | 168.692-250.883 | 71.863 | 1059328000 |
| `linux-arm64` | 1237.395-2355.634 | 144.482-194.798 | 43.153 | 1018228736 |
| `osx-arm64` | 1313.865-2533.781 | 226.714-522.701 | 147.680 | 1020477440 |

Cancelled results have empty final text, `cancelled=true`, no natural input-close
time and native finish reason 0, followed by explicit cleanup and exit 0.
RSS is the maximum recorded across each lane's 23 root JVM processes, not
whole-system memory. Windows/Linux use sampled OS peak counters; macOS samples
current RSS and is only a lower bound. Hardware is not controlled, and CPU model/
core count were not captured. Paced RTF includes deliberate waiting. End-to-end
readiness and network-transfer bytes remain unknown with explicit reasons;
installed sizes and progress percentages are not substitutes.

**Retained-integrity limitation:** success artifacts omitted
`model-verification.json`. Reviewed executed code enforces all sixteen actual
raw file hashes/sizes, the complete ordinal manifest and installed total before
ASR. Retained v3 metadata and actual installed totals match the selected Windows
or non-Windows contract. However, the actual per-file rows from this run are
unavailable for independent manifest replay. No rows are invented and no earlier
diagnostic inventory is relabeled as an observation from this matrix.

The [thin-JAR artifact](https://github.com/jiec-msft/foundry-local/actions/runs/34411280765/artifacts/10127340452)
and five evidence archives total 180,643 archived / 1,196,790 extracted bytes.
The report records their independently checked exact hashes; no native/model/
runtime/corpus bundles are included. Artifacts expire on 2026-09-16.
The budget is exhausted: **2/2 full matrices plus 3 diagnostics, 5 runs total**.
No further run is authorized. These results do not establish UI/plugin,
physical microphone or full user-facing readiness, strict offline behavior,
production qualification, or model/native redistribution rights.

## Earlier local Windows evidence (separate artifacts)

This is experimental POC evidence, not production release approval. Java 17 and
the explicitly selected IU 2026.1.5 / JBR 25 native ASR smoke succeed; the original
consumer JBR 21 remains incompatible with the pinned native dependencies.
Strict process-wide network isolation is also not established.

The earlier final-artifact native observations below refer to source
`06bf21e65f9a48518a0422558c5bbac42b2fd618`, JAR SHA-256
`d9620c6a40199f1bc8359c91dad4d07e3dc5b3f959a2ad6b3029e8053a45bbcf`.
The subsequent `d0946a0` UTF-8 JSONL and cancellation-publication fixes now have
independent local regression and the hosted five-target evidence above. The earlier
observations in this report remain attached to their original artifacts, not
relabeled as that newer binary's measurements. See [MODEL_LOCK.md](MODEL_LOCK.md)
for its unchanged binary provenance and the separate target metadata contract.

### Public tuples and scope

Native runtime/header/dependency pins are in the bundled runtime lock. The
selected model was exactly `nemotron-3.5-asr-streaming-0.6b-generic-cpu:3`,
`CPUExecutionProvider`, model type `nemotron_speech`, PCM16LE/16000 Hz/mono.
One explicitly authorized download populated the dedicated cache; all subsequent
runs reused it. Its 16 installed files total **793,344,452 bytes**, not network
download bytes. The SHA-256 of its sorted file manifest is
`483ce0b37c44b952a369de4257161df7ca42c8621109f20222ad1a9126f55001`.
The manifest hashes UTF-8 lines sorted by ordinal filename, each consisting of
filename, TAB, decimal byte size, TAB, lowercase file SHA-256, LF.
`genai_config.json` SHA-256:
`d114900e0bc8ecf3474d8dd566a1b75c3f1e8771eb5f490fce97055fa2e080d8`.
Its encoder chunk size is 8960 samples; VAD threshold is 0.3 and silence duration
is 3360 ms. No model settings or weights were changed.
This Windows baseline manifest is [model-lock.json](model-lock.json).
The additive [external target inventory contract](MODEL_LOCK.md) preserves it
and distinguishes subsequently observed target metadata from ASR qualification.

The model's MIT wrapper license, NVIDIA license references, upstream OpenMDW
card and broken catalog license link are distinguished in README and the notices.
No native/model materials are redistributed.

### Attributed speech

Fixtures/scorer were extracted read-only from public
`jiec-msft/foundry-local@70fe3c3e2561274ea79d3546c365652604a3253c`.
The ten independent LibriSpeech development utterances are five clean and five
other, ten speakers, 60.28 seconds, 1,929,400 WAV bytes. Attribution: Vassil
Panayotov, Guoguo Chen, Daniel Povey, Sanjeev Khudanpur; LibriVox readers.
Audio/references are CC-BY-4.0, not the SDK's MIT license. Preserve the evaluator's
manifest and attribution when using them.

Both unpaced WAV and 20 ms paced streaming completed all ten utterances on
Temurin 17.0.20.1+1. The independently implemented evaluator's `score_pair` and
`aggregate` functions produced identical pooled results for both paths:

| Subset | Reference words | Substitutions | Deletions | Insertions | Pooled WER |
|---|---:|---:|---:|---:|---:|
| All | 151 | 5 | 0 | 3 | 5.298% |
| Clean | 62 | 1 | 0 | 2 | 4.839% |
| Other | 89 | 4 | 0 | 1 | 5.618% |

Raw transcripts were not edited. For example, a native `<en-US>` token remains
in the output and is scored by the frozen normalizer, not stripped opportunistically.
All ten paced runs emitted nonblank native token deltas before input closed.
First meaningful output was 1269.6..2406.9 ms after first input; finalization
was 175.9..313.4 ms after input close. Paced RTF was approximately 1.02..1.07
and includes deliberate real-time waits. These are token deltas, not revisable
utterance hypotheses. Sampled process-tree working-set peak across paced runs
was 1,057,222,656 bytes; sampling is a lower bound, not a universal benchmark.
Java 17 runs observed one process and zero descendants at result time.

Full raw JSONL, raw/normalized references/hypotheses and timing reports remain
in ignored local output, not in the published SDK. The 20-case run preceded
the final additive cancellation-timestamp/bounded-file-read changes; final Java 17
native lifecycle, headerless PCM, cancellation and cached-proxy probes were rerun.
Those early runs did not produce a fully conforming evaluator measurement:
network payload byte counts were unknown and the schema at that stage required
concrete counts. Later hosted v3 measurements above retain explicit unknowns;
they do not retrofit measurements into these earlier runs.

### Native behavior and limits

Java 17 native tests execute without skipping: WAV, repeated PCM sessions, natural
finish, cancellation, listener failure, callback lifetime, buffer release, borrowed
handles, cascaded manager close and worker termination. A separately paced
cancellation observed native acknowledgment about 3.5 ms after the Java cancel
request, followed by request/model/manager cleanup events. This single observation
is not a cancellation latency guarantee. Headerless PCM also completed.

Two package limitations required explicit binding behavior:

- Nemotron rejects the single-AUDIO URI path. WAV input now uses its unchanged PCM
  through the supported bounded queue; inference still runs only in Foundry.
- Native manager recreation after shutdown reproducibly caused an access violation.
  The binding now rejects a second manager lifetime before native entry; reuse the
  first manager for repeated sessions, or start another JVM.

Consumer JBR 21.0.8+1-b895.146 passed 11 offline tests but native ORT loading failed
both under Maven and directly. An isolated preload of already-installed system
CRT DLLs did not resolve it. Subsequent controlled load-only diagnosis isolated
the older CRT as causal: a fresh non-JVM host with that CRT reproduces a null-read
access violation during ORT initialization; modern CRT controls succeed. See
[WINDOWS_LOADING.md](WINDOWS_LOADING.md) for exact errors and independent JVM
comparisons. No JVM/system installation was modified; the original JBR 21 failure is unchanged.

### Authorized alternate JBR native experiment

The separately installed official IU 2026.1.5 / JBR 25.0.4+1-b329.128 combination,
with actually loaded CRT 14.44.35211.0, was explicitly authorized as an alternate
Windows POC compatibility candidate. This was not an automatic fallback or a
silent change to the original consumer baseline.

Its `NativeAsrTest` passed (1 test, 0 failures, 0 errors, **0 skips**, 14.20 seconds):
WAV, repeated PCM sessions, natural finish, real cancel, listener failure,
callback lifetime, borrowed ownership, cascaded close, worker termination and
rejection of manager recreation. Separate final-JAR CLI runs completed the
attributed 6.8-second `84-121123-0003` WAV and its headerless PCM at 20 ms pacing.
Both returned the same native transcript and finish reason 2, then successful
request/model/manager cleanup events. Paced first meaningful output was 1288.36 ms
after first input and finalization was 274.21 ms after input closed.

A separate 1500 ms cancellation run submitted 48,640 PCM bytes, observed native
acknowledgment 3.70 ms after the Java cancellation call, returned empty text with
`cancelled: true` and finish reason 0, and emitted all cleanup events. These are
observations, not latency guarantees. Each CLI run observed one process and no
descendants at result time; the largest sampled working set was 1,077,510,144 bytes.
No IDE, model download, or replacement native library was involved. The full
20-case corpus sweep was not rerun on JBR 25, and no JBR 25 pooled WER is claimed.

A cached WAV inference succeeded with process-local HTTP/HTTPS/ALL proxy
variables pointing to an unreachable loopback port. This demonstrates that probe,
not a firewall-enforced offline guarantee. Native public catalog lookup can access
the network and the pinned in-process API has no supported cache-only catalog
switch. ORT telemetry opt-out is not network isolation. These earlier local
experiments did not exercise other OS/CPU targets or hosted CI; the hosted matrix
above is separate evidence. Physical microphone integration remains untested.
