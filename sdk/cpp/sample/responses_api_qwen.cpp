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

#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ole32.lib")
#include <shellapi.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <iomanip>

using namespace foundry_local;
namespace fs = std::filesystem;

static const std::string MODEL_ALIAS = "qwen3.5-2b";
static const int MAX_IMAGE_DIM = 960;

// ── Logger ──────────────────────────────────────────────────────────────────

class StdLogger final : public ILogger {
public:
    void Log(LogLevel level, std::string_view message) noexcept override {
        const char* tags[] = {"DEBUG", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
        auto idx = static_cast<int>(level);
        std::cout << "[" << tags[idx < 6 ? idx : 0] << "] " << message << "\n";
    }
};

// ── GDI+ RAII init ──────────────────────────────────────────────────────────

struct GdiplusInit {
    ULONG_PTR token{};
    GdiplusInit()  { Gdiplus::GdiplusStartupInput in{}; Gdiplus::GdiplusStartup(&token, &in, nullptr); }
    ~GdiplusInit() { Gdiplus::GdiplusShutdown(token); }
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

static std::string GetMimeType(const std::string& filePath) {
    auto ext = fs::path(filePath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".webp") return "image/webp";
    if (ext == ".bmp") return "image/bmp";
    return "image/png";
}

// Returns the CLSID for a GDI+ encoder by MIME type (e.g. L"image/png").
static bool GetEncoderClsid(const WCHAR* mimeType, CLSID* clsid) {
    UINT count = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&count, &size);
    if (size == 0) return false;
    std::vector<uint8_t> buf(size);
    auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    Gdiplus::GetImageEncoders(count, size, codecs);
    for (UINT i = 0; i < count; i++)
        if (wcscmp(codecs[i].MimeType, mimeType) == 0) { *clsid = codecs[i].Clsid; return true; }
    return false;
}

// Resizes img proportionally so neither dimension exceeds MAX_IMAGE_DIM.
// Encodes the result as PNG entirely in memory. Returns empty vector if no resize needed.
static std::vector<uint8_t> ResizeToPngIfNeeded(Gdiplus::Image* img) {
    UINT origW = img->GetWidth(), origH = img->GetHeight();
    if (origW <= static_cast<UINT>(MAX_IMAGE_DIM) && origH <= static_cast<UINT>(MAX_IMAGE_DIM))
        return {};

    double scale = std::min(double(MAX_IMAGE_DIM) / origW, double(MAX_IMAGE_DIM) / origH);
    int nw = static_cast<int>(origW * scale);
    int nh = static_cast<int>(origH * scale);
    std::cout << "* Resizing image " << origW << "x" << origH << " -> " << nw << "x" << nh << "\n";

    Gdiplus::Bitmap bmp(nw, nh, PixelFormat32bppARGB);
    {
        auto g = std::unique_ptr<Gdiplus::Graphics>(Gdiplus::Graphics::FromImage(&bmp));
        g->SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g->DrawImage(img, 0, 0, nw, nh);
    }

    CLSID pngClsid;
    if (!GetEncoderClsid(L"image/png", &pngClsid)) return {};

    IStream* pStream = nullptr;
    if (CreateStreamOnHGlobal(nullptr, TRUE, &pStream) != S_OK) return {};
    if (bmp.Save(pStream, &pngClsid) != Gdiplus::Ok) { pStream->Release(); return {}; }

    STATSTG stat{};
    pStream->Stat(&stat, STATFLAG_NONAME);
    LARGE_INTEGER li{}; pStream->Seek(li, STREAM_SEEK_SET, nullptr);
    std::vector<uint8_t> out(static_cast<size_t>(stat.cbSize.QuadPart));
    ULONG bytesRead;
    pStream->Read(out.data(), (ULONG)out.size(), &bytesRead);
    pStream->Release();
    return out;
}

// If either dimension exceeds MAX_IMAGE_DIM, resizes the image proportionally
// using HighQualityBicubic interpolation, saves as a temp PNG, and returns
// its path. Returns empty string if no resize was needed or on error.
static std::string ResizeImageIfNeeded(const std::string& filePath) {
    // Convert path to wide string for GDI+
    int wlen = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, nullptr, 0);
    std::wstring wPath(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, wPath.data(), wlen);

    auto img = std::unique_ptr<Gdiplus::Image>(Gdiplus::Image::FromFile(wPath.c_str()));
    if (!img || img->GetLastStatus() != Gdiplus::Ok) return "";

    auto pngBytes = ResizeToPngIfNeeded(img.get());
    if (pngBytes.empty()) return "";
    img.reset();

    // Write resized PNG to a temp file (ReadImageAsDataUri reads it back)
    wchar_t tempDir[MAX_PATH], tempBase[MAX_PATH];
    GetTempPathW(MAX_PATH, tempDir);
    GetTempFileNameW(tempDir, L"flr", 0, tempBase);
    std::wstring tempPng = std::wstring(tempBase) + L".png";
    DeleteFileW(tempBase);

