// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "http/http_client.h"
#include "exception.h"

#include <azure/core/context.hpp>
#include <azure/core/datetime.hpp>
#include <azure/core/http/curl_transport.hpp>
#include <azure/core/http/http.hpp>
#include <azure/core/http/raw_response.hpp>
#include <azure/core/io/body_stream.hpp>

#include <chrono>
#include <string>
#include <vector>

namespace fl
{
  namespace http
  {

    namespace
    {

      std::string HttpRequest(const Azure::Core::Http::HttpMethod &method,
                              const std::string &url,
                              const std::string &body,
                              const std::string &user_agent,
                              bool close_connection)
      {
        using namespace Azure::Core;
        using namespace Azure::Core::Http;

        CurlTransport transport;

        // Build the request. For methods with a body (POST), attach a MemoryBodyStream.
        std::vector<uint8_t> body_bytes(body.begin(), body.end());
        IO::MemoryBodyStream body_stream(body_bytes);

        Request request = body.empty()
                              ? Request(method, Url(url))
                              : Request(method, Url(url), &body_stream);

        request.SetHeader("User-Agent", user_agent);

        if (close_connection)
        {
          request.SetHeader("Connection", "close");
        }

        if (!body.empty())
        {
          request.SetHeader("Content-Type", "application/json");
        }

        // 30 second timeout
        Context context = Context{}.WithDeadline(
            Azure::DateTime(std::chrono::system_clock::now() + std::chrono::seconds(30)));

        std::unique_ptr<RawResponse> response;
        try
        {
          response = transport.Send(request, context);
        }
        catch (const std::exception &e)
        {
          FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL, "HTTP request failed for " + url + " (" + e.what() + ")");
        }

        auto status = static_cast<int>(response->GetStatusCode());

        // Read the response body — prefer the stream if available, otherwise use the buffered body.
        std::string response_body;
        auto response_stream = response->ExtractBodyStream();

        if (response_stream)
        {
          auto bytes = response_stream->ReadToEnd(context);
          response_body.assign(bytes.begin(), bytes.end());
        }
        else
        {
          auto &bytes = response->GetBody();
          response_body.assign(bytes.begin(), bytes.end());
        }

        if (status < 200 || status >= 300)
        {
          FL_THROW(FOUNDRY_LOCAL_ERROR_INTERNAL, "HTTP " + std::to_string(status) + " from " + url +
                                                     ": " + response_body);
        }

        return response_body;
      }

    } // anonymous namespace

    std::string HttpGet(const std::string &url, const std::string &user_agent, bool close_connection)
    {
      return HttpRequest(Azure::Core::Http::HttpMethod::Get, url, "", user_agent, close_connection);
    }

    std::string HttpPost(const std::string &url, const std::string &json_body,
                         const std::string &user_agent, bool close_connection)
    {
      return HttpRequest(Azure::Core::Http::HttpMethod::Post, url, json_body, user_agent, close_connection);
    }

    std::string HttpDelete(const std::string &url, const std::string &user_agent, bool close_connection)
    {
      return HttpRequest(Azure::Core::Http::HttpMethod::Delete, url, "", user_agent, close_connection);
    }

  } // namespace http
} // namespace fl
