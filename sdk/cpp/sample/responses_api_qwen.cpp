// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Responses API with Qwen 3.5 — C++ Sample
//
// Usage:
//   responses_api_qwen.exe "What is quantum computing?"
//   responses_api_qwen.exe "Describe this image" --image photo.png
//   responses_api_qwen.exe --check-cache

#include "foundry_local.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <iomanip>

using namespace foundry_local;
namespace fs = std::filesystem;

static const std::string MODEL_ALIAS = "qwen3.5-4b";

// ── Logger ──────────────────────────────────────────────────────────────────

class StdLogger final : public ILogger {
public:
    void Log(LogLevel level, std::string_view message) noexcept override {
        const char* tags[] = {"DEBUG", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
        auto idx = static_cast<int>(level);
        std::cout << "[" << tags[idx < 6 ? idx : 0] << "] " << message << "\n";
    }
};

// ── Image helpers ───────────────────────────────────────────────────────────

static std::string Base64Encode(const std::vector<uint8_t>& data) {
    static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string r;
    r.reserve(((data.size() + 2) / 3) * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t n = uint32_t(data[i]) << 16;
        if (i + 1 < data.size()) n |= uint32_t(data[i + 1]) << 8;
        if (i + 2 < data.size()) n |= uint32_t(data[i + 2]);
        r += t[(n >> 18) & 0x3F];
        r += t[(n >> 12) & 0x3F];
        r += (i + 1 < data.size()) ? t[(n >> 6) & 0x3F] : '=';
        r += (i + 2 < data.size()) ? t[n & 0x3F] : '=';
    }
    return r;
}

static std::string ReadImageAsDataUri(const std::string& filePath) {
    auto ext = fs::path(filePath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    std::string mime = (ext == ".jpg" || ext == ".jpeg") ? "image/jpeg"
                     : (ext == ".gif") ? "image/gif"
                     : (ext == ".webp") ? "image/webp"
                     : "image/png";

    std::ifstream file(fs::absolute(filePath).string(), std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open image: " + filePath);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), {});
    return "data:" + mime + ";base64," + Base64Encode(data);
}

static bool IsUrl(const std::string& p) {
    return p.find("http://") == 0 || p.find("https://") == 0;
}

// ── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    try {
        // Parse CLI
        std::string textInput, imagePath;
        bool checkCache = false;
        for (int i = 1; i < argc; i++) {
            std::string a = argv[i];
            if (a == "--image" && i + 1 < argc) imagePath = argv[++i];
            else if (a == "--check-cache") checkCache = true;
            else if (textInput.empty()) textInput = a;
        }
        if (textInput.empty() && !checkCache) {
            std::cout << "Usage: responses_api_qwen.exe \"prompt\" [--image path] [--check-cache]\n";
            return 1;
        }

        // Init
        StdLogger logger;
        Configuration config("responses_api_qwen3");
        config.log_level = LogLevel::Information;
        config.web = WebServiceConfig{};
        Manager::Create(std::move(config), &logger);
        auto& mgr = Manager::Instance();

        // Check cache
        if (checkCache) {
            for (const auto* m : mgr.GetCatalog().ListModels())
                if (m->IsCached()) std::cout << "  - " << m->GetAlias() << "\n";
            Manager::Destroy();
            return 0;
        }

        // Find model
        auto& catalog = mgr.GetCatalog();
        IModel* model = nullptr;
        for (auto* m : catalog.ListModels())
            if (m->GetAlias() == MODEL_ALIAS || m->GetId() == MODEL_ALIAS) { model = m; break; }
        if (!model) { std::cerr << "Model not found: " << MODEL_ALIAS << "\n"; Manager::Destroy(); return 1; }

        // Select CPU variant & get max_output_tokens
        std::optional<int> modelMaxTokens;
        if (auto* cm = dynamic_cast<Model*>(model)) {
            for (const auto& v : cm->GetAllModelVariants()) {
                if (v.GetId().find("cpu") != std::string::npos) {
                    cm->SelectVariant(v);
                    std::cout << "* Selected CPU variant: " << v.GetId() << "\n";
                    if (v.GetInfo().max_output_tokens)
                        modelMaxTokens = static_cast<int>(*v.GetInfo().max_output_tokens);
                    break;
                }
            }
        }

        // Download & load
        if (!model->IsCached()) {
            std::cout << "Downloading " << MODEL_ALIAS << "...\n";
            model->Download([](float p) { std::cout << "\r  " << std::fixed << std::setprecision(1) << p << "%" << std::flush; });
            std::cout << "\n";
        }
        model->Load();
        std::cout << "* Model loaded\n";

        // Start web service
        mgr.StartWebService();
        std::cout << "* Web service at " << mgr.GetUrls()[0] << "\n";

        // Create client
        auto client = mgr.CreateResponsesClient(model->GetId());
        client.GetSettings().temperature = 0.7f;
        client.GetSettings().max_output_tokens = modelMaxTokens.value_or(imagePath.empty() ? 2048 : 4096);

        // Build input
        std::cout << "Prompt: " << textInput << "\n--- Response ---\n";

        ResponseMessageItem userMsg;
        userMsg.role = "user";

        if (!imagePath.empty()) {
            ContentPart img;
            img.type = "input_image";
            img.image_url = IsUrl(imagePath) ? imagePath : ReadImageAsDataUri(imagePath);
            img.detail = "auto";
            userMsg.content_parts.push_back(std::move(img));
        }

        ContentPart txt;
        txt.type = "input_text";
        txt.text = textInput;
        userMsg.content_parts.push_back(std::move(txt));

        ResponseInputItem inputItem;
        inputItem.type = "message";
        inputItem.message = std::move(userMsg);

        std::vector<ResponseInputItem> input = {std::move(inputItem)};

        // Stream
        client.CreateStreaming(input, [](const StreamingEvent& ev) {
            if (ev.type == "response.output_text.delta" && ev.delta)
                std::cout << *ev.delta << std::flush;
            if (ev.type == "response.failed" && ev.response && ev.response->error)
                std::cerr << "\n[ERROR] " << ev.response->error->code << ": " << ev.response->error->message << "\n";
        });
        std::cout << "\n";

        // Cleanup
        model->Unload();
        Manager::Destroy();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        if (Manager::IsInitialized()) Manager::Destroy();
        return 1;
    }
}
