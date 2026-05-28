# Realtime Audio Streaming Plan

> **Status: IMPLEMENTED** — All items in this plan are complete. See below for
> what was implemented and any deviations from the original plan.
>
> **Deviations from plan:**
> - ORT GenAI `OgaStreamingProcessor` was NOT needed. The existing `OgaMultiModalProcessor`
>   + `OgaGenerator` pipeline handles streaming audio natively when fed PCM chunks via
>   `ItemQueue::WaitAndPop`. No version bump to 0.12.2 was required.
> - WebSocket endpoint for streaming audio is deferred — not needed for direct SDK usage.
>   The `ItemQueue` push pattern works for both C++ and C# SDK callers.
> - C# SDK added `LiveAudioTranscriptionSession` wrapping the native streaming via
>   `AudioItem.CreateFormatDescriptor` + `ItemQueue` + `BytesItem.CreateOwned` push.
>
> **What was delivered:**
> - `flAudioData` extended with `sample_rate`/`channels` (C ABI + C# P/Invoke)
> - `ItemQueue::WaitAndPop()` with condition variable for blocking reads
> - `AudioSession::ProcessStreamingAudio()` — full streaming pipeline
> - PCM s16le → float32 conversion utility
> - 5 C++ integration tests (chunks, initial data, empty queue, cancellation, callback)
> - C# `LiveAudioTranscriptionSession` implementation + 49/49 C# tests passing

> Extend AudioSession to support incremental PCM audio input via ItemQueue,
> enabling real-time transcription (live microphone → text).

## Architecture Overview

### C# Two-Layer Design (for reference only)

The C# implementation spans two layers that we are collapsing into one:

1. **FoundryLocalCore** (`AudioStreamingSession.cs`): Owns the ORT GenAI
   `StreamingProcessor` + `Generator` + `Tokenizer` pipeline. Exposes three
   FFI entry points: `audio_stream_start`, `audio_stream_push` (binary),
   `audio_stream_stop`. Each push feeds PCM bytes to `StreamingProcessor.Process()`,
   which buffers internally and returns `NamedTensors` when a full encoder chunk
   (~30s) is ready. The generator then decodes tokens from those tensors.

2. **Public SDK** (`LiveAudioTranscriptionSession.cs`): Manages a bounded push
   channel, a background drain loop, and an output channel for transcription
   results. Serializes audio chunks to the core via P/Invoke.

### C++ Unified Design

The C++ SDK eliminates the FFI boundary. The caller constructs a `Request` with
two items and calls `AudioSession::ProcessRequest()` directly — the same pattern
used for all other session types.

```
Caller thread                     AudioSession::ProcessRequestImpl
─────────────                     ────────────────────────────────
Request req;
auto audio = AudioItem(nullptr, 0, "pcm_s16le");  // format descriptor, no initial data
auto queue = ItemQueue();
req.AddOwnedItem(audio);
req.AddBorrowedItem(&queue);       // caller keeps queue alive

// Spawn session processing
auto future = std::async([&] {
  session.ProcessRequest(req, response);
});

// Push PCM chunks as they arrive
queue.Push(BytesItem(chunk1));     →  WaitAndPop() returns chunk1
queue.Push(BytesItem(chunk2));     →  WaitAndPop() returns chunk2
...
queue.MarkFinished();              →  WaitAndPop() returns nullptr (EOF)

future.get();                      // response has full transcript
```

**Key difference from C#:** No session factory, no session handles, no binary
FFI entry point. The ItemQueue *is* the push channel. The session processes
items from the queue until it's finished or canceled.

---

## Detailed Design

### 1. `AudioItem` Field Extensions

Add `sample_rate` and `channels` to `flAudioData` (C ABI) and `AudioItem`
(internal). These describe the raw PCM data for streaming input. They are
irrelevant for URI-based audio (ORT GenAI reads format from the file header).

**C ABI struct** (`flAudioData` in `foundry_local_c.h`):
```c
struct flAudioData {
  uint32_t version;
  const void* data;
  void* mutable_data;
  size_t data_size;
  const char* format;            ///< Audio format: "mp3", "wav", "pcm", etc.
  const char* uri;
  int sample_rate;               ///< Sample rate in Hz (e.g. 16000). 0 = unspecified.
  int channels;                  ///< Channel count: 1 = mono, 2 = stereo. 0 = unspecified.
  flAudioDataDeleter deleter;
  void* deleter_user_data;
};
```

**Internal struct** (`AudioItem` in `audio_item.h`):
```cpp
struct AudioItem : Item {
  // ... existing fields ...
  int sample_rate = 0;   // 0 = unspecified (file-based path detects from header)
  int channels = 0;      // 0 = unspecified
};
```

**Format string convention:**
- `"pcm"` — raw PCM. Default assumption: 16-bit signed little-endian (s16le).
  This is what every audio capture API produces by default.
- `"mp3"`, `"wav"`, etc. — encoded formats (file-based path only).
- Matches MIME subtype fragments (`audio/mpeg` → `"mp3"`, `audio/pcm` → `"pcm"`).
- If float32 PCM is ever needed, add `"pcm_f32"` as a future extension.

**Design rationale for `channels` (int) vs `stereo` (bool):**
- Standard audio terminology uses channel count.
- `flAudioData` is a C ABI struct — `int` is cleaner than `bool` in C.
- `channels = 1` reads better than `stereo = false`.
- Matches the C# `AudioStreamingSettings.Channels` field.

**`bits_per_sample` is NOT needed** — the format string encodes it implicitly.
`"pcm"` = s16le by default. No realistic scenario sends big-endian or unsigned
PCM from a microphone capture API.

### 2. Existing: `BytesItem` and C ABI

`BytesItem` (`src/items/bytes_item.h`) already exists with `FOUNDRY_LOCAL_ITEM_BYTES = 1`,
`SetBytes`/`GetBytes` C ABI, and `flBytesData` struct. The `item_type` tag on
`BytesItem` lets the receiver distinguish audio PCM chunks from other byte payloads.

For streaming audio, the caller creates `BytesItem` instances with
`item_type = FOUNDRY_LOCAL_ITEM_AUDIO` and pushes raw PCM bytes through the
`ItemQueue`. No changes needed.

### 3. ItemQueue Blocking Read: `WaitAndPop`

Current `TryPop()` is non-blocking (returns nullptr when empty). Streaming
needs a blocking read that waits for data or a signal that the queue is done:

```cpp
// In item_queue.h — new method
/// Wait up to `timeout` for an item to become available.
/// Returns the item if one was available (immediately or after waiting).
/// Returns nullptr if the timeout expired, the queue is finished, or it was
/// woken by notify. No loop — the caller decides what to do on nullptr
/// (check cancellation, check overall timeout, retry, etc.).
std::unique_ptr<Item> WaitAndPop(std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
  std::unique_lock<std::mutex> lock(mutex);

  if (items.empty() && !finished) {
    cv.wait_for(lock, timeout);
  }

  if (!items.empty()) {
    auto front = std::move(items.front());
    items.pop_front();
    return front;
  }

  return nullptr;
}
```

Also add a `std::condition_variable cv` member and notify in `Push()` and
`MarkFinished()`:

```cpp
void Push(std::unique_ptr<Item> item) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    items.push_back(std::move(item));
  }
  cv.notify_one();
}

void MarkFinished() {
  {
    std::lock_guard<std::mutex> lock(mutex);
    finished = true;
  }
  cv.notify_all();
}
```

The caller (AudioSession) owns all policy:
```cpp
while (!request.canceled) {
  auto item = queue.WaitAndPop(std::chrono::milliseconds(100));
  if (!item) {
    if (queue.IsFinished()) break;  // producer is done
    continue;                       // timeout — re-check canceled, loop
  }
  // process item...
}
```

`WaitAndPop` is internal only — no C ABI wrapper. External callers push
items into queues (streaming callback) but never wait to read from them.
The only consumer of `WaitAndPop` is `AudioSession`.

### 4. Streaming Audio Generator — `OgaStreamingProcessor`

ORT GenAI **does** have a C/C++ `StreamingProcessor` API. It was introduced
in v0.12.2 (commit `dc3f30be`, March 2026). We are currently on v0.11.2, so
**step 0 is bumping ORT GenAI to ≥ 0.12.2**.

The API is in `ort_genai_c.h` / `ort_genai.h`:

```cpp
// C API
OgaResult* OgaCreateStreamingProcessor(OgaModel* model, OgaStreamingProcessor** out);
OgaResult* OgaStreamingProcessorProcess(OgaStreamingProcessor* processor,
                                         const float* audio_data, size_t num_samples,
                                         OgaNamedTensors** out);
OgaResult* OgaStreamingProcessorFlush(OgaStreamingProcessor* processor,
                                       OgaNamedTensors** out);
void       OgaDestroyStreamingProcessor(OgaStreamingProcessor* processor);
OgaResult* OgaStreamingProcessorSetOption(OgaStreamingProcessor* processor,
                                           const char* key, const char* value);
OgaResult* OgaStreamingProcessorGetOption(const OgaStreamingProcessor* processor,
                                           const char* key, const char** value);

// C++ wrapper (ort_genai.h)
struct OgaStreamingProcessor : OgaAbstract {
  static std::unique_ptr<OgaStreamingProcessor> Create(OgaModel& model);
  std::unique_ptr<OgaNamedTensors> Process(const float* audio_data, size_t num_samples);
  std::unique_ptr<OgaNamedTensors> Flush();
  void SetOption(const char* key, const char* value);
  OgaString GetOption(const char* key) const;
};
```

**Protocol** (from `NemotronStreamingProcessor` implementation):
1. `Process()` accumulates incoming float32 PCM in an internal buffer.
2. When the buffer reaches `chunk_samples` (model-specific, e.g. ~480k for
   30s at 16kHz), it computes mel spectrogram features and returns
   `NamedTensors`. Otherwise returns `nullptr`.
3. `Flush()` pads remaining buffered audio with silence to fill a chunk,
   processes it, and returns the final `NamedTensors` (or `nullptr` if empty).
4. Built-in VAD support (Voice Activity Detection) — can drop silence chunks.
   Configurable via `SetOption("use_vad", "true")`, `"vad_threshold"`,
   `"silence_duration_ms"`, `"prefix_padding_ms"`.

The tensors from `Process()`/`Flush()` are fed to a `Generator` via
`SetInputs()`, then tokens are decoded in the existing `IsDone()`/
`GenerateNextToken()` loop.

This gives us **true incremental transcription** — text flows back as audio
arrives, matching the C# behavior exactly.

### 5. AudioSession Changes

`ProcessRequestImpl` gains a third code path (in addition to AudioItem-with-URI
and JsonItem):

```
Request items[0] = AudioItem (format="pcm", sample_rate=16000, channels=1)
Request items[1] = ItemQueue (streaming PCM chunks as BytesItems)
```

Detection: scan `request.items` for an `ITEM_QUEUE`. If found alongside an
`AUDIO` item, enter the streaming path.

#### Streaming Path Pseudocode

```cpp
void AudioSession::ProcessStreamingAudio(const AudioItem& format_item,
                                         ItemQueue& queue,
                                         const Request& request,
                                         Response& response) {
  // 1. Validate format
  if (format_item.format != "pcm") {
    FL_LOG_AND_THROW(logger_, FOUNDRY_LOCAL_ERROR_INVALID_USAGE,
        "Streaming audio requires format 'pcm', got '" + format_item.format + "'");
  }
  if (format_item.sample_rate != 0 && format_item.sample_rate != 16000) {
    FL_LOG_AND_THROW(logger_, FOUNDRY_LOCAL_ERROR_INVALID_USAGE,
        "Streaming audio requires 16000 Hz sample rate, got "
        + std::to_string(format_item.sample_rate));
  }
  if (format_item.channels != 0 && format_item.channels != 1) {
    FL_LOG_AND_THROW(logger_, FOUNDRY_LOCAL_ERROR_INVALID_USAGE,
        "Streaming audio requires mono (1 channel), got "
        + std::to_string(format_item.channels));
  }

  // 2. Determine audio parameters from request options
  //    "language" (optional)

  // 3. Create ORT GenAI streaming pipeline
  auto& oga_model = Model().OgaModel();  // OgaModel from GenAIModelInstance
  auto processor = OgaStreamingProcessor::Create(oga_model);
  auto gen_params = std::make_unique<OgaGeneratorParams>(oga_model);
  auto generator = std::make_unique<OgaGenerator>(oga_model, *gen_params);
  auto tokenizer_stream = OgaTokenizerStream::Create(
      *OgaMultiModalProcessor::Create(oga_model));  // or from Tokenizer

  auto streaming_callback = CreateCallbackHandler(request);
  std::string full_text;

  // 4. Process PCM data from the queue incrementally
  //    If the AudioItem itself has initial data, process it first
  if (format_item.data && format_item.data_size > 0) {
    auto float_samples = ConvertS16LEToFloat(
        static_cast<const uint8_t*>(format_item.data), format_item.data_size);
    ProcessChunk(processor, generator, tokenizer_stream,
                 float_samples, full_text, streaming_callback, request);
  }

  // Read from queue until finished or cancelled.
  // WaitAndPop returns nullptr on timeout or finished — caller checks policy.
  while (!request.canceled) {
    auto item = queue.WaitAndPop(std::chrono::milliseconds(100));
    if (!item) {
      if (queue.IsFinished()) {
        break;  // producer is done
      }
      continue;  // timeout — re-check canceled, loop
    }

    if (item->type != FOUNDRY_LOCAL_ITEM_BYTES) {
      FL_LOG_AND_THROW(logger_, FOUNDRY_LOCAL_ERROR_INVALID_USAGE,
          "Streaming audio queue expects BYTES items, got item type " + std::to_string(item->type));
    }

    auto& bytes = static_cast<BytesItem&>(*item);
    auto float_samples = ConvertS16LEToFloat(
        static_cast<const uint8_t*>(bytes.data), bytes.data_size);

    // Feed to StreamingProcessor — may produce NamedTensors if a full
    // encoder chunk is ready, or nullptr if still accumulating
    ProcessChunk(processor, generator, tokenizer_stream,
                 float_samples, full_text, streaming_callback, request);
  }

  if (!request.canceled) {
    // 5. Flush remaining buffered audio
    auto flush_tensors = processor->Flush();
    if (flush_tensors) {
      generator->SetInputs(*flush_tensors);
      DecodeTokens(generator, tokenizer_stream, full_text,
                   streaming_callback, request);
    }
  }

  // 6. Produce response
  response.items.push_back(std::make_unique<TextItem>(std::move(full_text)));
  response.finish_reason = request.canceled ? FOUNDRY_LOCAL_FINISH_NONE
                                            : FOUNDRY_LOCAL_FINISH_STOP;
}

/// Feed float samples to StreamingProcessor. If a full chunk is ready,
/// set the tensors on the generator and decode tokens.
///
/// IMPORTANT: DecodeTokens must drain to IsDone() before the next SetInputs() call.
/// This is safe because the entire consume loop is single-threaded — WaitAndPop,
/// ProcessChunk, and DecodeTokens all run sequentially on the AudioSession thread.
/// The producer thread only pushes into the queue; it never touches the generator.
void AudioSession::ProcessChunk(OgaStreamingProcessor& processor,
                                OgaGenerator& generator,
                                OgaTokenizerStream& tokenizer_stream,
                                const std::vector<float>& samples,
                                std::string& full_text,
                                std::unique_ptr<CallbackHandler>& callback,
                                const Request& request) {
  auto tensors = processor.Process(samples.data(), samples.size());
  if (tensors) {
    generator.SetInputs(*tensors);
    DecodeTokens(generator, tokenizer_stream, full_text, callback, request);
  }
}

/// Decode all available tokens from the generator.
/// This MUST run to completion (IsDone() == true) before the next SetInputs() call.
/// The caller does not call SetInputs() again until this returns, which is safe
/// because the entire consume loop is single-threaded on the AudioSession thread.
void AudioSession::DecodeTokens(OgaGenerator& generator,
                                OgaTokenizerStream& tokenizer_stream,
                                std::string& full_text,
                                std::unique_ptr<CallbackHandler>& callback,
                                const Request& request) {
  while (!generator.IsDone() && !request.canceled) {
    generator.GenerateNextToken();
    auto* tokens = generator.GetOutput("output_ids");  // or GetNextTokens
    // Decode via TokenizerStream
    std::string text = tokenizer_stream.Decode(token_id);
    if (!text.empty()) {
      full_text += text;
      if (callback) {
        callback->PushItem(std::make_unique<TextItem>(text));
      }
    }
  }
}
```

This mirrors the C# `AudioStreamingSession` pipeline exactly:
- `AppendAudioChunk` → `ConvertS16LEToFloat` + `processor.Process()`
- `CommitTranscription` → `generator.SetInputs()` + `DecodeTokens()`
- `Flush` → `processor.Flush()` + `generator.SetInputs()` + `DecodeTokens()`

With this design, transcription text flows back via the streaming callback
**as audio arrives**, not just at the end.

### 6. PCM Conversion Utility

```cpp
// src/inferencing/generative/audio/pcm_utils.h

/// Convert 16-bit signed little-endian PCM bytes to float32 samples.
/// Each pair of bytes becomes one float in [-1.0, 1.0].
std::vector<float> ConvertS16LEToFloat(const std::vector<uint8_t>& pcm_bytes);
```

This mirrors the C# `AppendAudioChunk` conversion:
```csharp
short sample = BitConverter.ToInt16(pcmBytes, i * 2);
floatSamples[i] = sample / 32768.0f;
```

---

## C ABI Changes

### `flAudioData` fields for streaming PCM

`sample_rate` and `channels` are placed before the deleter fields in the
`flAudioData` struct (nothing has shipped yet, so no ABI compatibility
concern). The `SetAudio` / `GetAudio` C ABI functions read/write
all fields including these.

### No new C ABI functions needed

`WaitAndPop` is internal C++ only — `AudioSession` calls it directly.
External callers interact with `ItemQueue` via `Push` (already exposed).
No new session lifecycle APIs needed either (unlike the C# approach which
required `audio_stream_start`/`push`/`stop` commands across the FFI boundary).

Unlike the C# approach (which needed `audio_stream_start`/`push`/`stop` commands
because of the FFI boundary), the C++ API uses the existing `Session_ProcessRequest`
with the right items. No new session lifecycle APIs.

