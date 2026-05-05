// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Foundry Local SDK - WinML 2.0 EP Verification (C++)
//
// Verifies:
//   1. Execution providers are discovered and registered.
//   2. Accelerated models appear in the catalog after EP registration.
//   3. Streaming chat completions work on an accelerated model.

#include "foundry_local.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view PASS = "[PASS]";
constexpr std::string_view FAIL = "[FAIL]";
constexpr std::string_view INFO = "[INFO]";
constexpr std::string_view WARN = "[WARN]";

class StdLogger final : public foundry_local::ILogger {
public:
    void Log(foundry_local::LogLevel level, std::string_view message) noexcept override {
        if (level == foundry_local::LogLevel::Warning) {
            std::cout << "[FoundryLocal][WARN] " << message << '\n';
        } else if (level == foundry_local::LogLevel::Error) {
            std::cout << "[FoundryLocal][ERROR] " << message << '\n';
        }
    }
};

struct TestResults {
    std::vector<std::pair<std::string, bool>> results;

    void Add(std::string name, bool passed, const std::string& detail = {}) {
        std::cout << (passed ? PASS : FAIL) << ' ' << name;
        if (!detail.empty()) {
            std::cout << " - " << detail;
        }
        std::cout << '\n';
        results.emplace_back(std::move(name), passed);
    }

    void PrintSummary() const {
        PrintSeparator("Summary");
        auto passed = std::count_if(results.begin(), results.end(), [](const auto& result) {
            return result.second;
        });

        for (const auto& [name, ok] : results) {
            std::cout << "  " << (ok ? "PASS " : "FAIL ") << name << '\n';
        }

        std::cout << "\n  " << passed << '/' << results.size() << " tests passed\n";
    }

    bool AllPassed() const {
        return !results.empty() &&
            std::all_of(results.begin(), results.end(), [](const auto& result) {
                return result.second;
            });
    }

    static void PrintSeparator(std::string_view title) {
        std::cout << "\n" << std::string(60, '=') << '\n';
        std::cout << "  " << title << '\n';
        std::cout << std::string(60, '=') << "\n\n";
    }
};

struct Candidate {
    foundry_local::IModel* model = nullptr;
    foundry_local::ModelInfo info;
};

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string DeviceTypeName(foundry_local::DeviceType deviceType) {
    switch (deviceType) {
    case foundry_local::DeviceType::CPU:
        return "CPU";
    case foundry_local::DeviceType::GPU:
        return "GPU";
    case foundry_local::DeviceType::NPU:
        return "NPU";
    default:
        return "?";
    }
}

bool IsAcceleratedVariant(const foundry_local::ModelInfo& info) {
    if (!info.runtime) {
        return false;
    }

    return info.runtime->device_type == foundry_local::DeviceType::GPU ||
        info.runtime->device_type == foundry_local::DeviceType::NPU;
}

int VariantScore(const foundry_local::ModelInfo& info) {
    const auto id = ToLower(info.id);
    auto score = info.runtime && info.runtime->device_type == foundry_local::DeviceType::NPU ? 10000 : 0;

    if (id.find("whisper") != std::string::npos) {
        score += 5000;
    }
    if (id.find("reasoning") != std::string::npos ||
        id.find("deepseek-r1") != std::string::npos ||
        id.find("gpt-oss") != std::string::npos) {
        score += 2000;
    }

    if (id.find("0.5b") != std::string::npos) {
        score += 0;
    } else if (id.find("1.5b") != std::string::npos) {
        score += 100;
    } else if (id.find("3b") != std::string::npos) {
        score += 300;
    } else if (id.find("7b") != std::string::npos) {
        score += 700;
    } else if (id.find("14b") != std::string::npos) {
        score += 1400;
    } else if (id.find("20b") != std::string::npos) {
        score += 2000;
    } else {
        score += 500;
    }

    return score;
}

