// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Sample: Text embeddings with the Foundry Local C++ SDK (sdk_v2/cpp).
// Demonstrates native, in-process embedding generation for a single input and a
// batch, then computes cosine similarity between the batch vectors.

#include <foundry_local/foundry_local_cpp.h>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace foundry_local;

namespace {

/// Cosine similarity between two equal-length vectors. Returns 0 if either is a zero vector.
float CosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
  float dot = 0.0f;
  float norm_a = 0.0f;
  float norm_b = 0.0f;
  for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
    dot += a[i] * b[i];
    norm_a += a[i] * a[i];
    norm_b += b[i] * b[i];
  }

  if (norm_a == 0.0f || norm_b == 0.0f) {
    return 0.0f;
  }

  return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
}

/// Find the first embeddings model in the catalog and return its alias, or "" if none exist.
std::string FindEmbeddingsAlias(ICatalog& catalog) {
  ModelList all_models = catalog.GetModels();
  for (const auto& model : all_models.Models()) {
    if (model->GetInfo().Task() == "embeddings") {
      return std::string(model->GetInfo().Alias());
    }
  }

  return "";
}

}  // namespace

int main() {
  try {
    // 1. Create a configuration and manager (long-lived; keep it alive while using the SDK).
    Configuration config("foundry_local_samples");
    Manager manager(std::move(config));

    // 2. Locate an embeddings model in the catalog.
    auto& catalog = manager.GetCatalog();
    const std::string alias = FindEmbeddingsAlias(catalog);
    if (alias.empty()) {
      std::cerr << "No embeddings model found in the catalog.\n";
      return 1;
    }

    auto model = catalog.GetModel(alias);
    if (!model) {
      std::cerr << "Failed to retrieve embeddings model '" << alias << "'.\n";
      return 1;
    }

    ModelInfo info = model->GetInfo();
    std::cout << "Using model: " << info.Name() << " (alias: " << info.Alias() << ")\n";

    // 3. Download if not already cached.
    if (!model->IsCached()) {
      std::cout << "Downloading...\n";
      model->Download([](float progress) -> int {
        std::cout << "\r  " << static_cast<int>(progress) << "%" << std::flush;
        return 0;  // return non-zero to cancel
      });
      std::cout << "\n";
    }

    // 4. Load the model into memory.
    if (!model->IsLoaded()) {
      std::cout << "Loading model...\n";
      model->Load();
    }

    // 5. Create an embeddings session and generate vectors.
    {
      EmbeddingsSession session(*model);

      std::cout << "\n=== Single embedding ===\n";
      std::vector<float> embedding = session.Embed("The quick brown fox jumps over the lazy dog.");
      std::cout << "Dimensions: " << embedding.size() << "\n";
      std::cout << "First 5 values: [";
      for (size_t i = 0; i < 5 && i < embedding.size(); ++i) {
        std::cout << (i > 0 ? ", " : "") << embedding[i];
      }
      std::cout << "]\n";

      std::cout << "\n=== Batch embeddings + cosine similarity ===\n";
      const std::vector<std::string> sentences = {
          "The cat sat on the mat.",
          "A kitten rested on the rug.",
          "The stock market crashed yesterday.",
      };

      std::vector<std::vector<float>> embeddings = session.Embed(sentences);
      if (embeddings.empty()) {
        std::cerr << "No embeddings returned for the batch input.\n";
        return 1;
      }

      std::cout << "Generated " << embeddings.size() << " embeddings of dimension " << embeddings[0].size() << "\n\n";

      // Compare every pair: semantically similar sentences should score higher.
      for (size_t i = 0; i < sentences.size(); ++i) {
        for (size_t j = i + 1; j < sentences.size(); ++j) {
          const float similarity = CosineSimilarity(embeddings[i], embeddings[j]);
          std::cout << "  similarity(\"" << sentences[i] << "\",\n"
                    << "             \"" << sentences[j] << "\") = " << similarity << "\n\n";
        }
      }
    }  // session destroyed before unload

    // 6. Unload when done (the destructor would also handle this).
    model->Unload();
  } catch (const Error& ex) {
    std::cerr << "Foundry Local error [" << ex.Code() << "]: " << ex.what() << "\n";
    return 1;
  }

  return 0;
}