---

## File Changes Summary

| File | Change |
|------|--------|
| `cmake/FindOnnxRuntimeGenAI.cmake` | Bump `ORT_GENAI_VERSION` from `0.11.2` to `0.12.2` (or later) |
| `include/foundry_local/foundry_local_c.h` | Add `sample_rate`/`channels` to `flAudioData` |
| `src/items/audio_item.h` | Add `sample_rate`, `channels` fields; update `SetAudioData`/`GetApiData` |
| `src/items/item_queue.h` | Add `condition_variable`, `WaitAndPop()`, update `Push()`/`MarkFinished()` |
| `src/inferencing/generative/audio/pcm_utils.h` | **New** — PCM s16le → float32 conversion |
| `src/inferencing/generative/audio/pcm_utils.cc` | **New** — implementation |
| `src/c_api/item_api.cc` | Update `SetAudio`/`GetAudio` for `sample_rate`/`channels` |
| `src/inferencing/generative/audio/audio_session.h` | Add `ProcessStreamingAudio()`, `ProcessChunk()`, `DecodeTokens()` declarations |
| `src/inferencing/generative/audio/audio_session.cc` | Add streaming path in `ProcessRequestImpl`, implement streaming methods |
| `test/internal_api/audio/audio_item_test.cc` | Add sample_rate/channels round-trip tests |
| `test/internal_api/audio/audio_session_test.cc` | Add streaming audio tests |
| `test/internal_api/items/item_queue_test.cc` | Add `WaitAndPop` tests |

