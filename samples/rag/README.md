# Foundry Local RAG Implementation Guide

## Overview

This guide demonstrates how to build a complete offline RAG (Retrieval-Augmented Generation) solution using Foundry Local with the **Foundry Local C# SDK**, combining local embedding models with vector search capabilities for enhanced AI inference on edge devices. The SDK manages the full model lifecycle — cache checking, downloading, loading, and providing an OpenAI-compatible endpoint.

## Prerequisites

- **Qdrant**: Local vector database — `docker run -p 6333:6333 -p 6334:6334 qdrant/qdrant`
- **.NET 8+**: Runtime environment
- **.NET Interactive Notebook**: For development and testing
- **Foundry Local**: Latest — see [foundrylocal.ai](https://foundrylocal.ai)

### Hardware Considerations

- **CPU-only environments**: Use Qwen2.5-0.5b model for optimal performance
- **GPU environments**: Can leverage more powerful models through ONNX Runtime providers

## What is RAG?

RAG (Retrieval-Augmented Generation) combines information retrieval with text generation to provide contextually relevant responses. In this implementation, we create a fully offline RAG system that:

1. **Embeds documents** using local embedding models
2. **Stores vectors** in Qdrant for efficient similarity search
3. **Retrieves relevant context** based on user queries
4. **Generates responses** using Foundry Local's language models

## Local Embedding Model Setup

For a complete offline RAG solution, we use ONNX-based embedding models that run locally alongside Foundry Local. The recommended model is JinaAI's [jina-embeddings-v2-base-en](https://huggingface.co/jinaai/jina-embeddings-v2-base-en).

### Required Files

Download and place these files in a `./jina/` directory:

1. **ONNX Model**: [model.onnx](https://huggingface.co/jinaai/jina-embeddings-v2-base-en/resolve/main/model.onnx)
2. **Vocabulary**: [vocab.txt](https://huggingface.co/jinaai/jina-embeddings-v2-base-en/resolve/main/vocab.txt)

## Building RAG with Semantic Kernel

### 1. Core Dependencies

```csharp
#r "nuget: Microsoft.SemanticKernel, 1.60.0"
#r "nuget: Microsoft.SemanticKernel.Connectors.Onnx, 1.60.0-alpha"
#r "nuget: Microsoft.SemanticKernel.Connectors.Qdrant, 1.60.0-preview"
#r "nuget: Qdrant.Client, 1.14.1"
#r "nuget: Microsoft.AI.Foundry.Local"
```

### 2. SDK Initialization and Model Lifecycle

```csharp
using Microsoft.AI.Foundry.Local;
using Microsoft.Extensions.Logging.Abstractions;

// Initialize the SDK with web service support
await FoundryLocalManager.CreateAsync(
    new Configuration
    {
        AppName = "rag-notebook",
        Web = new Configuration.WebService { Urls = "http://127.0.0.1:0" }
    },
    NullLogger.Instance);

var manager = FoundryLocalManager.Instance;

// Look up model by alias — SDK auto-selects the best variant
var catalog = await manager.GetCatalogAsync();
var model = await catalog.GetModelAsync("qwen2.5-0.5b");

// Cache-aware download: only downloads on first run
if (!await model.IsCachedAsync())
    await model.DownloadAsync(progress => Console.Write($"\rDownload: {progress:F1}%"));

await model.LoadAsync();
await manager.StartWebServiceAsync();
var endpoint = manager.Urls![0];
```

### 3. Kernel Configuration

```csharp
var builder = Kernel.CreateBuilder();

// Local embedding model
builder.AddBertOnnxEmbeddingGenerator("./jina/model.onnx", "./jina/vocab.txt");

// Foundry Local chat completion — endpoint and variant from SDK
builder.AddOpenAIChatCompletion(
    model.SelectedVariant.Id,
    new Uri($"{endpoint}/v1"),
    apiKey: "",
    serviceId: "qwen2.5-0.5b");

var kernel = builder.Build();
```

### 3. Vector Store Service

The `VectorStoreService` class manages interactions with Qdrant:

```csharp
public class VectorStoreService
{
    private readonly QdrantClient _client;
    private readonly string _collectionName;

    public async Task InitializeAsync(int vectorSize = 768)
    {
        // Create collection if it doesn't exist
        await _client.CreateCollectionAsync(_collectionName, new VectorParams
        {
            Size = (ulong)vectorSize,
            Distance = Distance.Cosine
        });
    }

    public async Task UpsertAsync(string id, ReadOnlyMemory<float> embedding, 
        Dictionary<string, object> metadata)
    {
        // Store document chunks with embeddings
    }

    public async Task<List<ScoredPoint>> SearchAsync(ReadOnlyMemory<float> queryEmbedding, 
        int limit = 3)
    {
        // Perform similarity search
    }
}
```

### 4. Document Ingestion

The `DocumentIngestionService` processes documents into searchable chunks:

```csharp
public class DocumentIngestionService
{
    public async Task IngestDocumentAsync(string documentPath, string documentId)
    {
        var content = await File.ReadAllTextAsync(documentPath);
        var chunks = ChunkText(content, 300, 60); // 300 words, 60 word overlap

        foreach (var chunk in chunks)
        {
            var embedding = await _embeddingService.GenerateAsync(chunk);
            await _vectorStoreService.UpsertAsync(
                id: Guid.NewGuid().ToString(),
                embedding: embedding.Vector,
                metadata: new Dictionary<string, object>
                {
                    ["document_id"] = documentId,
                    ["text"] = chunk,
                    ["document_path"] = documentPath
                });
        }
    }
}
```

### 5. RAG Query Service

The `RagQueryService` combines retrieval and generation:

```csharp
public class RagQueryService
{
    public async Task<string> QueryAsync(string question)
    {
        // 1. Generate query embedding
        var queryEmbedding = await _embeddingService.GenerateAsync(question);
        
        // 2. Search for relevant chunks
        var searchResults = await _vectorStoreService.SearchAsync(
            queryEmbedding.Vector, limit: 5);
        
        // 3. Build context from retrieved chunks
        var context = string.Join("", searchResults
            .Select(r => r.Payload["text"].ToString()));
        
        // 4. Generate response using context
        var prompt = $"Question: {question}\nContext: {context}";
        var chatHistory = new ChatHistory();
        chatHistory.AddSystemMessage(
            "You are a helpful assistant that answers questions based on the provided context.");
        chatHistory.AddUserMessage(prompt);
        
        // 5. Stream response from Foundry Local
        var fullMessage = string.Empty;
        await foreach (var chatUpdate in _chatService.GetStreamingChatMessageContentsAsync(chatHistory))
        {
            if (chatUpdate.Content?.Length > 0)
                fullMessage += chatUpdate.Content;
        }
        
        return fullMessage ?? "I couldn't generate a response.";
    }
}
```

## Usage Example

```csharp
// Initialize services
var vectorStoreService = new VectorStoreService("http://localhost:6334", "", "demodocs");
await vectorStoreService.InitializeAsync();

var documentIngestionService = new DocumentIngestionService(embeddingService, vectorStoreService);
var ragQueryService = new RagQueryService(embeddingService, chatService, vectorStoreService);

// Ingest a document
await documentIngestionService.IngestDocumentAsync("./foundry-local-architecture.md", "doc1");

// Query the RAG system
var answer = await ragQueryService.QueryAsync("What's Foundry Local?");
Console.WriteLine(answer);
```

## Architecture Benefits

1. **Complete Offline Operation**: No external API dependencies
2. **Edge-Optimized**: Runs efficiently on local hardware
3. **Scalable Vector Search**: Qdrant provides high-performance similarity search
4. **Flexible Model Support**: ONNX Runtime supports multiple hardware providers
5. **Streaming Responses**: Real-time response generation

## Performance Considerations

- **Chunk Size**: 300 words with 60-word overlap balances context and performance
- **Vector Dimensions**: 768-dimensional embeddings from jina-embeddings-v2
- **Search Limit**: Retrieve top 5 most relevant chunks for context
- **Memory Management**: TTL-based model caching in Foundry Local

This implementation provides a robust foundation for building production-ready RAG applications that run entirely on local infrastructure while maintaining high performance and accuracy.

***Note***Go to [demo](./rag_foundrylocal_demo.ipynb)
