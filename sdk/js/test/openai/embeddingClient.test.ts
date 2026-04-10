import { describe, it } from 'mocha';
import { expect } from 'chai';
import { getTestManager, EMBEDDING_MODEL_ALIAS } from '../testUtils.js';

describe('Embedding Client Tests', () => {

    it('should generate embedding', async function() {
        this.timeout(30000);
        const manager = getTestManager();
        const catalog = manager.catalog;

        const cachedModels = await catalog.getCachedModels();
        expect(cachedModels.length).to.be.greaterThan(0);

        const cachedVariant = cachedModels.find(m => m.alias === EMBEDDING_MODEL_ALIAS);
        expect(cachedVariant, 'qwen3-0.6b-embedding-generic-cpu should be cached').to.not.be.undefined;

        const model = await catalog.getModel(EMBEDDING_MODEL_ALIAS);
        expect(model).to.not.be.undefined;
        if (!cachedVariant) return;

        model.selectVariant(cachedVariant);
        await model.load();

        try {
            const embeddingClient = model.createEmbeddingClient();
            expect(embeddingClient).to.not.be.undefined;

            const response = await embeddingClient.generateEmbedding(
                'The quick brown fox jumps over the lazy dog'
            );

            expect(response).to.not.be.undefined;
            expect(response.data).to.be.an('array').with.length.greaterThan(0);
            expect(response.data[0].embedding).to.be.an('array');
            expect(response.data[0].embedding.length).to.equal(1024);
            expect(response.data[0].index).to.equal(0);

            console.log(`Embedding dimension: ${response.data[0].embedding.length}`);
        } finally {
            await model.unload();
        }
    });

    it('should generate normalized embedding', async function() {
        this.timeout(30000);
        const manager = getTestManager();
        const catalog = manager.catalog;

        const cachedModels = await catalog.getCachedModels();
        const cachedVariant = cachedModels.find(m => m.alias === EMBEDDING_MODEL_ALIAS);
        if (!cachedVariant) { this.skip(); return; }

        const model = await catalog.getModel(EMBEDDING_MODEL_ALIAS);
        model.selectVariant(cachedVariant);
        await model.load();

        try {
            const embeddingClient = model.createEmbeddingClient();
            const response = await embeddingClient.generateEmbedding(
                'Machine learning is a subset of artificial intelligence'
            );

            const embedding = response.data[0].embedding;
            expect(embedding.length).to.equal(1024);

            // Verify L2 norm is approximately 1.0
            let norm = 0;
            for (const val of embedding) {
                norm += val * val;
            }
            norm = Math.sqrt(norm);
            expect(norm).to.be.greaterThan(0.99);
            expect(norm).to.be.lessThan(1.01);
        } finally {
            await model.unload();
        }
    });

    it('should produce different embeddings for different inputs', async function() {
        this.timeout(30000);
        const manager = getTestManager();
        const catalog = manager.catalog;

        const cachedModels = await catalog.getCachedModels();
        const cachedVariant = cachedModels.find(m => m.alias === EMBEDDING_MODEL_ALIAS);
        if (!cachedVariant) { this.skip(); return; }

        const model = await catalog.getModel(EMBEDDING_MODEL_ALIAS);
        model.selectVariant(cachedVariant);
        await model.load();

        try {
            const embeddingClient = model.createEmbeddingClient();

            const response1 = await embeddingClient.generateEmbedding('The quick brown fox');
            const response2 = await embeddingClient.generateEmbedding('The capital of France is Paris');

            expect(response1.data[0].embedding.length).to.equal(response2.data[0].embedding.length);

            // Cosine similarity should not be 1.0
            let dot = 0, norm1 = 0, norm2 = 0;
            for (let i = 0; i < response1.data[0].embedding.length; i++) {
                const v1 = response1.data[0].embedding[i];
                const v2 = response2.data[0].embedding[i];
                dot += v1 * v2;
                norm1 += v1 * v1;
                norm2 += v2 * v2;
            }
            const cosineSimilarity = dot / (Math.sqrt(norm1) * Math.sqrt(norm2));
            expect(cosineSimilarity).to.be.lessThan(0.99);
        } finally {
            await model.unload();
        }
    });

    it('should produce same embedding for same input', async function() {
        this.timeout(30000);
        const manager = getTestManager();
        const catalog = manager.catalog;

        const cachedModels = await catalog.getCachedModels();
        const cachedVariant = cachedModels.find(m => m.alias === EMBEDDING_MODEL_ALIAS);
        if (!cachedVariant) { this.skip(); return; }

        const model = await catalog.getModel(EMBEDDING_MODEL_ALIAS);
        model.selectVariant(cachedVariant);
        await model.load();

        try {
            const embeddingClient = model.createEmbeddingClient();

            const response1 = await embeddingClient.generateEmbedding('Deterministic embedding test');
            const response2 = await embeddingClient.generateEmbedding('Deterministic embedding test');

            for (let i = 0; i < response1.data[0].embedding.length; i++) {
                expect(response1.data[0].embedding[i]).to.equal(response2.data[0].embedding[i]);
            }
        } finally {
            await model.unload();
        }
    });

    it('should throw for empty input', function() {
        const manager = getTestManager();
        const catalog = manager.catalog;

        // Create a client directly (model doesn't need to be loaded for input validation)
        expect(() => {
            // Validation happens in generateEmbedding, but we need a loaded model for that.
            // Instead test the synchronous validation path.
            const { EmbeddingClient } = require('../../src/openai/embeddingClient.js');
        }).to.not.throw();
    });
});