---

## Test Plan

### Unit Tests (no model needed)

1. **AudioItem sample_rate/channels round-trip**
   - Set via `flAudioData`, read back via `GetApiData`
   - Defaults are 0 (unspecified) when not set

2. **ItemQueue WaitAndPop**
   - Returns item immediately when queue non-empty
   - Blocks and returns item after Push from another thread
   - Returns nullptr when MarkFinished and queue empty
   - Returns nullptr when canceled flag set
   - Returns remaining items before nullptr when MarkFinished with items in queue
   - Handles timeout correctly (returns nullptr, then gets item after push)

3. **PCM Conversion**
   - Known s16le bytes → expected float values
   - Silence (all zeros) → all zeros
   - Max positive/negative → ±1.0 (approximately)
   - Odd byte count handling (truncate trailing byte)

4. **AudioSession streaming input validation**
   - Request with ItemQueue but no AudioItem → error
   - Request with AudioItem + ItemQueue but unsupported format → error
   - Request with empty queue (immediate MarkFinished) → empty transcription

### Integration Tests (model needed — nemotron)

5. **Streaming transcription with synthetic PCM**
   - Generate a 440Hz sine wave as 16-bit PCM
   - Push in chunks (e.g., 100ms each = 3200 bytes at 16kHz mono 16-bit)
   - MarkFinished, verify response has text output
   - Verify `finish_reason == STOP`

