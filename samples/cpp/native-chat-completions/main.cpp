// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "foundry_local.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace foundry_local;

// ---------------------------------------------------------------------------
// Logger
// ---------------------------------------------------------------------------
class StdLogger final : public ILogger {
public:
    void Log(LogLevel level, std::string_view message) noexcept override {
        const char* tag = "UNK";
        switch (level) {
        case LogLevel::Information:
            tag = "INFO";
            break;
        case LogLevel::Warning:
            tag = "WARN";
            break;
        case LogLevel::Error:
            tag = "ERROR";
            break;
        default:
            tag = "DEBUG";
            break;
        }
        std::cout << "[FoundryLocal][" << tag << "] " << message << "\n";
    }
};

// ---------------------------------------------------------------------------
// Example 1 – Browse the catalog
// ---------------------------------------------------------------------------
void BrowseCatalog(Manager& manager) {
    std::cout << "\n=== Example 1: Browse Catalog ===\n";

    auto& catalog = manager.GetCatalog();
    std::cout << "Catalog: " << catalog.GetName() << "\n";

    auto models = catalog.GetModels();
    std::cout << "Models in catalog: " << models.size() << "\n";

    for (const auto* model : models) {
        std::cout << "  - " << model->GetAlias() << " (" << model->GetId() << ")"
                  << "  cached=" << (model->IsCached() ? "yes" : "no")
                  << "  loaded=" << (model->IsLoaded() ? "yes" : "no") << "\n";

        auto* concreteModel = dynamic_cast<const Model*>(model);
        if (!concreteModel) continue;
        for (const auto& variant : concreteModel->GetVariants()) {
            const auto& info = variant.GetInfo();
            std::cout << "      variant: " << info.name << "  v" << info.version
                      << "  cached=" << (variant.IsCached() ? "yes" : "no");
            if (info.display_name)
                std::cout << "  display=\"" << *info.display_name << "\"";
            if (info.publisher)
                std::cout << "  publisher=" << *info.publisher;
            if (info.license)
                std::cout << "  license=" << *info.license;
            if (info.runtime) {
                std::cout << "  device="
                          << (info.runtime->device_type == DeviceType::GPU   ? "GPU"
                              : info.runtime->device_type == DeviceType::NPU ? "NPU"
                                                                             : "CPU")
                          << "  ep=" << info.runtime->execution_provider;
            }
            if (info.file_size_mb)
                std::cout << "  size=" << *info.file_size_mb << "MB";
            if (info.supports_tool_calling)
                std::cout << "  tools=" << (*info.supports_tool_calling ? "yes" : "no");
            std::cout << "\n";
        }
    }
}

// ---------------------------------------------------------------------------
// Example 2 – Download, load, chat (non-streaming), then unload
// ---------------------------------------------------------------------------
void ChatNonStreaming(Manager& manager, const std::string& alias) {
    std::cout << "\n=== Example 2: Non-Streaming Chat ===\n";

    auto& catalog = manager.GetCatalog();

    auto* model = catalog.GetModel(alias);
    if (!model) {
        std::cerr << "Model '" << alias << "' not found in catalog.\n";
        return;
    }

    model->Download([](float pct) { printf("\rDownloading: %5.1f%%", pct); fflush(stdout); return true; });
    std::cout << "\n";

    model->Load();

    if (model->IsLoaded()) {
        std::cout << "Model is loaded and ready for inference.\n";
    }
    else {
        std::cerr << "Failed to load model.\n";
        return;
    }

    OpenAIChatClient chat(*model);

    std::vector<ChatMessage> messages = {{"user", "What is the capital of Croatia?"}};

    ChatSettings settings;
    settings.temperature = 0.7f;
    settings.max_tokens = 128;

    auto response = chat.CompleteChat(messages, settings);

    if (!response.choices.empty() && response.choices[0].message) {
        std::cout << "Assistant: " << response.choices[0].message->content << "\n";
    }

    model->Unload();
    std::cout << "Model unloaded.\n";
}

