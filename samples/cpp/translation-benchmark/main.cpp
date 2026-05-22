// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Translation benchmark: runs 20 zh<->en translations on WebGPU and CPU
// variants, then prints a comparison table.

#include "foundry_local.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace foundry_local;

// ---------------------------------------------------------------------------
// Logger (quiet — only errors)
// ---------------------------------------------------------------------------
class QuietLogger final : public ILogger {
public:
    void Log(LogLevel level, std::string_view message) noexcept override {
        if (level == LogLevel::Error) {
            std::cerr << "[ERROR] " << message << "\n";
        }
    }
};

// ---------------------------------------------------------------------------
// Translation item
// ---------------------------------------------------------------------------
struct TranslationItem {
    int id;
    std::string direction;   // "zh->en" or "en->zh"
    std::string source;
};

// ---------------------------------------------------------------------------
// Translate a single text using a loaded model
// ---------------------------------------------------------------------------
std::string Translate(OpenAIChatClient& chat,
                      const std::string& direction,
                      const std::string& sourceText) {
    std::string systemPrompt;
    std::string userPrompt;
    if (direction == "zh->en") {
        systemPrompt = "You are a translation assistant. Translate the user's Chinese text to English. "
                       "Respond with only the translated text, no explanations.";
        userPrompt = "Translate the following Chinese to English: " + sourceText;
    } else {
        systemPrompt = "You are a translation assistant. Translate the user's English text to Chinese. "
                       "Respond with only the translated Chinese text, no explanations.";
        userPrompt = "Translate the following English to Chinese: " + sourceText;
    }

    std::vector<ChatMessage> messages = {
        {"system", systemPrompt},
        {"user", userPrompt}
    };

    ChatSettings settings;
    settings.temperature = 0.0f;
    settings.max_tokens = 100;

    auto response = chat.CompleteChat(messages, settings);

    if (!response.choices.empty() && response.choices[0].message) {
        return response.choices[0].message->content;
    }
    return "(no response)";
}

