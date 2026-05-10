// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Example: Tool calling with the Foundry Local C++ SDK.
// Demonstrates: defining tools, handling tool calls, and returning tool results
// using static factories on Item and convenience free functions (UserMessage, etc.).

#include <foundry_local/foundry_local_cpp.h>

#include <iostream>
#include <string>

// Simulated tool implementation.
static std::string get_weather(const std::string& /*location*/) {
  return R"({"temperature": 22, "unit": "celsius", "condition": "sunny"})";
}

int main() {
  using namespace foundry_local;

  try {
    // 1. Setup: create manager, get model, load it.
    Configuration config("tool_calling_example");
    Manager manager(std::move(config));
    auto& catalog = manager.GetCatalog();
    auto model = catalog.GetModel("qwen2.5-0.5b");
    if (!model) {
      std::cerr << "Model not found.\n";
      return 1;
    }

    if (!model->IsCached()) {
      std::cout << "Downloading model..." << std::flush;
      model->Download([](float pct) {
        std::cout << "\rDownloading model... " << static_cast<int>(pct) << "%" << std::flush;
        return true;  // continue
      });
      std::cout << "\n";
    }

    if (!model->IsLoaded()) {
      model->Load();
    }

    {  // scope for session
      ChatSession session(*model);

      // 2. Define the tool the model can invoke.
      const char* weather_schema = R"({
        "type": "object",
        "properties": {
          "location": { "type": "string", "description": "City name" }
        },
        "required": ["location"]
      })";

      ToolDefinition tool_definition{
          "get_weather",
          "Get current weather for a location",
          weather_schema,
      };

      // Add tool definition to the session so it's available across multiple ProcessRequest calls.
      session.AddToolDefinition(tool_definition);

      // 3. Build the request with the user query.
      Request request;
      request.AddItem(UserMessage("What's the weather in Paris?"));

      // Set tool_choice to "required" so the model must call a tool on the first turn.
      // Also limit output tokens to prevent runaway generation.
      request.SetOptions({{FOUNDRY_LOCAL_PARAM_TOOL_CHOICE, "required"},
                          {FOUNDRY_LOCAL_PARAM_MAX_OUTPUT_TOKENS, "256"}});

      // 4. Send requests in a loop — handle tool calls until the model produces a final message.
      Response response = session.ProcessRequest(request);

      while (true) {
        const auto& items = response.GetItems();
        if (items.empty()) {
          throw Error("Response contained no items", FOUNDRY_LOCAL_ERROR_INTERNAL);
        }

        const auto& item = items.front();  // either message or tool call request in this example

        // Check if final response
        if (item.GetType() == FOUNDRY_LOCAL_ITEM_MESSAGE) {
          auto msg = item.GetMessage();
          std::cout << "Assistant: " << msg.GetSimpleText() << "\n";
          break;
        }

        // Handle tool call request
        if (item.GetType() == FOUNDRY_LOCAL_ITEM_TOOL_CALL) {
          auto [call_id, tool_name, arguments] = item.GetToolCall();
          std::cout << "Tool call: " << tool_name << "(" << arguments << ")\n";

          // 5. Execute the tool and send the result back.
          std::string result = get_weather("Paris");

          // feed the tool result back into the model as a new request.
          // the session maintains state from the previous request so we only need to send in new item.
          // Clear tool_choice so the model can produce a final text response.
          Request tool_request;
          tool_request.AddItem(Item::ToolResult(std::string(call_id), result));
          tool_request.SetOptions({{FOUNDRY_LOCAL_PARAM_TOOL_CHOICE, "none"},
                                   {FOUNDRY_LOCAL_PARAM_MAX_OUTPUT_TOKENS, "256"}});

          // Send the tool result back to the model and get the next response.
          response = session.ProcessRequest(tool_request);
          continue;
        }

        throw Error("Unexpected item type in response", FOUNDRY_LOCAL_ERROR_INTERNAL);
      }
    }  // release session

    model->Unload();

  } catch (const Error& ex) {
    std::cerr << "Foundry Local error [" << ex.Code() << "]: " << ex.what()
              << "\n";
    return 1;
  }

  return 0;
}
