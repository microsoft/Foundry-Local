// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Sample: Vision / image understanding via the Foundry Local embedded web service
// and the OpenAI Responses API (POST /v1/responses).
//
// Flow:
//   1. Host the web service (AddWebServiceEndpoint + StartWebService).
//   2. Load a vision-capable model.
//   3. Base64-encode a local image into a `data:` URL.
//   4. POST /v1/responses with an `input_text` + `input_image` message.
//   5. Print the model's description from the response's `output_text`.
//
// The Responses API is only exposed over the web service, so — unlike the chat
// and embeddings samples — vision here goes through HTTP rather than a native
// in-process session.

#include <foundry_local/foundry_local_cpp.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "common/base64.h"
#include "common/local_http_client.h"

using namespace foundry_local;
using json = nlohmann::json;

namespace {

// Default vision model alias (overridable on the command line), matching the
// other-language responses-vision samples.
constexpr const char* kDefaultModelAlias = "qwen3.5-0.8b";

/// Read an entire file into a byte buffer. Throws std::runtime_error if it can't be opened.
std::vector<uint8_t> ReadFileBytes(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to open image: " + path.string());
  }

  return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

/// Build the /v1/responses request body: one user message with text + image content.
json BuildVisionRequest(const std::string& model_id, const std::string& data_url, const std::string& prompt) {
  return json{
      {"model", model_id},
      {"stream", false},
      {"input",
       json::array({{{"type", "message"},
                     {"role", "user"},
                     {"content", json::array({{{"type", "input_text"}, {"text", prompt}},
                                              {{"type", "input_image"}, {"image_url", data_url}}})}}})}};
}

}  // namespace

int main(int argc, char* argv[]) {
  const std::string model_alias = argc > 1 ? argv[1] : kDefaultModelAlias;

  // Default to the bundled test image; allow an override as the second argument.
  std::filesystem::path image_path =
      argc > 2 ? std::filesystem::path(argv[2]) : std::filesystem::path(SAMPLE_SOURCE_DIR) / "test_image.jpg";

  try {
    // 1. Configure the SDK with an embedded web service endpoint.
    Configuration config("foundry_local_samples");
    config.AddWebServiceEndpoint("http://127.0.0.1:0");

    Manager manager(std::move(config));

    // 2. Resolve and prepare the vision model.
    auto& catalog = manager.GetCatalog();
    auto model = catalog.GetModel(model_alias);
    if (!model) {
      std::cerr << "Model '" << model_alias << "' not found in catalog.\n";
      return 1;
    }

    std::cout << "Using model: " << model->GetInfo().Name() << "\n";

    if (!model->IsCached()) {
      std::cout << "Downloading...\n";
      model->Download([](float progress) -> int {
        std::cout << "\r  " << static_cast<int>(progress) << "%" << std::flush;
        return 0;  // return non-zero to cancel
      });
      std::cout << "\n";
    }

    if (!model->IsLoaded()) {
      std::cout << "Loading model...\n";
      model->Load();
    }

    // 3. Start the web service and discover its bound URL.
    std::cout << "\n=== Starting web service ===\n";
    manager.StartWebService();

    const std::vector<std::string> endpoints = manager.GetWebServiceEndpoints();
    if (endpoints.empty()) {
      std::cerr << "Web service did not report any endpoints.\n";
      return 1;
    }

    const sample::http::Url url = sample::http::ParseUrl(endpoints[0]);
    std::cout << "Web service listening at " << endpoints[0] << "\n";

    // 4. Encode the image as a base64 data URL (the Responses API requires a
    //    `data:<mime>;base64,<payload>` URL or a local file path for input_image).
    std::cout << "\n=== Vision request (POST /v1/responses) ===\n";
    std::cout << "Image: " << image_path.string() << "\n";

    const std::vector<uint8_t> image_bytes = ReadFileBytes(image_path);
    const std::string data_url = "data:image/jpeg;base64," + sample::Base64Encode(image_bytes);

    const json body = BuildVisionRequest(std::string(model->GetInfo().Id()), data_url, "Describe this image in detail.");

    const sample::http::Response response = sample::http::Post(url.host, url.port, "/v1/responses", body.dump());

    if (response.status != 200) {
      std::cerr << "HTTP " << response.status << ": " << response.body << "\n";
      manager.StopWebService();
      return 1;
    }

    // 5. Print the assistant's description.
    const json parsed = json::parse(response.body);
    std::cout << "\nAssistant: " << parsed.value("output_text", "") << "\n";

    manager.StopWebService();
    model->Unload();
  } catch (const Error& ex) {
    std::cerr << "Foundry Local error [" << ex.Code() << "]: " << ex.what() << "\n";
    return 1;
  } catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }

  return 0;
}