// ---------------------------------------------------------------------------
// Find a variant matching a given execution provider name (case-insensitive)
// ---------------------------------------------------------------------------
const ModelVariant* FindVariant(const Model& model, const std::string& epName) {
    auto toLower = [](std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    std::string target = toLower(epName);
    for (const auto& v : model.GetVariants()) {
        const auto& info = v.GetInfo();
        if (!info.runtime) continue;
        // Substring match: "cuda" matches "CUDAExecutionProvider"
        std::string ep = toLower(info.runtime->execution_provider);
        if (ep.find(target) != std::string::npos) return &v;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Run all translations on one variant
// ---------------------------------------------------------------------------
std::vector<std::string> RunTranslations(Model& model,
                                         const ModelVariant& variant,
                                         const std::vector<TranslationItem>& items,
                                         const std::string& label) {
    model.SelectVariant(variant);

    std::cout << "Downloading " << label << " variant...\n";
    model.Download([&](float pct) {
        printf("\r  %s: %5.1f%%", label.c_str(), pct);
        fflush(stdout);
        return true;
    });
    std::cout << "\n";

    std::cout << "Loading " << label << " variant...\n";
    model.Load();

    if (!model.IsLoaded()) {
        std::cerr << "Failed to load " << label << " variant.\n";
        return std::vector<std::string>(items.size(), "(load failed)");
    }

    OpenAIChatClient chat(model);
    std::vector<std::string> results;
    results.reserve(items.size());

    for (const auto& item : items) {
        std::cout << "  [" << label << "] #" << item.id << " " << item.direction
                  << " \"" << item.source << "\" ... " << std::flush;
        std::string result = Translate(chat, item.direction, item.source);
        // Strip trailing newlines
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
        std::cout << result << "\n";
        results.push_back(std::move(result));
    }

    model.Unload();
    return results;
}

// ---------------------------------------------------------------------------
// Print TSV table
// ---------------------------------------------------------------------------
void PrintTable(const std::vector<TranslationItem>& items,
                const std::vector<std::string>& cudaResults,
                const std::vector<std::string>& webgpuResults,
                const std::vector<std::string>& cpuResults) {
    auto printRow = [](std::ostream& os, const TranslationItem& item,
                       const std::string& cuda, const std::string& webgpu, const std::string& cpu) {
        os << item.id << "\t" << item.direction << "\t" << item.source
           << "\t" << cuda << "\t" << webgpu << "\t" << cpu << "\n";
    };

    // Print to console
    std::cout << "\n#\tDirection\tSource\tCUDA\tWebGPU\tCPU\n";
    for (size_t i = 0; i < items.size(); ++i) {
        printRow(std::cout, items[i],
                 i < cudaResults.size()   ? cudaResults[i]   : "(skipped)",
                 i < webgpuResults.size() ? webgpuResults[i] : "(skipped)",
                 i < cpuResults.size()    ? cpuResults[i]    : "(skipped)");
    }

    // Write to file (UTF-8)
    std::ofstream outFile("translation_results.tsv", std::ios::binary);
    if (outFile) {
        // UTF-8 BOM so Excel/editors recognize encoding
        outFile << "\xEF\xBB\xBF";
        outFile << "#\tDirection\tSource\tCUDA\tWebGPU\tCPU\n";
        for (size_t i = 0; i < items.size(); ++i) {
            printRow(outFile, items[i],
                     i < cudaResults.size()   ? cudaResults[i]   : "(skipped)",
                     i < webgpuResults.size() ? webgpuResults[i] : "(skipped)",
                     i < cpuResults.size()    ? cpuResults[i]    : "(skipped)");
        }
        std::cout << "\nResults written to translation_results.tsv\n";
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    const std::string chatAlias = (argc > 1) ? argv[1] : "qwen3.5-2b-text";

    // --- Translation data ---
    std::vector<TranslationItem> items = {
        { 1, "zh->en", u8"你好"},
        { 2, "zh->en", u8"主播睡醒了"},
        { 3, "zh->en", u8"玩的什么游戏啊"},
        { 4, "zh->en", u8"听不见声音"},
        { 5, "zh->en", u8"主播在哪里啊"},
        { 6, "zh->en", u8"欢迎进入直播间的小伙伴"},
        { 7, "zh->en", u8"哈哈哈哈哈"},
        { 8, "zh->en", u8"我也会"},
        { 9, "zh->en", u8"我先走了 下次见"},
        {10, "zh->en", u8"这个游戏咋样"},
        {11, "en->zh", "First time here, love the stream!"},
        {12, "en->zh", "Streamer, your skills are amazing"},
        {13, "en->zh", "What's the name of this game?"},
        {14, "en->zh", "Can you turn up the mic?"},
        {15, "en->zh", "Hi from the US!"},
        {16, "en->zh", "Where can I follow you?"},
        {17, "en->zh", "Welcome to all the new viewers"},
        {18, "en->zh", "LOL that was hilarious"},
        {19, "en->zh", "I gotta go, see you tomorrow"},
        {20, "en->zh", "Is the streamer AFK?"},
    };

    try {
        QuietLogger logger;
        Manager::Create({"TranslationBenchmark"}, &logger);
        auto& manager = Manager::Instance();

        // Discover and register execution providers
        try {
            auto eps = manager.DiscoverEps();
            std::cout << "Available execution providers:\n";
            for (const auto& ep : eps)
                std::cout << "  " << ep.name << "\n";

            if (!eps.empty()) {
                std::cout << "Downloading execution providers...\n";
                manager.DownloadAndRegisterEps([](const std::string& epName, double percent) {
                    printf("\r  %-30s  %5.1f%%", epName.c_str(), percent);
                    fflush(stdout);
                });
                std::cout << "\n";
            }
        } catch (const std::exception& ex) {
            std::cerr << "EP discovery skipped: " << ex.what() << "\n";
        }

        // Get the model
        auto& catalog = manager.GetCatalog();
        auto* imodel = catalog.GetModel(chatAlias);
        if (!imodel) {
            std::cerr << "Model '" << chatAlias << "' not found in catalog.\n";
            Manager::Destroy();
            return 1;
        }

        auto* model = dynamic_cast<Model*>(imodel);
        if (!model) {
            std::cerr << "Unexpected model type for '" << chatAlias << "'.\n";
            Manager::Destroy();
            return 1;
        }

        // List available variants
        std::cout << "\nVariants for '" << chatAlias << "':\n";
        for (const auto& v : model->GetVariants()) {
            const auto& info = v.GetInfo();
            std::cout << "  " << info.name;
            if (info.runtime)
                std::cout << "  [ep=" << info.runtime->execution_provider
                          << ", device=" << (info.runtime->device_type == DeviceType::GPU ? "GPU"
                                            : info.runtime->device_type == DeviceType::NPU ? "NPU"
                                            : "CPU") << "]";
            if (info.file_size_mb)
                std::cout << "  " << *info.file_size_mb << " MB";
            std::cout << "\n";
        }

        // Find CUDA, WebGPU, and CPU variants
        const ModelVariant* cudaVariant   = FindVariant(*model, "CUDA");
        const ModelVariant* webgpuVariant = FindVariant(*model, "WebGPU");
        const ModelVariant* cpuVariant    = FindVariant(*model, "CPU");

        std::vector<std::string> cudaResults;
        std::vector<std::string> webgpuResults;
        std::vector<std::string> cpuResults;

        // --- Run CUDA ---
        if (cudaVariant) {
            std::cout << "\n=== CUDA Translations ===\n";
            cudaResults = RunTranslations(*model, *cudaVariant, items, "CUDA");
        } else {
            std::cout << "\nCUDA variant not available — skipping.\n";
            cudaResults.assign(items.size(), "(no CUDA variant)");
        }

        // --- Run WebGPU ---
        if (webgpuVariant) {
            std::cout << "\n=== WebGPU Translations ===\n";
            webgpuResults = RunTranslations(*model, *webgpuVariant, items, "WebGPU");
        } else {
            std::cout << "\nWebGPU variant not available — skipping.\n";
            webgpuResults.assign(items.size(), "(no WebGPU variant)");
        }

        // --- Run CPU ---
        if (cpuVariant) {
            std::cout << "\n=== CPU Translations ===\n";
            cpuResults = RunTranslations(*model, *cpuVariant, items, "CPU");
        } else {
            std::cout << "\nCPU variant not available — skipping.\n";
            cpuResults.assign(items.size(), "(no CPU variant)");
        }

        // --- Print results table ---
        PrintTable(items, cudaResults, webgpuResults, cpuResults);

        Manager::Destroy();
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << std::endl;
        Manager::Destroy();
        return 1;
    }
}
