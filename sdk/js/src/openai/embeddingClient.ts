import { CoreInterop } from '../detail/coreInterop.js';

export class EmbeddingClientSettings {
    dimensions?: number;
    encodingFormat?: string;

    /**
     * Serializes the settings into an OpenAI-compatible request object.
     * @internal
     */
    _serialize() {
        const result: any = {
            dimensions: this.dimensions,
            encoding_format: this.encodingFormat,
        };

        // Filter out undefined properties
        return Object.fromEntries(Object.entries(result).filter(([_, v]) => v !== undefined));
    }
}

/**
 * Client for generating text embeddings with a loaded model.
 * Follows the OpenAI Embeddings API structure.
 */
export class EmbeddingClient {
    private modelId: string;
    private coreInterop: CoreInterop;

    /**
     * Configuration settings for embedding operations.
     */
    public settings = new EmbeddingClientSettings();

    /**
     * @internal
     * Restricted to internal use because CoreInterop is an internal implementation detail.
     * Users should create clients via the Model.createEmbeddingClient() factory method.
     */
    constructor(modelId: string, coreInterop: CoreInterop) {
        this.modelId = modelId;
        this.coreInterop = coreInterop;
    }

    /**
     * Validates that the input text is a non-empty string.
     * @internal
     */
    private validateInput(input: string): void {
        if (typeof input !== 'string' || input.trim() === '') {
            throw new Error('Input must be a non-empty string.');
        }
    }

    /**
     * Generates embeddings for the given input text.
     * @param input - The text to generate embeddings for.
     * @returns The embedding response containing the embedding vector.
     */
    public async generateEmbedding(input: string): Promise<any> {
        this.validateInput(input);

        const request = {
            model: this.modelId,
            input,
            ...this.settings._serialize()
        };

        try {
            const response = this.coreInterop.executeCommand('embeddings', {
                Params: { OpenAICreateRequest: JSON.stringify(request) }
            });
            return JSON.parse(response);
        } catch (error: any) {
            throw new Error(`Embedding generation failed: ${error.message}`, { cause: error });
        }
    }
}
