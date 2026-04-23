// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "foundry_local.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace foundry_local;

// ---------------------------------------------------------------------------
// Logger
// ---------------------------------------------------------------------------
class StdLogger final : public ILogger {
public:
    void Log(LogLevel level, std::string_view message) noexcept override {
        const char* tag = "UNK";
        switch (level) {
        case LogLevel::Information:
            tag = "INFO";
            break;
        case LogLevel::Warning:
            tag = "WARN";
            break;
        case LogLevel::Error:
            tag = "ERROR";
            break;
        default:
            tag = "DEBUG";
            break;
        }
        std::cout << "[FoundryLocal][" << tag << "] " << message << "\n";
    }
};

// ---------------------------------------------------------------------------
// Helper – Select the CPU variant for a model (if available)
// ---------------------------------------------------------------------------
void PreferCpuVariant(Model& model) {
    for (const auto& variant : model.GetVariants()) {
        const auto& info = variant.GetInfo();
        if (info.runtime && info.runtime->device_type == DeviceType::CPU) {
            model.SelectVariant(variant);
            std::cout << "Selected CPU variant: " << info.name << "\n";
            return;
        }
    }
    std::cout << "No CPU variant found; using default variant.\n";
}

// ---------------------------------------------------------------------------
// Example 1 – Browse the catalog
// ---------------------------------------------------------------------------
void BrowseCatalog(Manager& manager) {
    std::cout << "\n=== Example 1: Browse Catalog ===\n";

    auto& catalog = manager.GetCatalog();
    std::cout << "Catalog: " << catalog.GetName() << "\n";

    auto models = catalog.GetModels();
    std::cout << "Models in catalog: " << models.size() << "\n";

    for (const auto* model : models) {
        std::cout << "  - " << model->GetAlias() << " (" << model->GetId() << ")"
                  << "  cached=" << (model->IsCached() ? "yes" : "no")
                  << "  loaded=" << (model->IsLoaded() ? "yes" : "no") << "\n";

        auto* concreteModel = dynamic_cast<const Model*>(model);
        if (!concreteModel) continue;
        for (const auto& variant : concreteModel->GetVariants()) {
            const auto& info = variant.GetInfo();
            std::cout << "      variant: " << info.name << "  v" << info.version
                      << "  cached=" << (variant.IsCached() ? "yes" : "no");
            if (info.display_name)
                std::cout << "  display=\"" << *info.display_name << "\"";
            if (info.publisher)
                std::cout << "  publisher=" << *info.publisher;
            if (info.license)
                std::cout << "  license=" << *info.license;
            if (info.runtime) {
                std::cout << "  device="
                          << (info.runtime->device_type == DeviceType::GPU   ? "GPU"
                              : info.runtime->device_type == DeviceType::NPU ? "NPU"
                                                                             : "CPU")
                          << "  ep=" << info.runtime->execution_provider;
            }
            if (info.file_size_mb)
                std::cout << "  size=" << *info.file_size_mb << "MB";
            if (info.supports_tool_calling)
                std::cout << "  tools=" << (*info.supports_tool_calling ? "yes" : "no");
            std::cout << "\n";
        }
    }
}

// ---------------------------------------------------------------------------
// Example 2 – Download, load, chat (non-streaming), then unload
// ---------------------------------------------------------------------------
void ChatNonStreaming(Manager& manager, const std::string& alias) {
    std::cout << "\n=== Example 2: Non-Streaming Chat ===\n";

    auto& catalog = manager.GetCatalog();

    auto* model = catalog.GetModel(alias);
    if (!model) {
        std::cerr << "Model '" << alias << "' not found in catalog.\n";
        return;
    }

    // Prefer CPU variant to avoid DML/GPU provider issues
    if (auto* concreteModel = dynamic_cast<Model*>(model)) {
        PreferCpuVariant(*concreteModel);
    }

    model->Download([](float pct) { printf("\rDownloading: %5.1f%%", pct); fflush(stdout); return true; });
    std::cout << "\n";

    model->Load();

    if (model->IsLoaded()) {
        std::cout << "Model is loaded and ready for inference.\n";
    }
    else {
        std::cerr << "Failed to load model.\n";
        return;
    }

    OpenAIChatClient chat(*model);

    std::vector<ChatMessage> messages = {{"user", "What is the capital of Croatia?"}};

    ChatSettings settings;
    settings.temperature = 0.7f;
    settings.max_tokens = 128;

    auto response = chat.CompleteChat(messages, settings);

    if (!response.choices.empty() && response.choices[0].message) {
        std::cout << "Assistant: " << response.choices[0].message->content << "\n";
    }

    model->Unload();
    std::cout << "Model unloaded.\n";
}

