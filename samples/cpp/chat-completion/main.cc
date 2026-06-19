// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Sample: Chat completions with the Foundry Local C++ SDK (sdk_v2/cpp), shown two
// ways with the *same* prompt so you can compare them:
//
//   1. Native, in-process inference via ChatSession (non-streaming + streaming).
//   2. The embedded OpenAI-compatible web service: host it with
//      AddWebServiceEndpoint + StartWebService, then POST /v1/chat/completions.
//
// Both paths use the same loaded model — the web service reuses the in-process
// model the SDK already loaded.

#include <foundry_local/foundry_local_cpp.h>

#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <vector>

#include "common/local_http_client.h"

using namespace foundry_local;
using json = nlohmann::json;

namespace {

constexpr const char* kModelAlias = "qwen2.5-0.5b";
constexpr const char* kPrompt = "What is the capital of France?";

// ---------------------------------------------------------------------------
// JSON contract types for POST /v1/chat/completions (OpenAI shape).
// ---------------------------------------------------------------------------

struct ChatMessage {
  std::string role;
  std::string content;
};

struct ChatCompletionRequest {
  std::string model;
  std::vector<ChatMessage> messages;
  bool stream = false;
};

void to_json(json& j, const ChatMessage& m) {
  j = json{{"role", m.role}, {"content", m.content}};
}

void to_json(json& j, const ChatCompletionRequest& r) {
  j = json{{"model", r.model}, {"messages", r.messages}, {"stream", r.stream}};
}

// ---------------------------------------------------------------------------
// Native, in-process inference.
// ---------------------------------------------------------------------------

void NativeNonStreaming(IModel& model) {
  ChatSession session(model);

  Request request{UserMessage(kPrompt)};
  Response response = session.ProcessRequest(request);

  for (const auto& item : response.GetItems()) {
    if (item.GetType() == FOUNDRY_LOCAL_ITEM_MESSAGE) {
      std::cout << "Assistant: " << item.GetMessage().GetSimpleText() << "\n";
    }
  }

  const flUsage usage = response.GetUsage();
  std::cout << "Tokens — prompt: " << usage.prompt_tokens << ", completion: " << usage.completion_tokens
            << ", total: " << usage.total_tokens << "\n";
}

void NativeStreaming(IModel& model) {
  ChatSession session(model);

  // Each callback delivers exactly one item from the queue; we print TEXT items as they arrive.
  session.SetStreamingCallback([](flStreamingCallbackData event) -> int {
    const auto* item_api = detail::item_api();

    flItem* raw_item = nullptr;
    if (item_api->ItemQueue_TryPop(event.item_queue, &raw_item)) {
      Item item(*raw_item);
      if (item.GetType() == FOUNDRY_LOCAL_ITEM_TEXT) {
        std::cout << item.GetText().text << std::flush;
      }
    }

    return 0;  // return non-zero to cancel
  });

  Request request{UserMessage(kPrompt)};

  std::cout << "Assistant: ";
  session.ProcessRequest(request);
  std::cout << "\n";
}

// ---------------------------------------------------------------------------
// Web service inference: POST the same prompt to /v1/chat/completions.
// ---------------------------------------------------------------------------

void WebServiceChat(Manager& manager, IModel& model) {
  manager.StartWebService();

  const std::vector<std::string> endpoints = manager.GetWebServiceEndpoints();
  if (endpoints.empty()) {
    std::cerr << "Web service did not report any endpoints.\n";
    return;
  }

  const sample::http::Url url = sample::http::ParseUrl(endpoints[0]);
  std::cout << "Web service listening at " << endpoints[0] << "\n";

  // The web service resolves models by their full variant id, not the alias.
  ChatCompletionRequest request{.model = std::string(model.GetInfo().Id()),
                                .messages = {{"user", kPrompt}},
                                .stream = false};

  const json body = request;
  const sample::http::Response response =
      sample::http::Post(url.host, url.port, "/v1/chat/completions", body.dump());

  if (response.status != 200) {
    std::cerr << "HTTP " << response.status << ": " << response.body << "\n";
    manager.StopWebService();
    return;
  }

  const json parsed = json::parse(response.body);
  const std::string content = parsed["choices"][0]["message"].value("content", "");
  std::cout << "Assistant: " << content << "\n";

  if (parsed.contains("usage")) {
    const auto& usage = parsed["usage"];
    std::cout << "Tokens — prompt: " << usage.value("prompt_tokens", 0)
              << ", completion: " << usage.value("completion_tokens", 0)
              << ", total: " << usage.value("total_tokens", 0) << "\n";
  }

  manager.StopWebService();
}

}  // namespace

int main() {
  try {
    // 1. Configure the SDK and request an embedded web service endpoint
    //    (ephemeral port — the bound URL is reported by GetWebServiceEndpoints()).
    Configuration config("foundry_local_samples");
    config.AddWebServiceEndpoint("http://127.0.0.1:0");

    Manager manager(std::move(config));

    // 2. Resolve the chat model and prepare it.
    auto& catalog = manager.GetCatalog();
    auto model = catalog.GetModel(kModelAlias);
    if (!model) {
      std::cerr << "Model '" << kModelAlias << "' not found in catalog.\n";
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

    // 3. Run the same prompt three ways.
    std::cout << "\n=== Native in-process (non-streaming) ===\n";
    NativeNonStreaming(*model);

    std::cout << "\n=== Native in-process (streaming) ===\n";
    NativeStreaming(*model);

    std::cout << "\n=== Local web server (POST /v1/chat/completions) ===\n";
    WebServiceChat(manager, *model);

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
