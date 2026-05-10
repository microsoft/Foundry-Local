// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "http/http_download.h"

#include "logger.h"
#include "util/make_string.h"

#include <azure/core/context.hpp>
#include <azure/core/http/curl_transport.hpp>
#include <azure/core/http/http.hpp>
#include <azure/core/io/body_stream.hpp>

#include <filesystem>
#include <fstream>

namespace fl {

bool HttpDownloadFile(const std::string& url,
                      const std::filesystem::path& destination,
                      const std::string& user_agent,
                      std::atomic<bool>* cancel_flag,
                      std::function<void(float percent)> progress_cb,
                      ILogger& logger) {
  using namespace Azure::Core;
  using namespace Azure::Core::Http;

  // Ensure parent directory exists
  auto parent = destination.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }

  CurlTransport transport;
  Request request(HttpMethod::Get, Url(url));
  request.SetHeader("User-Agent", user_agent);

  // Long timeout for large downloads (30 minutes)
  Context context = Context{}.WithDeadline(
      Azure::DateTime(std::chrono::system_clock::now() + std::chrono::minutes(30)));

  std::unique_ptr<RawResponse> response;

  try {
    response = transport.Send(request, context);
  } catch (const std::exception& ex) {
    logger.Log(LogLevel::Warning, MakeString("HTTP download failed for ", url, ": ", ex.what()));
    return false;
  } catch (...) {
    logger.Log(LogLevel::Warning, MakeString("HTTP download failed for ", url, ": unknown exception"));
    return false;
  }

  auto status = static_cast<int>(response->GetStatusCode());
  if (status < 200 || status >= 300) {
    logger.Log(LogLevel::Warning, MakeString("HTTP download failed for ", url, ": HTTP status ", status));
    return false;
  }

  auto body_stream = response->ExtractBodyStream();
  if (!body_stream) {
    logger.Log(LogLevel::Warning, MakeString("HTTP download failed for ", url, ": no body stream in response"));
    return false;
  }

  // Get content length for progress reporting
  int64_t content_length = -1;
  auto cl_header = response->GetHeaders().find("content-length");
  if (cl_header != response->GetHeaders().end()) {
    content_length = std::stoll(cl_header->second);
  }

  std::ofstream out(destination, std::ios::binary);
  if (!out) {
    logger.Log(LogLevel::Warning,
               MakeString("HTTP download: failed to open output file ", destination.string()));
    return false;
  }

  constexpr size_t kBufferSize = 65536;
  uint8_t buffer[kBufferSize];
  int64_t bytes_downloaded = 0;
  int chunks_since_progress = 0;

  while (true) {
    if (cancel_flag && cancel_flag->load()) {
      out.close();
      std::filesystem::remove(destination);
      return false;
    }

    size_t bytes_read = body_stream->Read(buffer, kBufferSize, context);
    if (bytes_read == 0) {
      break;
    }

    out.write(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(bytes_read));
    bytes_downloaded += static_cast<int64_t>(bytes_read);

    // Report progress every 32 chunks (~2MB), matching C# behavior
    chunks_since_progress++;
    if (progress_cb && content_length > 0 && chunks_since_progress >= 32) {
      float percent = static_cast<float>(bytes_downloaded * 100.0 / content_length);
      progress_cb(percent);
      chunks_since_progress = 0;
    }
  }

  out.close();

  if (progress_cb) {
    progress_cb(100.0f);
  }

  return true;
}

}  // namespace fl
