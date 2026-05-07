// <complete_code>
// <imports>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <stb_image.h>
#include <stb_image_resize2.h>
#include <stb_image_write.h>

#include "foundry_local.h"
// </imports>

using json = nlohmann::json;

// ─── Base64 encoding ────────────────────────────────────────────────────────

static const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const std::vector<uint8_t>& data) {
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    size_t i = 0;
    const size_t len = data.size();
    while (i < len) {
        uint32_t octet_a = data[i++];
        uint32_t octet_b = (i < len) ? data[i++] : 0;
        uint32_t octet_c = (i < len) ? data[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        out.push_back(kBase64Chars[(triple >> 18) & 0x3F]);
        out.push_back(kBase64Chars[(triple >> 12) & 0x3F]);
        out.push_back(kBase64Chars[(triple >> 6) & 0x3F]);
        out.push_back(kBase64Chars[triple & 0x3F]);
    }
    size_t mod = len % 3;
    if (mod == 1) {
        out[out.size() - 2] = '=';
        out[out.size() - 1] = '=';
    } else if (mod == 2) {
        out[out.size() - 1] = '=';
    }
    return out;
}

// Load and resize a local image, returning (base64_str, media_type).
// Mirrors the Python sample's resize_and_encode(path, max_dim=512).
std::pair<std::string, std::string> ResizeAndEncode(const std::filesystem::path& path, int maxDim = 512) {
    int w = 0, h = 0, channels = 0;
    unsigned char* img = stbi_load(path.string().c_str(), &w, &h, &channels, 3);
    if (!img) {
        throw std::runtime_error("Failed to load image: " + path.string());
    }

    int newW = w, newH = h;
    if ((std::max)(w, h) > maxDim) {
        if (w >= h) {
            newW = maxDim;
            newH = static_cast<int>(static_cast<float>(h) * maxDim / w);
        } else {
            newH = maxDim;
            newW = static_cast<int>(static_cast<float>(w) * maxDim / h);
        }
        // Clamp to at least 1 pixel for extreme aspect ratios
        newW = (std::max)(newW, 1);
        newH = (std::max)(newH, 1);

        const auto resizedSize =
            static_cast<std::vector<unsigned char>::size_type>(newW) *
            static_cast<std::vector<unsigned char>::size_type>(newH) *
            static_cast<std::vector<unsigned char>::size_type>(3);
        std::vector<unsigned char> resized(resizedSize);
        unsigned char* result = stbir_resize_uint8_linear(
            img, w, h, 0, resized.data(), newW, newH, 0, STBIR_RGB);
        stbi_image_free(img);
        if (!result) {
            throw std::runtime_error("Failed to resize image");
        }

        std::cout << "  (resized to " << newW << "x" << newH << ")" << std::endl;

        // Encode resized image to JPEG in memory
        std::vector<uint8_t> jpegBuf;
        int writeOk = stbi_write_jpg_to_func(
            [](void* ctx, void* data, int size) {
                auto* buf = static_cast<std::vector<uint8_t>*>(ctx);
                auto* bytes = static_cast<uint8_t*>(data);
                buf->insert(buf->end(), bytes, bytes + size);
            },
            &jpegBuf, newW, newH, 3, resized.data(), 90);
        if (!writeOk) {
            throw std::runtime_error("Failed to encode resized image to JPEG");
        }

        return {Base64Encode(jpegBuf), "image/jpeg"};
    }

    // No resize needed — encode original to JPEG
    std::vector<uint8_t> jpegBuf;
    int writeOk = stbi_write_jpg_to_func(
        [](void* ctx, void* data, int size) {
            auto* buf = static_cast<std::vector<uint8_t>*>(ctx);
            auto* bytes = static_cast<uint8_t*>(data);
            buf->insert(buf->end(), bytes, bytes + size);
        },
        &jpegBuf, w, h, 3, img, 90);
    stbi_image_free(img);
    if (!writeOk) {
        throw std::runtime_error("Failed to encode image to JPEG");
    }

    return {Base64Encode(jpegBuf), "image/jpeg"};
}

// Persistent buffer for SSE parsing across cURL callbacks.
struct SseBuffer {
    std::string partial; // incomplete line carried over between callbacks
    bool done = false;   // set when [DONE] is received
};

// Process a single complete SSE line. Returns true if [DONE] was received.
static bool ProcessSseLine(const std::string& line) {
    if (line.rfind("data: ", 0) != 0) return false;
    std::string data = line.substr(6);
    if (data == "[DONE]") return true;

    try {
        auto j = json::parse(data);
        std::string type = j.value("type", "");
        if (type == "response.output_text.delta") {
            std::string delta = j.value("delta", "");
            std::cout << delta << std::flush;
        }
    } catch (...) {
        // Skip malformed JSON
    }
    return false;
}

// cURL SSE streaming callback — appends to a persistent buffer and
// processes only complete lines, retaining any trailing partial line.
// Returns 0 to abort the transfer once [DONE] is observed.
static size_t StreamWriteCallback(char* ptr, size_t size, size_t nmemb,
                                  void* userdata) {
    size_t totalBytes = size * nmemb;
    auto* buf = static_cast<SseBuffer*>(userdata);

    if (buf->done) return 0; // abort transfer

    buf->partial.append(ptr, totalBytes);

    // Process all complete lines (terminated by \n)
    std::string::size_type pos = 0;
    std::string::size_type newline;
    while ((newline = buf->partial.find('\n', pos)) != std::string::npos) {
        std::string line = buf->partial.substr(pos, newline - pos);
        // Strip trailing \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (ProcessSseLine(line)) {
            buf->done = true;
            buf->partial.clear();
            return 0; // abort transfer cleanly
        }
        pos = newline + 1;
    }
    // Retain any trailing partial line for the next callback
    buf->partial.erase(0, pos);

    return totalBytes;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: web-server-responses-vision <model_alias>" << std::endl;
        std::cout << "  Example: web-server-responses-vision qwen3.5-0.8b" << std::endl;
        return 1;
    }

    const std::string modelAlias = argv[1];
    const std::filesystem::path imagePath =
        std::filesystem::path(__FILE__).parent_path() / "test_image.jpg";

    CURLcode globalRes = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (globalRes != CURLE_OK) {
        std::cerr << "Error: curl_global_init failed: " << curl_easy_strerror(globalRes) << std::endl;
        return 1;
    }

    try {
        // <init>
        foundry_local::Configuration config("foundry_local_samples");
        config.web = foundry_local::WebServiceConfig{};

        foundry_local::Manager::Create(config);
        auto& manager = foundry_local::Manager::Instance();

        // Discover and download execution providers (like C# sample)
        auto eps = manager.DiscoverEps();
        std::cout << "\nAvailable execution providers:" << std::endl;
        for (const auto& ep : eps) {
            std::cout << "  " << ep.name << std::endl;
        }

        if (!eps.empty()) {
            std::cout << "\nDownloading execution providers:" << std::endl;
            std::string currentEp;
            manager.DownloadAndRegisterEps([&](const std::string& epName, double percent) {
                if (epName != currentEp) {
                    if (!currentEp.empty()) {
                        std::cout << std::endl;
                    }
                    currentEp = epName;
                }
                // Fixed-width output to overwrite previous line cleanly
                std::cout << "\r  " << std::left << std::setw(30) << epName
                          << "  " << std::right << std::fixed << std::setprecision(1)
                          << std::setw(6) << percent << "%   " << std::flush;
            });
            if (!currentEp.empty()) {
                std::cout << std::endl;
            }
        } else {
            std::cout << "\nNo execution providers to download." << std::endl;
        }
        // </init>

        // <model_setup>
        auto& catalog = manager.GetCatalog();
        auto* model = catalog.GetModel(modelAlias);
        if (!model) {
            auto models = catalog.GetModels();
            std::string available;
            for (auto* m : models) {
                available += " " + m->GetAlias();
            }
            throw std::runtime_error(
                "Model '" + modelAlias + "' not found in catalog. Available:" + available);
        }

        if (!model->IsCached()) {
            std::cout << "\nDownloading model " << modelAlias << "..." << std::endl;
            model->Download([](float pct) {
                std::cout << "\rDownloading model: " << pct << "%   " << std::flush;
                return true;
            });
            std::cout << "\nModel downloaded" << std::endl;
        }

        std::cout << "\nLoading model..." << std::endl;
        model->Load();
        std::cout << "Model loaded" << std::endl;
        // </model_setup>

        // <server_setup>
        std::cout << "\nStarting web service..." << std::endl;
        manager.StartWebService();
        auto endpoints = manager.GetWebServiceEndpoints();
        if (endpoints.empty()) {
            throw std::runtime_error("No web service endpoints available");
        }
        std::string baseUrl = endpoints[0];
        if (!baseUrl.empty() && baseUrl.back() == '/') {
            baseUrl.pop_back();
        }
        baseUrl += "/v1";
        std::cout << "Web service started" << std::endl;

        // Use cURL to call the local Foundry web service Responses API
        // (C++ equivalent of OpenAI SDK used in the Python sample)
        std::string responsesUrl = baseUrl + "/responses";
        // </server_setup>

        // <inference>
        std::cout << "\nPreparing image: " << imagePath.string() << std::endl;
        auto [imageB64, mediaType] = ResizeAndEncode(imagePath);

        json visionInput = json::array({
            {
                {"type", "message"},
                {"role", "user"},
                {"content", json::array({
                    {{"type", "input_text"}, {"text", "Describe this image."}},
                    {
                        {"type", "input_image"},
                        {"image_data", imageB64},
                        {"media_type", mediaType},
                    }
                })}
            }
        });

        std::cout << "\nStreaming vision response..." << std::endl;

        json requestBody = {
            {"model", model->GetId()},
            {"input", visionInput},
            {"stream", true},
        };
        std::string body = requestBody.dump();

        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize cURL");
        }

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "Accept: text/event-stream");

        curl_easy_setopt(curl, CURLOPT_URL, responsesUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StreamWriteCallback);

        SseBuffer sseBuf;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sseBuf);

        std::cout << "[ASSISTANT]: " << std::flush;
        CURLcode res = curl_easy_perform(curl);
        std::cout << std::endl;

        if (res != CURLE_OK && !(res == CURLE_WRITE_ERROR && sseBuf.done)) {
            std::cerr << "cURL error: " << curl_easy_strerror(res) << std::endl;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        // </inference>

        manager.StopWebService();
        model->Unload();
        foundry_local::Manager::Destroy();

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        curl_global_cleanup();
        return 1;
    }

    curl_global_cleanup();
    return 0;
}
