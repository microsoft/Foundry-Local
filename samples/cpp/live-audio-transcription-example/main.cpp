// Live Audio Transcription — Foundry Local C++ SDK Example
//
// NOTE: The live-transcription session API (CreateLiveTranscriptionSession)
// is not yet available in the C++ SDK. This sample is a forward-looking
// reference based on the API surface proposed in PR #655.
//
// APIs used:
//   - OpenAIAudioClient::CreateLiveTranscriptionSession()
//   - LiveAudioTranscriptionSession::{Start, Append, TryGetNext, Stop}

#include <chrono>
#include <climits>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "foundry_local.h"

namespace {
std::vector<uint8_t> GenerateSineWavePcm(int sampleRate, int durationSeconds, double frequencyHz) {
    const auto totalSamples = static_cast<size_t>(sampleRate * durationSeconds);
    std::vector<uint8_t> pcm(totalSamples * 2, 0); // 16-bit mono, little-endian

    for (size_t i = 0; i < totalSamples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(sampleRate);
        const auto sample = static_cast<int16_t>(
            static_cast<double>(INT16_MAX) * 0.5 * std::sin(2.0 * 3.14159265358979323846 * frequencyHz * t));
        const auto encodedSample = static_cast<uint16_t>(sample);
        pcm[i * 2] = static_cast<uint8_t>(encodedSample & 0xFF);
        pcm[i * 2 + 1] = static_cast<uint8_t>((encodedSample >> 8) & 0xFF);
    }
    return pcm;
}
} // namespace

int main() {
    try {
        // Manager/model bootstrapping follows the same pattern as other Foundry Local SDK samples.
        foundry_local::Configuration config;
        config.appName = "foundry_local_samples";

        auto manager = foundry_local::FoundryLocalManager::Create(config);
        auto catalog = manager->GetCatalog();
        auto model = catalog.GetModel("nemotron");
        if (!model) {
            throw std::runtime_error("Model \"nemotron\" not found in catalog");
        }

        model->Download();
        model->Load();

        auto audioClient = model->GetAudioClient();
        auto session = audioClient.CreateLiveTranscriptionSession();

        session->Settings().sample_rate = 16000;
        session->Settings().channels = 1;
        session->Settings().bits_per_sample = 16;
        session->Settings().language = "en";
        session->Start();

        std::cout << "Session started. Pushing synthetic audio..." << std::endl;
        const auto pcm = GenerateSineWavePcm(16000, 3, 440.0);
        const size_t chunkSize = static_cast<size_t>(16000 / 10 * 2); // 100ms
        for (size_t offset = 0; offset < pcm.size(); offset += chunkSize) {
            const size_t len = std::min(chunkSize, pcm.size() - offset);
            session->Append(pcm.data() + offset, len);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        foundry_local::LiveAudioTranscriptionResponse result;
        int consecutiveTimeouts = 0;
        const int maxConsecutiveTimeouts = 10; // 5 seconds of silence
        while (true) {
            const auto status = session->TryGetNext(result, std::chrono::milliseconds(500));
            if (status == foundry_local::TranscriptionStatus::Result) {
                consecutiveTimeouts = 0;
                if (result.is_final) {
                    std::cout << "\n[FINAL] " << result.text << std::endl;
                } else {
                    std::cout << result.text << std::flush;
                }
            } else if (status == foundry_local::TranscriptionStatus::Timeout) {
                if (++consecutiveTimeouts >= maxConsecutiveTimeouts) {
                    break; // No more results after extended wait
                }
                continue; // Engine may still be processing buffered audio
            } else if (status == foundry_local::TranscriptionStatus::Closed) {
                break;
            } else {
                std::cerr << "Transcription stream error: " << session->GetErrorMessage() << std::endl;
                break;
            }
        }

        session->Stop();
        model->Unload();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}