// ---------------------------------------------------------------------------
// Example 3 – Streaming chat
// ---------------------------------------------------------------------------
void ChatStreaming(Manager& manager, const std::string& alias) {
    std::cout << "\n=== Example 3: Streaming Chat ===\n";

    auto& catalog = manager.GetCatalog();

    auto* model = catalog.GetModel(alias);
    if (!model) {
        std::cerr << "Model '" << alias << "' not found in catalog.\n";
        return;
    }

    // Prefer CPU variant to avoid DML/GPU provider issues
    if (auto* concreteModel = dynamic_cast<Model*>(model)) {
        PreferCpuVariant(*concreteModel);
    }

    model->Load();

    OpenAIChatClient chat(*model);

    std::vector<ChatMessage> messages = {{"user", "Explain quantum computing in three sentences."}};

    ChatSettings settings;
    settings.temperature = 0.9f;
    settings.max_tokens = 256;

    std::cout << "Assistant: ";
    chat.CompleteChatStreaming(messages, settings, [](const ChatCompletionCreateResponse& chunk) {
        if (chunk.choices.empty())
            return;
        const auto& choice = chunk.choices[0];
        if (choice.delta && !choice.delta->content.empty()) {
            std::cout << choice.delta->content << std::flush;
        }
    });
    std::cout << "\n";

    model->Unload();
}

// ---------------------------------------------------------------------------
// Example 4 – Audio transcription
// ---------------------------------------------------------------------------
void TranscribeAudio(Manager& manager, const std::string& alias, const std::string& audioPath) {
    std::cout << "\n=== Example 4: Audio Transcription ===\n";

    auto& catalog = manager.GetCatalog();

    auto* model = catalog.GetModel(alias);
    if (!model) {
        std::cerr << "Model '" << alias << "' not found in catalog.\n";
        return;
    }

    // Prefer CPU variant to avoid DML/GPU provider issues
    if (auto* concreteModel = dynamic_cast<Model*>(model)) {
        PreferCpuVariant(*concreteModel);
    }

    model->Download([](float pct) { printf("\rDownloading: %5.1f%%", pct); fflush(stdout); return true; });
    std::cout << "\n";

    model->Load();

    OpenAIAudioClient audio(*model);

    std::cout << "Transcribing: " << audioPath << "\n";
    auto result = audio.TranscribeAudio(audioPath);
    std::cout << "Transcription: " << result.text << "\n";

    // Streaming alternative:
    audio.TranscribeAudioStreaming(
        audioPath, [](const AudioCreateTranscriptionResponse& chunk) { std::cout << chunk.text << std::flush; });
    std::cout << "\n";

    model->Unload();
}

