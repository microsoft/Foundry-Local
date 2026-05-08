# Embeddings Example (C++)

Demonstrates single-input and batch text embedding generation using the Foundry Local C++ SDK.

Loads the `qwen3-embedding-0.6b` embedding model, generates an embedding for a
single string and a batch of strings via `OpenAIEmbeddingClient`, and prints
the resulting vector dimensionality.


## Build

```bash
g++ -std=c++17 main.cpp -lfoundry_local -o embeddings-example
```

## Run

```bash
./embeddings-example
```
