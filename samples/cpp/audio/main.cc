// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Sample: Live / file audio transcription with the Foundry Local C++ SDK (sdk_v2/cpp).
//
// Two transcription paths, matching the other-language audio samples:
//   * Live microphone  -> Nemotron streaming ASR (incremental, real-time).
//   * File             -> Whisper (whole-file, non-streaming).
//
// The live path mirrors sdk_v2/cpp/examples/realtime_audio: a streaming AudioSession
// receives PCM through an ItemQueue and emits incremental text via a streaming
// callback. The file path submits a single AUDIO item (file URI) and reads the
// transcript from the response.
//
// Modes:
//   (default)      Live microphone capture via PortAudio (compile-time optional,
//                  behind HAS_PORTAUDIO). Falls back to Whisper transcription of the
//                  bundled WAV if PortAudio is unavailable.
//   --file [path]  Transcribe an audio file with Whisper. With no path, uses the
//                  bundled Recording.wav.
//   --synth        Stream a generated 440 Hz sine tone through the Nemotron model.
//
// The Nemotron streaming model expects 16 kHz mono PCM.

#include <foundry_local/foundry_local_cpp.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// PortAudio is optional: the CMake build defines HAS_PORTAUDIO and links the
// library only when it is found, so the sample also builds without a mic stack.
#ifdef HAS_PORTAUDIO
#include <portaudio.h>
#endif

using namespace foundry_local;

namespace {

constexpr const char* kStreamingModel = "nemotron-speech-streaming-en-0.6b";  // live mic / synthetic PCM
constexpr const char* kWhisperModel = "whisper-tiny";                          // file-based transcription
constexpr int kSampleRate = 16000;
constexpr int kChannels = 1;

// Set to false by Ctrl+C to request a graceful stop of live capture.
std::atomic<bool> g_running{true};

void HandleSigint(int /*signum*/) {
  g_running = false;
}

/// Resolve a catalog model by alias, download it if needed, and load it.
std::unique_ptr<IModel> LoadModel(Manager& manager, const std::string& alias) {
  auto model = manager.GetCatalog().GetModel(alias);
  if (!model) {
    throw std::runtime_error("Model '" + alias + "' not found in catalog.");
  }

  std::cout << "Using model: " << model->GetInfo().Name() << "\n";

  if (!model->IsCached()) {
    std::cout << "Downloading...\n";
    model->Download([](float progress) -> int {
      std::cout << "\r  " << static_cast<int>(progress) << "%" << std::flush;
      return 0;  // return non-zero to cancel
    });
    std::cout << "\n";
  }

  if (!model->IsLoaded()) {
    std::cout << "Loading model...\n";
    model->Load();
  }

  return model;
}

/// A producer pushes PCM into the session's ItemQueue, then returns.
/// RunSession marks the queue finished once the producer returns.
using Producer = std::function<void(ItemQueue&)>;

/// Drive an AudioSession: stream PCM from `produce` into the session and print
/// transcribed text incrementally via the streaming callback.
void RunSession(IModel& model, int sample_rate, int channels, const Producer& produce) {
  AudioSession session(model);

  // The streaming callback receives one item per invocation; print TEXT items as they arrive.
  session.SetStreamingCallback([](flStreamingCallbackData event) -> int {
    const auto* item_api = detail::item_api();

    flItem* raw_item = nullptr;
    if (item_api->ItemQueue_TryPop(event.item_queue, &raw_item)) {
      Item item(*raw_item);
      if (item.GetType() == FOUNDRY_LOCAL_ITEM_TEXT) {
        std::cout << item.GetText().text << std::flush;
      }
    }

    return 0;  // return non-zero to cancel
  });

  // Queue that carries streamed audio chunks. Added to the request without
  // transferring ownership so the producer thread can keep pushing into it.
  ItemQueue audio_input;

  Request request;
  request.AddItem(Item::AudioFromData("pcm", nullptr, 0, sample_rate, channels));  // format descriptor
  request.AddItem(audio_input, /*take_ownership*/ false);

  std::thread producer([&] {
    try {
      produce(audio_input);
    } catch (const std::exception& ex) {
      std::cerr << "\nAudio producer error: " << ex.what() << "\n";
    }

    audio_input.MarkFinished();
  });

  std::cout << "Transcription: ";
  Response response = [&]() -> Response {
    try {
      return session.ProcessRequest(request);
    } catch (...) {
      // Signal + join the producer before propagating so it never outlives `audio_input`.
      audio_input.MarkFinished();
      if (producer.joinable()) {
        producer.join();
      }

      throw;
    }
  }();
  std::cout << "\n";

  producer.join();

  const flUsage usage = response.GetUsage();
  std::cout << "Tokens — prompt: " << usage.prompt_tokens << ", completion: " << usage.completion_tokens
            << ", total: " << usage.total_tokens << "\n";
}

/// Produce non-owning chunks that slice a long-lived PCM buffer (file / synthetic modes).
/// `pcm` must outlive the RunSession call — BYTES items reference its storage, they do not copy.
Producer StreamBuffer(const std::vector<uint8_t>& pcm) {
  return [&pcm](ItemQueue& queue) {
    constexpr size_t kChunkSize = 4096;
    size_t offset = 0;
    while (offset < pcm.size() && g_running) {
      const size_t chunk_size = std::min(kChunkSize, pcm.size() - offset);
      queue.Push(Item::Bytes(FOUNDRY_LOCAL_ITEM_BYTES, pcm.data() + offset, chunk_size));
      offset += chunk_size;

      // Pace the stream to roughly real time so the demo resembles live audio.
      std::this_thread::sleep_for(std::chrono::milliseconds(125));
    }
  };
}

/// Generate `seconds` of 16-bit mono PCM for a sine tone at `frequency_hz`.
std::vector<uint8_t> GenerateSinePcm(int sample_rate, int seconds, double frequency_hz) {
  const auto total_samples = static_cast<size_t>(sample_rate) * static_cast<size_t>(seconds);
  std::vector<uint8_t> pcm(total_samples * 2, 0);
  for (size_t i = 0; i < total_samples; ++i) {
    const double t = static_cast<double>(i) / sample_rate;
    const auto sample = static_cast<int16_t>(0.5 * INT16_MAX * std::sin(2.0 * M_PI * frequency_hz * t));
    const auto encoded = static_cast<uint16_t>(sample);
    pcm[i * 2] = static_cast<uint8_t>(encoded & 0xFF);
    pcm[i * 2 + 1] = static_cast<uint8_t>((encoded >> 8) & 0xFF);
  }

  return pcm;
}

#ifdef HAS_PORTAUDIO

/// Bounded, thread-safe queue of captured PCM chunks (drops oldest on overflow).
class CaptureQueue {
 public:
  void Push(std::vector<uint8_t> chunk) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.size() >= kMaxSize) {
      queue_.pop_front();
    }

