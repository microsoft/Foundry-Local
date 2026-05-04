// Embeddings — Foundry Local C++ SDK Example
//
// Demonstrates single-input and batch embedding generation using the
// OpenAI-compatible `OpenAIEmbeddingClient` against a locally loaded
// embedding model.
//
// Requires: Foundry Local C++ SDK
//
// Usage: ./embeddings-example

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "foundry_local.h"

int main() {
    try {
        std::cout << "===========================================================" << std::endl;
        std::cout << "   Foundry Local -- Embeddings Demo (C++)" << std::endl;
        std::cout << "===========================================================" << std::endl;
        std::cout << std::endl;

        foundry_local::Configuration config("foundry_local_samples");

        foundry_local::Manager::Create(config);
        auto& manager = foundry_local::Manager::Instance();
        manager.EnsureEpsDownloaded();

        auto& catalog = manager.GetCatalog();
        auto* model = catalog.GetModel("qwen3-embedding-0.6b");
        if (!model) {
            throw std::runtime_error("Model \"qwen3-embedding-0.6b\" not found in catalog");
        }

        std::cout << "Downloading model (if needed)..." << std::endl;
        model->Download([](float pct) {
            std::cout << "\rDownloading: " << pct << "%   " << std::flush;
        });
        std::cout << std::endl;
        std::cout << "Loading model..." << std::endl;
        model->Load();
        std::cout << "Model loaded" << std::endl;

        foundry_local::OpenAIEmbeddingClient embeddings(*model);

        // Single input
        std::cout << std::endl << "--- Single Embedding ---" << std::endl;
        auto single = embeddings.GenerateEmbedding("The quick brown fox jumps over the lazy dog");
        if (!single.data.empty()) {
            std::cout << "Dimensions: " << single.data[0].embedding.size() << std::endl;
        }

        // Batch input
        std::cout << std::endl << "--- Batch Embeddings ---" << std::endl;
        std::vector<std::string> inputs = {
            "Machine learning is a subset of artificial intelligence",
            "The capital of France is Paris",
            "Rust is a systems programming language",
        };
        auto batch = embeddings.GenerateEmbeddings(inputs);
        std::cout << "Number of embeddings: " << batch.data.size() << std::endl;
        for (std::size_t i = 0; i < batch.data.size(); ++i) {
            std::cout << "  [" << i << "] Dimensions: " << batch.data[i].embedding.size() << std::endl;
        }

        model->Unload();
        std::cout << std::endl << "Model unloaded" << std::endl;

        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << std::endl;
        return 1;
    }
}
