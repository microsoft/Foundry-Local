# <complete_code>
# <imports>
from foundry_local_sdk import Configuration, FoundryLocalManager
# </imports>


def main():
    # <init>
    # Initialize the Foundry Local SDK
    config = Configuration(app_name="foundry_local_samples")
    FoundryLocalManager.initialize(config)
    manager = FoundryLocalManager.instance

    # Select and load an embedding model from the catalog
    model = manager.catalog.get_model("qwen3-0.6b-embedding")
    model.download(
        lambda progress: print(
            f"\rDownloading model: {progress:.2f}%",
            end="",
            flush=True,
        )
    )
    print()
    model.load()
    print("Model loaded and ready.")

    # Get an embedding client
    client = model.get_embedding_client()
    # </init>

    # <single_embedding>
    # Generate a single embedding
    print("\n--- Single Embedding ---")
    response = client.generate_embedding("The quick brown fox jumps over the lazy dog")
    embedding = response.data[0].embedding
    print(f"Dimensions: {len(embedding)}")
    print(f"First 5 values: {embedding[:5]}")
    # </single_embedding>

    # <batch_embedding>
    # Generate embeddings for multiple inputs
    print("\n--- Batch Embeddings ---")
    batch_response = client.generate_embeddings([
        "Machine learning is a subset of artificial intelligence",
        "The capital of France is Paris",
        "Rust is a systems programming language",
    ])

    print(f"Number of embeddings: {len(batch_response.data)}")
    for i, data in enumerate(batch_response.data):
        print(f"  [{i}] Dimensions: {len(data.embedding)}")
    # </batch_embedding>

    # Clean up
    model.unload()
    print("\nModel unloaded.")


if __name__ == "__main__":
    main()
# </complete_code>
