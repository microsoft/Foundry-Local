// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Internal WinHTTP helpers. Not part of the public API.

#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <algorithm>

#include <windows.h>
#include <winhttp.h>
#include <wil/resource.h>

#pragma comment(lib, "winhttp.lib")

namespace foundry_local::detail {

    inline std::wstring ToWide(std::string_view s) {
        if (s.empty()) return {};
        int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        std::wstring ws(n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), ws.data(), n);
        return ws;
    }

    struct ParsedUrl {
        std::wstring host;
        INTERNET_PORT port = INTERNET_DEFAULT_HTTP_PORT;
        std::wstring path;
        bool isHttps = false;
    };

    inline ParsedUrl ParseUrl(const std::string& url) {
        auto w = ToWide(url);
        URL_COMPONENTS uc{}; uc.dwStructSize = sizeof(uc);
        wchar_t h[256]{}, p[2048]{};
        uc.lpszHostName = h; uc.dwHostNameLength = 256;
        uc.lpszUrlPath = p; uc.dwUrlPathLength = 2048;
        if (!WinHttpCrackUrl(w.c_str(), (DWORD)w.size(), 0, &uc))
            throw std::runtime_error("Failed to parse URL: " + url);
        return {h, uc.nPort, p, uc.nScheme == INTERNET_SCHEME_HTTPS};
    }

    /// Opens a WinHTTP request handle (session → connect → request).
    struct WinHttpHandles {
        wil::unique_winhttp_hinternet session, connect, request;
    };

    inline WinHttpHandles OpenRequest(const ParsedUrl& parsed, const wchar_t* method) {
        WinHttpHandles h;
        h.session.reset(WinHttpOpen(L"FoundryLocal-CppSdk/1.0",
                        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0));
        if (!h.session) throw std::runtime_error("WinHttpOpen failed");

        h.connect.reset(WinHttpConnect(h.session.get(), parsed.host.c_str(), parsed.port, 0));
        if (!h.connect) throw std::runtime_error("WinHttpConnect failed");

        DWORD flags = parsed.isHttps ? WINHTTP_FLAG_SECURE : 0;
        h.request.reset(WinHttpOpenRequest(h.connect.get(), method, parsed.path.c_str(),
                        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
        if (!h.request) throw std::runtime_error("WinHttpOpenRequest failed");
        return h;
    }

    inline DWORD GetStatusCode(HINTERNET hRequest) {
        DWORD code = 0, sz = sizeof(code);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
        return code;
    }

    inline std::string ReadFullBody(HINTERNET hRequest) {
        std::string body;
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
            std::vector<char> buf(avail);
            DWORD read = 0;
            WinHttpReadData(hRequest, buf.data(), avail, &read);
            body.append(buf.data(), read);
        }
        return body;
    }

    // ── Synchronous HTTP request ────────────────────────────────────────

    struct HttpResponse { DWORD statusCode = 0; std::string body; };

    inline HttpResponse WinHttpRequest(const std::string& method, const std::string& url,
                                       const std::string& body = {}) {
        auto parsed = ParseUrl(url);
        auto h = OpenRequest(parsed, ToWide(method).c_str());
        auto hdr = ToWide("Content-Type: application/json");
        if (!WinHttpSendRequest(h.request.get(), hdr.c_str(), (DWORD)hdr.size(),
                body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
                (DWORD)body.size(), (DWORD)body.size(), 0))
            throw std::runtime_error("WinHttpSendRequest failed");
        if (!WinHttpReceiveResponse(h.request.get(), nullptr))
            throw std::runtime_error("WinHttpReceiveResponse failed");
        return {GetStatusCode(h.request.get()), ReadFullBody(h.request.get())};
    }

    // ── SSE streaming ───────────────────────────────────────────────────

    /// Normalize \r\n → \n, appending to `out`. Tracks \r at buffer boundaries via `lastCR`.
    inline void NormalizeAppend(const char* data, DWORD len, std::string& out, bool& lastCR) {
        for (DWORD i = 0; i < len; i++) {
            if (lastCR) {
                lastCR = false;
                if (data[i] == '\n') { out += '\n'; continue; }
                out += '\n';
            }
            if (data[i] == '\r') {
                if (i + 1 < len && data[i + 1] == '\n') { out += '\n'; i++; }
                else if (i + 1 < len) out += '\n';
                else lastCR = true;
            } else {
                out += data[i];
            }
        }
    }

    /// Extracts the `data:` payload from an SSE block.
    inline std::string ExtractSSEData(const std::string& block) {
        std::string result;
        size_t pos = 0;
        while (pos < block.size()) {
            auto end = block.find('\n', pos);
            if (end == std::string::npos) end = block.size();
            std::string_view line(block.data() + pos, end - pos);
            if (line.substr(0, 6) == "data: ") {
                if (!result.empty()) result += '\n';
                result += line.substr(6);
            }
            pos = end + 1;
        }
        return result;
    }

    inline void WinHttpStreamSSE(const std::string& url, const std::string& body,
                                 const std::function<void(const std::string&)>& onEvent) {
        auto parsed = ParseUrl(url);
        auto h = OpenRequest(parsed, L"POST");
        std::wstring hdrs = L"Content-Type: application/json\r\nAccept: text/event-stream";
        if (!WinHttpSendRequest(h.request.get(), hdrs.c_str(), (DWORD)hdrs.size(),
                const_cast<char*>(body.data()), (DWORD)body.size(), (DWORD)body.size(), 0))
            throw std::runtime_error("WinHttpSendRequest failed (streaming)");
        if (!WinHttpReceiveResponse(h.request.get(), nullptr))
            throw std::runtime_error("WinHttpReceiveResponse failed (streaming)");

        auto code = GetStatusCode(h.request.get());
        if (code < 200 || code >= 300)
            throw std::runtime_error("Streaming error (" + std::to_string(code) + "): " + ReadFullBody(h.request.get()));

        std::string buffer;
        bool lastCR = false;
        char readBuf[4096];

        while (true) {
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(h.request.get(), &avail))
                break;
            if (avail == 0) {
                // No data available right now; check if the response is complete.
                // A second query returning 0 means the stream has ended.
                if (!WinHttpQueryDataAvailable(h.request.get(), &avail) || avail == 0)
                    break;
            }

            DWORD toRead = (std::min)(avail, static_cast<DWORD>(sizeof(readBuf)));
            DWORD bytesRead = 0;
            if (!WinHttpReadData(h.request.get(), readBuf, toRead, &bytesRead) || bytesRead == 0)
                break;

            NormalizeAppend(readBuf, bytesRead, buffer, lastCR);

            size_t pos = 0;
            size_t blockEnd;
            while ((blockEnd = buffer.find("\n\n", pos)) != std::string::npos) {
                auto block = buffer.substr(pos, blockEnd - pos);
                pos = blockEnd + 2;

                // Trim leading whitespace
                auto start = block.find_first_not_of(" \n");
                if (start == std::string::npos) continue;
                if (start > 0) block.erase(0, start);

                if (block == "data: [DONE]") return;

                auto data = ExtractSSEData(block);
                if (!data.empty()) onEvent(data);
            }
            buffer.erase(0, pos);
        }
    }

} // namespace foundry_local::detail
