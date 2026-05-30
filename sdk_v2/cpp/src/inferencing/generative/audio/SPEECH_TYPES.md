# Speech Types — Design

Native SDK types returned by `AudioSession` for speech-to-text (file and live),
translation, and ASR scenarios. References: OpenAI `verbose_json` / Realtime
transcription events, Azure Speech SDK recognition results.

## Design rules

- **One set of types covers transcription, translation, and ASR.** Task
  selection (transcribe vs translate, target language) is a Request parameter,
  not a type variant. `text` is the recognized-or-translated string either way.
- **One shared segment type** for both streaming events and final-result entries,
  discriminated by `kind`.
- **No event wrapper, no `event_id`, no segment `id`.** Ordering is a property of
  the callback channel; segment identity is implicit in stream order (zero-or-more
  `kPartial` for the current segment, then one `kFinal` closes it). A web service
  above the SDK can add envelope/sequence metadata.
- **`text` on `kPartial` is the cumulative current hypothesis for the segment**,
  not a delta-since-last-event (Azure-style). A delta is recoverable by diffing
  against the previous hypothesis.
- **`utterance_start` is a boolean on the segment.** Knowable at emission time
  (VAD says "speech started" → producer tags the first `kPartial` of the new
  segment). There is no `utterance_end` field: end-of-utterance can't be known
  when the `kFinal` is emitted without delaying it by the silence threshold.
  Instead, end is implicit — the next `utterance_start` marks it (consumer
  infers end at the previous `kFinal.end_time`), a future `kSilence` event
  marks it explicitly, or the final `SpeechResult` marks it for file
  transcription.
- **Time as `int64_t` milliseconds.** Must survive the C ABI. Typedef'd so the
  unit is legible and changeable in one place.
- **Two C ABI item types** — one for streaming segments, one for the final
  aggregate. Both additive to existing items.

## Types

```cpp
namespace fl {

using DurationMs = std::int64_t;  // milliseconds; C ABI-safe

enum class SpeechSegmentKind : int {
  kNone     = 0,   // entry in a final aggregate result
  kPartial  = 1,   // streaming: hypothesis for the current segment; may change
  kFinal    = 2,   // streaming: segment is stable, or an entry in the final result
};

struct SpeechWord {
  std::string text;
  std::optional<DurationMs> start_time;
  std::optional<DurationMs> end_time;
  std::optional<float> confidence;        // 0..1
  std::optional<std::string> speaker_id;
};

struct SpeechSegment {
  SpeechSegmentKind kind = SpeechSegmentKind::kNone;

  std::string text;                       // for kPartial: cumulative current hypothesis
  std::optional<DurationMs> start_time;
  std::optional<DurationMs> end_time;

  // Utterance start signal — tagged on the first kPartial of a new utterance.
  // Knowable at emission time. End-of-utterance is implicit (see design rules).
  bool utterance_start = false;

  std::vector<SpeechWord> words;          // word-timestamp opt-in

  // Future / opt-in. Included here for visibility in review. 
  // We should only add fields that we expect to use as the C API types need to be ABI stable,
  // so we can't remove anything added.
  std::optional<float> confidence;        // 0..1 aggregate
  std::optional<std::string> language;    // per-segment, for code-switching
  std::optional<std::string> speaker_id;
  std::optional<std::int32_t> channel;
  // we could maybe use something more generic if we want to report these things instead of having per-value fields
  // e.g. shared float[] of fixed size and an enum saying which value is in which slot.
  std::optional<float> avg_logprob;       // Whisper-family diagnostic
  std::optional<float> no_speech_prob;    // Whisper-family diagnostic
  std::optional<float> compression_ratio; // Whisper-family diagnostic
};

struct SpeechResult {
  std::string text;                       // concatenated final transcript
  std::optional<std::string> language;    // detected source language
  std::optional<DurationMs> duration;     // total audio duration
  std::vector<SpeechSegment> segments;    // entries are kFinal or kNone
};

}  // namespace fl
```

## C ABI item types

```c
FOUNDRY_LOCAL_ITEM_SPEECH_SEGMENT = 31,  // pushed via streaming callback
FOUNDRY_LOCAL_ITEM_SPEECH_RESULT  = 32,  // final aggregate in response.items
```

`TextItem` remains the trivial fallback for `response_format: "text"`.

## V1 scope

Populated in the initial implementation:

- `SpeechSegmentKind`: `kNone`, `kPartial`, `kFinal`
- `SpeechSegment`: `kind`, `text`, `start_time`, `end_time`,
  `utterance_start` (defaulted; populated when computable)
- `SpeechResult`: `text`, `language`, `duration`, `segments`

Defined in the header but unpopulated until a producer exists:

- `SpeechWord` and `SpeechSegment::words` (word-timestamp opt-in)
- `confidence` (segment and word)
- `avg_logprob`, `no_speech_prob`, `compression_ratio` (Whisper diagnostics)
- `language` / `speaker_id` / `channel` on segment
- `speaker_id` on word

## Growth headroom (not built)

- **Diarization**: `speaker_id` already present on word and segment.
- **Multi-channel audio**: `channel` already present on segment.
- **N-best alternatives**: future `std::vector<SpeechAlternative> alternatives`
  on `SpeechSegment`.
- **OpenAI `verbose_json` compatibility**: handled by a
  `ToOpenAIVerboseJson(const SpeechResult&)` adapter in
  `contracts/audio_transcriptions.*`, not by changing native types.

Multi-target translation in a single pass is intentionally out of scope —
that's a server-side concern, not a local-inferencing one.