// ---------------------------------------------------------------------------
// Example 5 – Tool calling
// ---------------------------------------------------------------------------
// Tool calling lets you define functions that the model can decide to invoke.
// The flow is:
//   1. You describe your tools (functions) as ToolDefinition objects.
//   2. You send a chat request with those tools attached.
//   3. The model may respond with finish_reason = ToolCalls and include
//      ToolCall objects in the message, each containing the function name
//      and a JSON string of arguments.
//   4. YOUR CODE executes the real function using those arguments.
//   5. You add a message with role = "tool" containing the result, then
//      send the conversation back so the model can formulate a final answer.
//
// This lets the model "reach out" to external capabilities (calculators,
// databases, APIs, etc.) while keeping the actual execution in your code.
// ---------------------------------------------------------------------------
void ChatWithToolCalling(Manager& manager, const std::string& alias) {
    std::cout << "\n=== Example 5: Tool Calling ===\n";

    auto& catalog = manager.GetCatalog();

    auto* model = catalog.GetModel(alias);
    if (!model) {
        std::cerr << "Model '" << alias << "' not found in catalog.\n";
        return;
    }

    // Prefer CPU variant to avoid DML/GPU provider issues
    if (auto* concreteModel = dynamic_cast<Model*>(model)) {
        PreferCpuVariant(*concreteModel);
    }

    model->Download([](float pct) { printf("\rDownloading: %5.1f%%", pct); fflush(stdout); return true; });
    std::cout << "\n";

    model->Load();
    std::cout << "Model loaded: " << model->GetAlias() << "\n";

    OpenAIChatClient chat(*model);

    // ── Step 1: Define tools ──────────────────────────────────────────────
    // Each tool describes a function the model can call.  The PropertyDefinition
    // mirrors a JSON Schema so the model knows what arguments are expected.
    std::vector<ToolDefinition> tools = {
        {"function",
         FunctionDefinition{"multiply_numbers",                             // function name
                            "Multiply two integers and return the result.", // description
                            PropertyDefinition{
                                "object",     // top-level schema type
                                std::nullopt, // no top-level description
                                std::unordered_map<std::string, PropertyDefinition>{
                                    {"first", PropertyDefinition{"integer", "The first number"}},
                                    {"second", PropertyDefinition{"integer", "The second number"}}},
                                std::vector<std::string>{"first", "second"} // both params are required
                            }}}};

    // ── Step 2: Send the first request ────────────────────────────────────
    // tool_choice = Required forces the model to always produce a tool call.
    // In production you'd typically use Auto so the model decides on its own.
    std::vector<ChatMessage> messages = {
        {"system", "You are a helpful AI assistant. Use the provided tools when appropriate."},
        {"user", "What is 7 multiplied by 6?"}};

    ChatSettings settings;
    settings.temperature = 0.0f;
    settings.max_tokens = 500;
    settings.tool_choice = ToolChoiceKind::Required;

    std::cout << "Sending chat request with tool definitions...\n";
    auto response = chat.CompleteChat(messages, tools, settings);

    // ── Step 3: Inspect the model's tool call ─────────────────────────────
    if (response.choices.empty()) {
        std::cerr << "No choices returned.\n";
        model->Unload();
        return;
    }

    const auto& firstChoice = response.choices[0];

    // The model signals it wants to call a tool via finish_reason == ToolCalls.
    if (firstChoice.finish_reason == FinishReason::ToolCalls && firstChoice.message &&
        !firstChoice.message->tool_calls.empty()) {
        const auto& tc = firstChoice.message->tool_calls[0];
        std::cout << "Model requested tool call:\n"
                  << "  function : " << (tc.function_call ? tc.function_call->name : "(none)") << "\n"
                  << "  arguments: " << (tc.function_call ? tc.function_call->arguments : "{}") << "\n";

        // ── Step 4: Execute the tool locally ──────────────────────────────
        // Parse the arguments JSON and perform the actual computation.
        // In a real application this could be a web request, DB query, etc.
        std::string toolResult;
        if (tc.function_call && tc.function_call->name == "multiply_numbers") {
            // The arguments string is JSON, e.g. {"first": 7, "second": 6}
            // For brevity we hard-code the expected result here.
            toolResult = "7 x 6 = 42.";
            std::cout << "  result   : " << toolResult << "\n";
        }
        else {
            toolResult = "Unknown tool.";
        }

        // ── Step 5: Feed the tool result back ─────────────────────────────
        // First, append the assistant message that contains the tool_calls
        // so the model sees its own request in the conversation history.
        messages.push_back({"assistant", "", std::nullopt, firstChoice.message->tool_calls});

        // Then add a "tool" message with the result, referencing the
        // tool_call_id so the model can match it to the call it made.
        messages.push_back({"tool", toolResult, tc.id});

        // Switch to Auto so the model can answer without calling tools again.
        settings.tool_choice = ToolChoiceKind::Auto;

        std::cout << "\nSending tool result back to model...\n";
        auto followUp = chat.CompleteChat(messages, tools, settings);

        if (!followUp.choices.empty() && followUp.choices[0].message) {
            std::cout << "Assistant: " << followUp.choices[0].message->content << "\n";
        }
    }
    else {
        // The model answered directly without a tool call.
        if (firstChoice.message)
            std::cout << "Assistant: " << firstChoice.message->content << "\n";
    }

    model->Unload();
    std::cout << "Model unloaded.\n";
}