    queue_.push_back(std::move(chunk));
  }

  bool TryPop(std::vector<uint8_t>& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
      return false;
    }

    out = std::move(queue_.front());
    queue_.pop_front();
    return true;
  }

 private:
  static constexpr size_t kMaxSize = 100;
  std::deque<std::vector<uint8_t>> queue_;
  std::mutex mutex_;
};

/// PortAudio callback: copy 16-bit mono PCM into the capture queue.
int PaCapture(const void* input, void* /*output*/, unsigned long frame_count,
              const PaStreamCallbackTimeInfo* /*time_info*/, PaStreamCallbackFlags /*flags*/, void* user_data) {
  auto* queue = static_cast<CaptureQueue*>(user_data);
  const auto* bytes = static_cast<const uint8_t*>(input);
  if (bytes != nullptr) {
    const size_t byte_count = static_cast<size_t>(frame_count) * 2;  // 16-bit mono
    queue->Push(std::vector<uint8_t>(bytes, bytes + byte_count));
  }

  return g_running ? paContinue : paComplete;
}

/// Producer that captures live microphone PCM and streams it into the session.
/// Returns false if the microphone could not be opened (so the caller can fall back).
bool TryRunMic(IModel& model) {
  if (Pa_Initialize() != paNoError) {
    return false;
  }

  CaptureQueue capture;
  PaStream* stream = nullptr;

  PaStreamParameters input_params{};
  input_params.device = Pa_GetDefaultInputDevice();
  if (input_params.device == paNoDevice) {
    Pa_Terminate();
    return false;
  }

  input_params.channelCount = kChannels;
  input_params.sampleFormat = paInt16;
  input_params.suggestedLatency = Pa_GetDeviceInfo(input_params.device)->defaultLowInputLatency;
  input_params.hostApiSpecificStreamInfo = nullptr;

  PaError err = Pa_OpenStream(&stream, &input_params, nullptr, kSampleRate, 3200, paClipOff, PaCapture, &capture);
  if (err == paNoError) {
    err = Pa_StartStream(stream);
  }

  if (err != paNoError) {
    if (stream != nullptr) {
      Pa_CloseStream(stream);
    }

    Pa_Terminate();
    return false;
  }

  std::cout << "\n=== LIVE TRANSCRIPTION ACTIVE — speak into your microphone (Ctrl+C to stop) ===\n";

  // Each captured chunk is moved into an owning BYTES item: the item holds the
  // buffer (via a deleter) for as long as the session needs it, so chunks can
  // arrive dynamically without a single long-lived backing buffer.
  RunSession(model, kSampleRate, kChannels, [&capture](ItemQueue& queue) {
    while (g_running) {
      std::vector<uint8_t> chunk;
      if (capture.TryPop(chunk)) {
        auto* held = new std::vector<uint8_t>(std::move(chunk));
        queue.Push(Item::Bytes(FOUNDRY_LOCAL_ITEM_BYTES, held->data(), held->size(),
                               [held](const flBytesData*) { delete held; }));
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
  });

  Pa_StopStream(stream);
  Pa_CloseStream(stream);
  Pa_Terminate();
  return true;
}

#endif  // HAS_PORTAUDIO

/// File-based transcription with the Whisper model (native, non-streaming).
/// A single AUDIO item carrying the file URI drives transcription; the SDK reads
/// and decodes the file and returns the full transcript as a TEXT item.
void RunFile(Manager& manager, const std::string& path) {
  std::cout << "\n=== FILE TRANSCRIPTION (Whisper) ===\n";
  auto model = LoadModel(manager, kWhisperModel);

  AudioSession session(*model);

  Request request;
  request.AddItem(Item::AudioFromUri(path));

  std::cout << "Transcribing: " << path << "\n";
  Response response = session.ProcessRequest(request);

  std::cout << "Transcription: ";
  for (const auto& item : response.GetItems()) {
    if (item.GetType() == FOUNDRY_LOCAL_ITEM_TEXT) {
      std::cout << item.GetText().text;
    }
  }
  std::cout << "\n";

  const flUsage usage = response.GetUsage();
  std::cout << "Tokens — prompt: " << usage.prompt_tokens << ", completion: " << usage.completion_tokens
            << ", total: " << usage.total_tokens << "\n";

  model->Unload();
}

/// Stream a generated sine tone through the Nemotron streaming model.
void RunSynth(Manager& manager) {
  std::cout << "\n=== SYNTHETIC TONE (Nemotron streaming) ===\n";
  auto model = LoadModel(manager, kStreamingModel);

  std::cout << "Synthetic 440 Hz sine tone (2 s).\n";
  const std::vector<uint8_t> pcm = GenerateSinePcm(kSampleRate, 2, 440.0);
  RunSession(*model, kSampleRate, kChannels, StreamBuffer(pcm));

  model->Unload();
}

struct Options {
  bool use_file = false;
  bool use_synth = false;
  std::string file_path;
};

Options ParseArgs(int argc, char* argv[]) {
  Options opts;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--file") {
      opts.use_file = true;
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        opts.file_path = argv[++i];
      }
    } else if (arg == "--synth") {
      opts.use_synth = true;
    }
  }

  return opts;
}

}  // namespace

