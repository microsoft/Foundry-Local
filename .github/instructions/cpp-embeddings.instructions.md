---
description: "Use when working on embeddings inference, EmbeddingsSession, EmbeddingsHandler, embedding models, or debugging embedding test failures."
applyTo: "sdk_v2/cpp/src/inferencing/generative/embeddings/**"
---
# Embeddings Architecture

## Bidirectional Attention — No Batching with Padding

Embedding models like Qwen3-Embedding use **bidirectional attention**. Unlike causal (autoregressive) models where padding only affects padded positions, bidirectional attention means padding tokens corrupt *all* positions in the sequence — including real tokens.

**Constraint:** `EmbeddingsSession` must process each input independently with `batch_size=1`. Do not re-introduce batched inference with right-padding unless the padding/attention-mask problem is solved at the ORT GenAI level.

The implementation uses `GenerateSingleEmbedding()` per input:
1. Tokenize input
2. Append EOS token
3. Create generator with `batch_size=1`
4. Single forward pass (no autoregressive loop)
5. Extract last-token hidden state
6. L2-normalize the embedding vector

## Key Files

- `embeddings_session.cc` / `.h` — Core inference logic
- `embeddings_handler.cc` — HTTP handler for `/v1/embeddings` endpoint
- `embeddings_test.cc` — Integration tests (7 tests across 2 fixtures)