// ---------------------------------------------------------------------------
// Example 6 – Embeddings (single and batch)
// ---------------------------------------------------------------------------
void GenerateEmbeddings(Manager& manager, const std::string& alias) {
    std::cout << "\n=== Example 6: Embeddings ===\n";

    auto& catalog = manager.GetCatalog();

    auto* model = catalog.GetModel(alias);
    if (!model) {
        std::cerr << "Model '" << alias << "' not found in catalog.\n";
        return;
    }

    model->Download([](float pct) { std::cout << "\rDownloading: " << pct << "%   " << std::flush; });
    std::cout << "\n";

    model->Load();

    OpenAIEmbeddingClient embeddings(*model);

    // Single input
    auto single = embeddings.GenerateEmbedding("The quick brown fox jumps over the lazy dog");
    if (!single.data.empty()) {
        std::cout << "Single embedding: dim=" << single.data[0].embedding.size() << "\n";
    }

    // Batch input
    std::vector<std::string> inputs = {"The capital of France is Paris", "Machine learning is a subset of AI"};
    auto batch = embeddings.GenerateEmbeddings(inputs);
    std::cout << "Batch embeddings: count=" << batch.data.size();
    if (!batch.data.empty()) {
        std::cout << " dim=" << batch.data[0].embedding.size();
    }
    std::cout << "\n";

    model->Unload();
    std::cout << "Model unloaded.\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Optional command-line args: <chat-model-alias> <audio-model-alias> <audio-file-path>
    const std::string chatAlias = (argc > 1) ? argv[1] : "phi-3.5-mini";
    const std::string audioAlias = (argc > 2) ? argv[2] : "whisper-large-v3-turbo";
    const std::string audioPath = (argc > 3) ? argv[3] : "";

    try {
        StdLogger logger;
        Manager::Create({"SampleApp"}, &logger);
        auto& manager = Manager::Instance();

        // Discover and download execution providers (optional — may not be
        // supported by older Core DLLs or offline environments)
        try {
            auto eps = manager.DiscoverEps();
            std::cout << "\nAvailable execution providers:\n";
            for (const auto& ep : eps) {
                std::cout << "  " << ep.name << "\n";
            }

            if (!eps.empty()) {
                std::cout << "\nDownloading execution providers:\n";
                std::string currentEp;
                manager.DownloadAndRegisterEps([&](const std::string& epName, double percent) {
                    if (epName != currentEp) {
                        if (!currentEp.empty()) std::cout << "\n";
                        currentEp = epName;
                    }
                    printf("\r  %-30s  %5.1f%%", epName.c_str(), percent);
                    fflush(stdout);
                });
                if (!currentEp.empty()) std::cout << "\n";
            } else {
                std::cout << "\nNo execution providers to download.\n";
            }
        } catch (const std::exception& ex) {
            std::cerr << "EP discovery/download skipped: " << ex.what() << "\n";
        }

        // 1. Browse the full catalog
        try {
             BrowseCatalog(manager);
         }
        catch (const std::exception& ex) {
             std::cerr << "Example 1 failed: " << ex.what() << "\n";
         }

        // 2. Non-streaming chat
        try {
            ChatNonStreaming(manager, chatAlias);
        }
        catch (const std::exception& ex) {
            std::cerr << "Example 2 failed: " << ex.what() << "\n";
        }

        // 3. Streaming chat
        try {
            ChatStreaming(manager, chatAlias);
        }
        catch (const std::exception& ex) {
            std::cerr << "Example 3 failed: " << ex.what() << "\n";
        }

        // 4. Audio transcription (requires an audio file path)
        if (!audioPath.empty()) {
            try {
                TranscribeAudio(manager, audioAlias, audioPath);
            }
            catch (const std::exception& ex) {
                std::cerr << "Example 4 failed: " << ex.what() << "\n";
            }
        }

        // 5. Tool calling
        try {
            ChatWithToolCalling(manager, chatAlias);
        }
        catch (const std::exception& ex) {
            std::cerr << "Example 5 failed: " << ex.what() << "\n";
        }

        // 6. Embeddings — generate single and batch embeddings
        GenerateEmbeddings(manager, "qwen3-embedding-0.6b");

        Manager::Destroy();
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << std::endl;
        Manager::Destroy();
        return 1;
    }
}