    HANDLE hFile = CreateFileW(tempPng.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return "";
    DWORD written;
    WriteFile(hFile, pngBytes.data(), (DWORD)pngBytes.size(), &written, nullptr);
    CloseHandle(hFile);

    int len = WideCharToMultiByte(CP_UTF8, 0, tempPng.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, tempPng.c_str(), -1, result.data(), len, nullptr, nullptr);
    return result;
}

static std::string ReadImageAsDataUri(const std::string& filePath) {
    auto absPath = fs::absolute(filePath).string();

    // Resize if either dimension exceeds MAX_IMAGE_DIM
    std::string resized = ResizeImageIfNeeded(absPath);
    bool usedTemp = !resized.empty();
    const std::string& pathToRead = usedTemp ? resized : absPath;
    std::string mime = usedTemp ? "image/png" : GetMimeType(absPath);

    std::ifstream file(pathToRead, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open image: " + pathToRead);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), {});
    file.close();

    if (usedTemp) fs::remove(pathToRead);

    return "data:" + mime + ";base64," + Base64Encode(data);
}

static bool IsUrl(const std::string& p) {
    return p.find("http://") == 0 || p.find("https://") == 0;
}

// Downloads an image from a URL, optionally resizes it, and returns a base64 data URI.
// Loads entirely in memory — no temp files.
static std::string DownloadUrlAsDataUri(const std::string& url) {
    // Parse URL
    std::wstring wUrl(url.begin(), url.end());
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{}, path[2048]{};
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath  = path; uc.dwUrlPathLength  = 2048;
    if (!WinHttpCrackUrl(wUrl.c_str(), (DWORD)wUrl.size(), 0, &uc))
        throw std::runtime_error("Failed to parse image URL: " + url);

    bool isHttps = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    // WINHTTP_ACCESS_TYPE_NO_PROXY skips proxy auto-detection (avoids WPAD/PAC lookup delay)
    HINTERNET hSession = WinHttpOpen(L"FoundryLocal-CppSdk/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) throw std::runtime_error("WinHttpOpen failed");

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); throw std::runtime_error("WinHttpConnect failed"); }

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path,
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        throw std::runtime_error("WinHttpOpenRequest failed");
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        throw std::runtime_error("HTTP request failed for URL: " + url);
    }

    // Pre-allocate from Content-Length if available
    std::vector<uint8_t> imgBytes;
    wchar_t lenBuf[32]{};
    DWORD lenBufSize = sizeof(lenBuf);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH,
            WINHTTP_HEADER_NAME_BY_INDEX, lenBuf, &lenBufSize, WINHTTP_NO_HEADER_INDEX))
        imgBytes.reserve(static_cast<size_t>(_wtoi(lenBuf)));

    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
        size_t offset = imgBytes.size();
        imgBytes.resize(offset + avail);
        DWORD read = 0;
        WinHttpReadData(hRequest, imgBytes.data() + offset, avail, &read);
        if (read < avail) imgBytes.resize(offset + read);
    }
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);

    if (imgBytes.empty()) throw std::runtime_error("Empty response downloading image: " + url);

    // Load GDI+ directly from memory — no temp file needed
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, imgBytes.size());
    if (!hGlobal) throw std::runtime_error("GlobalAlloc failed");
    memcpy(GlobalLock(hGlobal), imgBytes.data(), imgBytes.size());
    GlobalUnlock(hGlobal);
    IStream* pStream = nullptr;
    CreateStreamOnHGlobal(hGlobal, TRUE, &pStream); // TRUE = stream owns and frees hGlobal

    auto img = std::unique_ptr<Gdiplus::Image>(Gdiplus::Image::FromStream(pStream));
    pStream->Release();

    if (!img || img->GetLastStatus() != Gdiplus::Ok)
        throw std::runtime_error("GDI+ failed to decode image from: " + url);

    std::string mime = GetMimeType(url);

    // Resize in memory if needed
    auto resizedBytes = ResizeToPngIfNeeded(img.get());
    if (!resizedBytes.empty())
        return "data:image/png;base64," + Base64Encode(resizedBytes);

    return "data:" + mime + ";base64," + Base64Encode(imgBytes);
}

// ── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // Use Windows wide-char API to properly handle Unicode command-line arguments
    SetConsoleOutputCP(CP_UTF8);
    int wargc;
    wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);

    auto wideToUtf8 = [](const wchar_t* ws) -> std::string {
        if (!ws || !*ws) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
        std::string s(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws, -1, s.data(), len, nullptr, nullptr);
        return s;
    };

    GdiplusInit gdiplusInit; // must outlive all GDI+ calls
    try {
        // Parse CLI
        std::string textInput, imagePath;
        bool checkCache = false;
        for (int i = 1; i < wargc; i++) {
            std::string a = wideToUtf8(wargv[i]);
            if (a == "--image" && i + 1 < wargc) imagePath = wideToUtf8(wargv[++i]);
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
            auto cached = mgr.GetCatalog().GetCachedModels();
            if (cached.empty()) {
                std::cout << "No models cached.\n";
            } else {
                std::vector<std::string> seen;
                for (const auto* m : cached) {
                    auto alias = m->GetAlias();
                    if (std::find(seen.begin(), seen.end(), alias) == seen.end()) {
                        seen.push_back(alias);
                        std::cout << "  - " << alias << "\n";
                    }
                }
            }
            Manager::Destroy();
            LocalFree(wargv);
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
            std::string dataUri = IsUrl(imagePath) ? DownloadUrlAsDataUri(imagePath) : ReadImageAsDataUri(imagePath);
            img.image_url  = dataUri;
            img.media_type = dataUri.substr(5, dataUri.find(';') - 5); // extract from "data:<mime>;..."
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
        LocalFree(wargv);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        if (Manager::IsInitialized()) Manager::Destroy();
        LocalFree(wargv);
        return 1;
    }
}
