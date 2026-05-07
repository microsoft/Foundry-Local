// <complete_code>
// <imports>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <stb_image.h>
#include <stb_image_resize2.h>
#include <stb_image_write.h>

#include "foundry_local.h"
// </imports>

#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

// ─── Base64 encoding ────────────────────────────────────────────────────────

static const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const std::vector<uint8_t>& data) {
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i < data.size()) {
        uint32_t octet_a = i < data.size() ? data[i++] : 0;
        uint32_t octet_b = i < data.size() ? data[i++] : 0;
        uint32_t octet_c = i < data.size() ? data[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        out.push_back(kBase64Chars[(triple >> 18) & 0x3F]);
        out.push_back(kBase64Chars[(triple >> 12) & 0x3F]);
        out.push_back(kBase64Chars[(triple >> 6) & 0x3F]);
        out.push_back(kBase64Chars[triple & 0x3F]);
    }
    size_t mod = data.size() % 3;
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
        std::vector<unsigned char> resized(newW * newH * 3);
        stbir_resize_uint8_linear(img, w, h, 0, resized.data(), newW, newH, 0, STBIR_RGB);
        stbi_image_free(img);

        std::cout << "  (resized to " << newW << "x" << newH << ")" << std::endl;

        // Encode resized image to JPEG in memory
        std::vector<uint8_t> jpegBuf;
        stbi_write_jpg_to_func(
            [](void* ctx, void* data, int size) {
                auto* buf = static_cast<std::vector<uint8_t>*>(ctx);
                auto* bytes = static_cast<uint8_t*>(data);
                buf->insert(buf->end(), bytes, bytes + size);
            },
            &jpegBuf, newW, newH, 3, resized.data(), 90);

        return {Base64Encode(jpegBuf), "image/jpeg"};
    }

    // No resize needed — encode original to JPEG
    std::vector<uint8_t> jpegBuf;
    stbi_write_jpg_to_func(
        [](void* ctx, void* data, int size) {
            auto* buf = static_cast<std::vector<uint8_t>*>(ctx);
            auto* bytes = static_cast<uint8_t*>(data);
            buf->insert(buf->end(), bytes, bytes + size);
        },
        &jpegBuf, w, h, 3, img, 90);
    stbi_image_free(img);

    return {Base64Encode(jpegBuf), "image/jpeg"};
}

// cURL SSE streaming callback
static size_t StreamWriteCallback(char* ptr, size_t size, size_t nmemb,
                                  void* /*userdata*/) {
    size_t totalBytes = size * nmemb;
    std::string chunk(ptr, totalBytes);

    std::istringstream stream(chunk);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.rfind("data: ", 0) != 0) continue;
        std::string data = line.substr(6);
        if (data == "[DONE]") break;

        try {
            auto j = json::parse(data);
            std::string type = j.value("type", "");
            if (type == "response.output_text.delta") {
                std::string delta = j.value("delta", "");
                std::cout << delta << std::flush;
            }
        } catch (...) {
            // Skip malformed JSON fragments
        }
    }
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

    try {
        // <init>
        foundry_local::Configuration config("foundry_local_samples");
        config.web = foundry_local::WebServiceConfig{};

        foundry_local::Manager::Create(config);
        auto& manager = foundry_local::Manager::Instance();
        // </init>

        // <model_setup>
        auto& catalog = manager.GetCatalog();
        auto* model = catalog.GetModel(modelAlias);
        if (!model) {
            auto models = catalog.GetModels();
            std::cout << "\nModel '" << modelAlias << "' not found in catalog." << std::endl;
            std::cout << "Available models:";
            for (auto* m : models) {
                std::cout << " " << m->GetAlias();
            }
            std::cout << std::endl;
            return 1;
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

        std::cout << "[ASSISTANT]: " << std::flush;
        CURLcode res = curl_easy_perform(curl);
        std::cout << std::endl;

        if (res != CURLE_OK) {
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
        return 1;
    }

    return 0;
}