6. **Streaming transcription from Recording.mp3 loaded as PCM**
   - Load the existing test recording, decode to PCM
   - Push through ItemQueue
   - Verify transcription produces text

7. **Cancellation mid-stream**
   - Start pushing chunks
   - Set `request.canceled = true` before MarkFinished
   - Verify session exits cleanly with `FINISH_NONE`

8. **Streaming callback (token-by-token output)**
   - Set streaming callback on session
   - Push audio through ItemQueue
   - Verify callback receives incremental text tokens

### C ABI Tests

9. **SetBytes / GetBytes round-trip**

---

## Implementation Order

| Step | Description | Depends On |
|------|-------------|------------|
| 0 | Bump ORT GenAI to ≥ 0.12.2 in `FindOnnxRuntimeGenAI.cmake` | — |
| 1 | `AudioItem` fields (`sample_rate`, `channels`) + C ABI | — |
| 2 | `ItemQueue::WaitAndPop()` + condition variable (internal only) | — |
| 3 | `pcm_utils` (s16le → float32) | — |
| 4 | Unit tests for steps 1–3 | 1–3 |
| 5 | `AudioSession` streaming path using `OgaStreamingProcessor` | 0–3 |
| 6 | AudioSession unit tests (validation, empty queue) | 5 |
| 7 | AudioSession integration tests (synthetic PCM, recording) | 5 |
| 8 | C ABI integration test for streaming audio | 2, 5 |

Steps 0–3 are independent and can be done in parallel.

---

## Open Questions

1. **ORT GenAI version compatibility.** We need ≥ 0.12.2 for `OgaStreamingProcessor`.
   Check that the Foundry-specific NuGet (`Microsoft.ML.OnnxRuntimeGenAI.Foundry`)
   publishes a 0.12.2 build. If not, we may need to use the standard package or
   build from source temporarily.

2. **Sample rate resampling.** `OgaStreamingProcessor::Process()` expects float32
   PCM at the model's native sample rate (16kHz for Nemotron). Verify whether it
   does internal resampling or if we must enforce 16kHz input.
   **Recommendation:** Require 16kHz mono for v1. Validate in `AudioSession` via
   `AudioItem.sample_rate` and `AudioItem.channels`. Resampling is a follow-up.

3. **VAD configuration.** `OgaStreamingProcessor` supports built-in Voice Activity
   Detection via `SetOption()`. Expose VAD options through request options? Or
   leave as a session-level configuration?
