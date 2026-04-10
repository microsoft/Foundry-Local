//! Integration tests for EmbeddingClient.

use std::sync::Arc;

use foundry_local_sdk::openai::EmbeddingClient;
use foundry_local_sdk::Model;

use crate::common;

async fn setup_embedding_client() -> (EmbeddingClient, Arc<Model>) {
    let manager = common::get_test_manager();
    let catalog = manager.catalog();

    let model = catalog
        .get_model(common::EMBEDDING_MODEL_ALIAS)
        .await
        .expect("embedding model should exist in catalog");

    model.load().await.expect("model should load successfully");

    let client = model.create_embedding_client();
    (client, model)
}

#[tokio::test]
async fn should_generate_embedding() {
    let (client, model) = setup_embedding_client().await;

    let response = client
        .generate_embedding("The quick brown fox jumps over the lazy dog")
        .await
        .expect("embedding should succeed");

    assert_eq!(response.data.len(), 1);
    assert_eq!(response.data[0].index, 0);
    assert_eq!(response.data[0].embedding.len(), 1024);

    println!("Embedding dimension: {}", response.data[0].embedding.len());

    model.unload().await.expect("unload should succeed");
}

#[tokio::test]
async fn should_generate_normalized_embedding() {
    let (client, model) = setup_embedding_client().await;

    let inputs = [
        "The quick brown fox jumps over the lazy dog",
        "Machine learning is a subset of artificial intelligence",
        "The capital of France is Paris",
    ];

    for input in &inputs {
        let response = client
            .generate_embedding(input)
            .await
            .expect("embedding should succeed");

        let embedding = &response.data[0].embedding;
        assert_eq!(embedding.len(), 1024);

        // Verify L2 norm is approximately 1.0
        let norm: f64 = embedding.iter().map(|v| v * v).sum::<f64>().sqrt();
        assert!(
            (0.99..=1.01).contains(&norm),
            "L2 norm {norm} not approximately 1.0"
        );

        for val in embedding {
            assert!(
                (-1.0..=1.0).contains(val),
                "value {val} outside [-1, 1]"
            );
        }
    }

    model.unload().await.expect("unload should succeed");
}

#[tokio::test]
async fn should_produce_different_embeddings_for_different_inputs() {
    let (client, model) = setup_embedding_client().await;

    let response1 = client
        .generate_embedding("The quick brown fox")
        .await
        .expect("embedding should succeed");

    let response2 = client
        .generate_embedding("The capital of France is Paris")
        .await
        .expect("embedding should succeed");

    let emb1 = &response1.data[0].embedding;
    let emb2 = &response2.data[0].embedding;

    assert_eq!(emb1.len(), emb2.len());

    // Cosine similarity should not be 1.0
    let dot: f64 = emb1.iter().zip(emb2.iter()).map(|(a, b)| a * b).sum();
    let norm1: f64 = emb1.iter().map(|v| v * v).sum::<f64>().sqrt();
    let norm2: f64 = emb2.iter().map(|v| v * v).sum::<f64>().sqrt();
    let cosine_similarity = dot / (norm1 * norm2);
    assert!(
        cosine_similarity < 0.99,
        "cosine similarity {cosine_similarity} should be < 0.99"
    );

    model.unload().await.expect("unload should succeed");
}

#[tokio::test]
async fn should_produce_same_embedding_for_same_input() {
    let (client, model) = setup_embedding_client().await;

    let response1 = client
        .generate_embedding("Deterministic embedding test")
        .await
        .expect("embedding should succeed");

    let response2 = client
        .generate_embedding("Deterministic embedding test")
        .await
        .expect("embedding should succeed");

    let emb1 = &response1.data[0].embedding;
    let emb2 = &response2.data[0].embedding;

    for (i, (a, b)) in emb1.iter().zip(emb2.iter()).enumerate() {
        assert_eq!(a, b, "mismatch at index {i}");
    }

    model.unload().await.expect("unload should succeed");
}

#[tokio::test]
async fn should_throw_for_empty_input() {
    let (client, model) = setup_embedding_client().await;

    let result = client.generate_embedding("").await;
    assert!(result.is_err(), "empty input should return an error");

    model.unload().await.expect("unload should succeed");
}
