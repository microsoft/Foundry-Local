# Embeddings Example (C++)

Demonstrates single and batch text embedding generation using the Foundry Local C++ SDK.

Loads the `qwen3-embedding-0.6b` model and exercises
`OpenAIEmbeddingClient::GenerateEmbedding` (single input) and
`OpenAIEmbeddingClient::GenerateEmbeddings` (batch input), printing the returned
dimensionality for each.


## Build

```bash
g++ -std=c++17 main.cpp -lfoundry_local -o embeddings-example
```

## Run

```bash
./embeddings-example
```