std::vector<Candidate> FindAcceleratedVariants(foundry_local::Catalog& catalog) {
    std::vector<Candidate> candidates;

    for (const auto* modelBase : catalog.ListModels()) {
        const auto* model = dynamic_cast<const foundry_local::Model*>(modelBase);
        if (!model) {
            continue;
        }

        for (const auto& variant : model->GetAllModelVariants()) {
            const auto& info = variant.GetInfo();
            if (!IsAcceleratedVariant(info)) {
                continue;
            }

            auto* candidateModel = catalog.GetModelVariant(variant.GetId());
            if (!candidateModel) {
                continue;
            }

            candidates.push_back(Candidate{candidateModel, info});
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        return VariantScore(lhs.info) < VariantScore(rhs.info);
    });

    return candidates;
}

} // namespace

int main() {
    TestResults results;
    StdLogger logger;
    foundry_local::IModel* chosen = nullptr;

    try {
        TestResults::PrintSeparator("Initialization");
        foundry_local::Configuration config{"verify_winml"};
        config.log_level = foundry_local::LogLevel::Information;

        foundry_local::Manager::Create(config, &logger);
        auto& manager = foundry_local::Manager::Instance();
        std::cout << INFO << " FoundryLocalManager initialized.\n";

        TestResults::PrintSeparator("Step 1: Discover & Register Execution Providers");
        std::vector<foundry_local::EpInfo> eps;
        try {
            eps = manager.DiscoverEps();
            std::cout << INFO << " Discovered " << eps.size() << " execution providers:\n";
            for (const auto& ep : eps) {
                std::cout << "  - " << std::left << std::setw(40) << ep.name
                          << "  Registered: " << (ep.is_registered ? "true" : "false") << '\n';
            }
            results.Add("EP Discovery", true, std::to_string(eps.size()) + " EP(s) found");
        } catch (const std::exception& e) {
            results.Add("EP Discovery", false, e.what());
        }

        if (eps.empty()) {
            const std::string detail = "No execution providers discovered on this machine";
            results.Add("EP Download & Registration", false, detail);
            std::cout << '\n' << FAIL << ' ' << detail << ".\n";
            results.PrintSummary();
            foundry_local::Manager::Destroy();
            return 1;
        }

        try {
            std::string currentProgressEp;
            auto currentProgressPercent = -1.0;

            auto epResult = manager.DownloadAndRegisterEps(
                [&](std::string_view epName, double percent) {
                    if (!currentProgressEp.empty() &&
                        (currentProgressEp != epName || percent < currentProgressPercent)) {
                        std::cout << '\n';
                    }

                    currentProgressEp = std::string(epName);
                    currentProgressPercent = percent;
                    std::cout << "\r  Downloading " << currentProgressEp << ": "
                              << std::fixed << std::setprecision(1) << percent << '%' << std::flush;
                });

            if (!currentProgressEp.empty()) {
                std::cout << '\n';
            }

            std::cout << INFO << " EP registration: success=" << (epResult.success ? "true" : "false")
                      << ", status=" << epResult.status << '\n';
            if (!epResult.registered_eps.empty()) {
                std::cout << "  Registered:";
                for (const auto& name : epResult.registered_eps) {
                    std::cout << ' ' << name;
                }
                std::cout << '\n';
            }
            if (!epResult.failed_eps.empty()) {
                std::cout << "  Failed:";
                for (const auto& name : epResult.failed_eps) {
                    std::cout << ' ' << name;
                }
                std::cout << '\n';
            }

            auto detail = epResult.success && !epResult.registered_eps.empty()
                ? std::to_string(epResult.registered_eps.size()) + " EP(s) registered"
                : epResult.status;
            results.Add("EP Download & Registration", epResult.success, detail);
            if (!epResult.success) {
                results.PrintSummary();
                foundry_local::Manager::Destroy();
                return 1;
            }
        } catch (const std::exception& e) {
            std::cout << '\n';
            results.Add("EP Download & Registration", false, e.what());
            results.PrintSummary();
            foundry_local::Manager::Destroy();
            return 1;
        }

        TestResults::PrintSeparator("Step 2: Model Catalog - Accelerated Models");
        auto& catalog = manager.GetCatalog();
        auto models = catalog.ListModels();
        auto acceleratedVariants = FindAcceleratedVariants(catalog);

        std::cout << INFO << " Total models in catalog: " << models.size() << '\n';
        for (const auto& candidate : acceleratedVariants) {
            const auto& runtime = *candidate.info.runtime;
            std::cout << "  - " << std::left << std::setw(50) << candidate.info.id
                      << "  Device: " << std::setw(3) << DeviceTypeName(runtime.device_type)
                      << "  EP: " << runtime.execution_provider << '\n';
        }

        results.Add("Catalog - Accelerated models found", !acceleratedVariants.empty(),
                    acceleratedVariants.empty()
                        ? "No accelerated model variants"
                        : std::to_string(acceleratedVariants.size()) + " accelerated variant(s)");
        if (acceleratedVariants.empty()) {
            std::cout << '\n' << FAIL << " No accelerated model variants are available.\n";
            std::cout << WARN << " Ensure the system has a compatible accelerator and matching model variants installed.\n";
            results.PrintSummary();
            foundry_local::Manager::Destroy();
            return 1;
        }

        TestResults::PrintSeparator("Step 3: Download & Load Model");
        bool downloadedAny = false;
        std::string lastLoadError;

        for (const auto& candidate : acceleratedVariants) {
            const auto& ep = candidate.info.runtime ? candidate.info.runtime->execution_provider : "unknown";
            std::cout << '\n' << INFO << " Trying model: " << candidate.info.id << " (EP: " << ep << ")\n";

            try {
                candidate.model->Download([](float progress) {
                    std::cout << "\r  Downloading model: " << std::fixed << std::setprecision(1)
                              << progress << '%' << std::flush;
                });
                std::cout << '\n';
                downloadedAny = true;
            } catch (const std::exception& e) {
                std::cout << '\n' << WARN << " Skipping " << candidate.info.id
                          << ": download failed: " << e.what() << '\n';
                lastLoadError = e.what();
                continue;
            }

            try {
                candidate.model->Load();
                chosen = candidate.model;
                break;
            } catch (const std::exception& e) {
                std::cout << WARN << " Skipping " << candidate.info.id
                          << ": load failed: " << e.what() << '\n';
                lastLoadError = e.what();
            }
        }

        results.Add("Model Download", downloadedAny,
                    downloadedAny ? "At least one accelerated variant downloaded"
                                  : (lastLoadError.empty() ? "No accelerated variant could be downloaded" : lastLoadError));

        if (!chosen) {
            results.Add("Model Load", false,
                        lastLoadError.empty() ? "No accelerated variant could be loaded on this machine" : lastLoadError);
            results.PrintSummary();
            foundry_local::Manager::Destroy();
            return 1;
        }

        results.Add("Model Load", true, "Loaded " + chosen->GetId());

        TestResults::PrintSeparator("Step 4: Streaming Chat Completions");
        try {
            foundry_local::OpenAIChatClient chat(*chosen);
            std::vector<foundry_local::ChatMessage> messages = {
                {"system", "You are a helpful assistant."},
                {"user", "What is 2 + 2? Reply with just the number."},
            };
            foundry_local::ChatSettings settings;
            settings.temperature = 0.0f;
            settings.max_tokens = 16;

            std::string fullResponse;
            const auto start = std::chrono::steady_clock::now();
            chat.CompleteChatStreaming(messages, settings, [&](const foundry_local::ChatCompletionCreateResponse& chunk) {
                if (chunk.choices.empty()) {
                    return;
                }

                const auto& choice = chunk.choices[0];
                if (choice.delta && !choice.delta->content.empty()) {
                    std::cout << choice.delta->content << std::flush;
                    fullResponse += choice.delta->content;
                }
            });
            const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            std::cout << '\n';

            results.Add("Streaming Chat", !fullResponse.empty(),
                        std::to_string(fullResponse.size()) + " chars in " + std::to_string(elapsed) + "s");
        } catch (const std::exception& e) {
            results.Add("Streaming Chat", false, e.what());
        }

        try {
            chosen->Unload();
            std::cout << INFO << " Model unloaded.\n";
        } catch (const std::exception& e) {
            std::cout << WARN << " Failed to unload model: " << e.what() << '\n';
        }

        results.PrintSummary();
        foundry_local::Manager::Destroy();
        return results.AllPassed() ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << FAIL << " " << e.what() << '\n';
        foundry_local::Manager::Destroy();
        return 1;
    }
}
