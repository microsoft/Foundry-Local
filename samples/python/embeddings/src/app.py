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

    # Discover available execution providers and their registration status.
    eps = manager.discover_eps()
    max_name_len = 30
    print("Available execution providers:")
    print(f"  {'Name':<{max_name_len}}  Registered")
    print(f"  {'─' * max_name_len}  ──────────")
    for ep in eps:
        print(f"  {ep.name:<{max_name_len}}  {ep.is_registered}")

    # Download and register all execution providers.
    print("\nDownloading execution providers:")
    current_ep = ""
    def ep_progress(ep_name: str, percent: float):
        nonlocal current_ep
        if ep_name != current_ep:
            if current_ep:
                print()
            current_ep = ep_name
        print(f"\r  {ep_name:<{max_name_len}}  {percent:5.1f}%", end="", flush=True)

    if eps:
        manager.download_and_register_eps(progress_callback=ep_progress)
        if current_ep:
            print()
    else:
        print("No execution providers to download.")

    # Select and load an embedding model from the catalog
    model = manager.catalog.get_model("qwen3-embedding-0.6b")
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
