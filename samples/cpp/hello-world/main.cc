// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <foundry_local/foundry_local_cpp.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "samples/cpp/sample_common.h"

int main() {
  using namespace foundry_local;

  try {
    // Manager owns catalog state and must outlive every model and session obtained from it.
    Configuration configuration{std::string(sample::kAppName)};
    Manager manager(std::move(configuration));

    const auto execution_providers = manager.GetDiscoverableEps();
    std::cout << "Execution providers:\n";
    for (const auto& ep : execution_providers) {
      std::cout << "  " << ep.name << (ep.is_registered ? " [registered]" : " [available]") << "\n";
    }

    manager.DownloadAndRegisterEps({}, [](std::string_view name, float percent) {
      std::cout << "\rRegistering " << name << ": " << static_cast<int>(percent) << "%" << std::flush;
      return true;
    });
    std::cout << "\n";

    ICatalog& catalog = manager.GetCatalog();
    auto model = sample::GetModel(catalog, "qwen2.5-0.5b");

    // Select a CPU chat variant explicitly. Applications may instead select a GPU/NPU variant.
    bool selected = false;
    auto variants = model->GetVariants();
    for (const auto& variant : variants) {
      const auto info = variant->GetInfo();
      if (info.DeviceType() == FOUNDRY_LOCAL_DEVICE_CPU && info.Task() == "chat-completion") {
        model->SelectVariant(*variant);
        selected = true;
        break;
      }
    }
    if (!selected) {
      throw std::runtime_error("No CPU chat variant is available for qwen2.5-0.5b");
    }

    const auto info = model->GetInfo();
    std::cout << "Selected " << info.Id() << "\n";
    if (!model->IsCached()) {
      model->Download([](float percent) {
        std::cout << "\rDownloading model: " << static_cast<int>(percent) << "%" << std::flush;
        return 0;
      });
      std::cout << "\n";
    } else {
      std::cout << "Model is already cached at " << model->GetPath() << "\n";
    }

    model->Load();
    {
      Session session(*model);
      Request request{UserMessage("In one sentence, what can I build with Foundry Local?")};
      const auto response = session.ProcessRequest(request);
      std::cout << "Assistant: ";
      sample::PrintAssistantMessages(response);
    }
    model->Unload();
    manager.Shutdown();
  } catch (const Error& error) {
    std::cerr << "Foundry Local error [" << error.Code() << "]: " << error.what() << "\n";
    return 1;
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << "\n";
    return 1;
  }

  return 0;
}
