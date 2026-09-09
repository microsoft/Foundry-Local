# Local native smoke boundary

This is experimental POC evidence, not production release approval. Java 17 and
the explicitly selected IU 2026.1.5 / JBR 25 native ASR smoke succeed; the original
consumer JBR 21 remains incompatible with the pinned native dependencies.
Strict process-wide network isolation is also not established.

The final-artifact native observations below refer to source
`06bf21e65f9a48518a0422558c5bbac42b2fd618`, JAR SHA-256
`d9620c6a40199f1bc8359c91dad4d07e3dc5b3f959a2ad6b3029e8053a45bbcf`.
The subsequent `d0946a0` UTF-8 JSONL and cancellation-publication fixes now have
independent local regression and hosted Windows evidence. The earlier
observations in this report remain attached to their original artifacts, not
relabeled as that newer binary's measurements. See [MODEL_LOCK.md](MODEL_LOCK.md)
for its unchanged binary provenance and the separate target metadata contract.

## Public tuples and scope

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

## Attributed speech

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
in ignored local output, not in the published SDK. The 20-utterance run preceded
the final additive cancellation-timestamp/bounded-file-read changes; final Java 17
native lifecycle, headerless PCM, cancellation and cached-proxy probes were rerun.
There is no fabricated fully conforming evaluator measurement: exact network
payload byte counts are unknown and its current schema requires concrete counts.

## Native behavior and limits

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

## Authorized alternate JBR native experiment

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
switch. ORT telemetry opt-out is not network isolation. Physical microphone,
other OS/CPU targets and hosted CI were not exercised.