// ---------------------------------------------------------------------------
// Example 3 – Streaming chat
// ---------------------------------------------------------------------------
void ChatStreaming(Manager& manager, const std::string& alias) {
    std::cout << "\n=== Example 3: Streaming Chat ===\n";

    auto& catalog = manager.GetCatalog();

    auto* model = catalog.GetModel(alias);
    if (!model) {
        std::cerr << "Model '" << alias << "' not found in catalog.\n";
        return;
    }

    model->Load();

    OpenAIChatClient chat(*model);

    std::vector<ChatMessage> messages = {{"user", "Explain quantum computing in three sentences."}};

    ChatSettings settings;
    settings.temperature = 0.9f;
    settings.max_tokens = 256;

    std::cout << "Assistant: ";
    chat.CompleteChatStreaming(messages, settings, [](const ChatCompletionCreateResponse& chunk) {
        if (chunk.choices.empty())
            return;
        const auto& choice = chunk.choices[0];
        if (choice.delta && !choice.delta->content.empty()) {
            std::cout << choice.delta->content << std::flush;
        }
    });
    std::cout << "\n";

    model->Unload();
}

// ---------------------------------------------------------------------------
// Example 4 – Translation
// ---------------------------------------------------------------------------
// Demonstrates using the chat model to translate text from one language to
// another by giving it an explicit system prompt and a user message containing
// the source text.
// ---------------------------------------------------------------------------
void ChatTranslate(Manager& manager, const std::string& alias) {
    std::cout << "\n=== Example 4: Translation ===\n";

    auto& catalog = manager.GetCatalog();

    auto* model = catalog.GetModel(alias);
    if (!model) {
        std::cerr << "Model '" << alias << "' not found in catalog.\n";
        return;
    }

    model->Load();
    std::cout << "Model loaded: " << model->GetAlias() << "\n";

    OpenAIChatClient chat(*model);

    const std::string sourceText = "你好。";

    std::vector<ChatMessage> messages = {
        {"system",
         "You are a translation assistant. Translate the user's text to English. "
         "Respond with only the translated text, no explanations."},
        {"user", "Translate the following to English: " + sourceText}};

    ChatSettings settings;
    settings.temperature = 0.0f;
    settings.max_tokens = 128;

    std::cout << "Source: " << sourceText << "\n";
    auto response = chat.CompleteChat(messages, settings);

    if (!response.choices.empty() && response.choices[0].message) {
        std::cout << "English: " << response.choices[0].message->content << "\n";
    }

    model->Unload();
    std::cout << "Model unloaded.\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Optional command-line arg: <chat-model-alias>
    const std::string chatAlias = (argc > 1) ? argv[1] : "qwen3.5-2b-text";

    try {
        StdLogger logger;
        Manager::Create({"SampleApp"}, &logger);
        auto& manager = Manager::Instance();

        // Discover and download execution providers (optional — may not be
        // supported by older Core DLLs or offline environments)
        try {
            auto eps = manager.DiscoverEps();
            std::cout << "\nAvailable execution providers:\n";
            for (const auto& ep : eps) {
                std::cout << "  " << ep.name << "\n";
            }

            if (!eps.empty()) {
                std::cout << "\nDownloading execution providers:\n";
                std::string currentEp;
                manager.DownloadAndRegisterEps([&](const std::string& epName, double percent) {
                    if (epName != currentEp) {
                        if (!currentEp.empty()) std::cout << "\n";
                        currentEp = epName;
                    }
                    printf("\r  %-30s  %5.1f%%", epName.c_str(), percent);
                    fflush(stdout);
                });
                if (!currentEp.empty()) std::cout << "\n";
            } else {
                std::cout << "\nNo execution providers to download.\n";
            }
        } catch (const std::exception& ex) {
            std::cerr << "EP discovery/download skipped: " << ex.what() << "\n";
        }

        // 1. Browse the full catalog
        try {
             BrowseCatalog(manager);
         }
        catch (const std::exception& ex) {
             std::cerr << "Example 1 failed: " << ex.what() << "\n";
         }

        // 2. Non-streaming chat
        try {
            ChatNonStreaming(manager, chatAlias);
        }
        catch (const std::exception& ex) {
            std::cerr << "Example 2 failed: " << ex.what() << "\n";
        }

        // 3. Streaming chat
        try {
            ChatStreaming(manager, chatAlias);
        }
        catch (const std::exception& ex) {
            std::cerr << "Example 3 failed: " << ex.what() << "\n";
        }

        // 4. Translation
        try {
            ChatTranslate(manager, chatAlias);
        }
        catch (const std::exception& ex) {
            std::cerr << "Example 4 failed: " << ex.what() << "\n";
        }

        Manager::Destroy();
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << std::endl;
        Manager::Destroy();
        return 1;
    }
}