int main(int argc, char* argv[]) {
  const Options opts = ParseArgs(argc, argv);
  const std::string bundled_wav = (std::filesystem::path(SAMPLE_SOURCE_DIR) / "Recording.wav").string();

  std::signal(SIGINT, HandleSigint);

  try {
    Configuration config("foundry_local_samples");
    Manager manager(std::move(config));

    // Mode selection: explicit --synth / --file win; otherwise try the live mic,
    // falling back to Whisper transcription of the bundled WAV.
    if (opts.use_synth) {
      RunSynth(manager);
    } else if (opts.use_file) {
      RunFile(manager, opts.file_path.empty() ? bundled_wav : opts.file_path);
    } else {
#ifdef HAS_PORTAUDIO
      auto model = LoadModel(manager, kStreamingModel);
      const bool mic_ran = TryRunMic(*model);
      model->Unload();

      if (!mic_ran) {
        std::cout << "Microphone unavailable — falling back to Whisper file transcription.\n";
        RunFile(manager, bundled_wav);
      }
#else
      std::cout << "Built without PortAudio — transcribing the bundled WAV with Whisper.\n";
      std::cout << "(Pass --file <path> for another file, or --synth for a generated tone.)\n";
      RunFile(manager, bundled_wav);
#endif
    }
  } catch (const Error& ex) {
    std::cerr << "Foundry Local error [" << ex.Code() << "]: " << ex.what() << "\n";
    return 1;
  } catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }

  return 0;
}
