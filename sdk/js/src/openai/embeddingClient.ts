import { CoreInterop } from '../detail/coreInterop.js';

export class EmbeddingClientSettings {
    dimensions?: number;
    encodingFormat?: string;

    /**
     * Serializes the settings into an OpenAI-compatible request object.
     * @internal
     */
    _serialize() {
        this.validateEncodingFormat(this.encodingFormat);

        const result: any = {
            dimensions: this.dimensions,
            encoding_format: this.encodingFormat,
        };

        // Filter out undefined properties
        return Object.fromEntries(Object.entries(result).filter(([_, v]) => v !== undefined));
    }

    /**
     * Validates that the encoding format is a supported value.
     * @internal
     */
    private validateEncodingFormat(format?: string): void {
        if (!format) return;
        const validFormats = ['float', 'base64'];
        if (!validFormats.includes(format)) {
            throw new Error(`encodingFormat must be one of: ${validFormats.join(', ')}`);
        }
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
     * Validates that the inputs array is non-empty and all elements are non-empty strings.
     * @internal
     */
    private validateInputs(inputs: string[]): void {
        if (!inputs || !Array.isArray(inputs) || inputs.length === 0) {
            throw new Error('Inputs must be a non-empty array of strings.');
        }
        for (const input of inputs) {
            this.validateInput(input);
        }
    }

    /**
     * Sends an embedding request and parses the response.
     * @internal
     */
    private executeRequest(input: string | string[]): any {
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
            throw new Error(
                `Embedding generation failed for model '${this.modelId}': ${error instanceof Error ? error.message : String(error)}`,
                { cause: error }
            );
        }
    }

    /**
     * Generates embeddings for the given input text.
     * @param input - The text to generate embeddings for.
     * @returns The embedding response containing the embedding vector.
     */
    public async generateEmbedding(input: string): Promise<any> {
        this.validateInput(input);
        return this.executeRequest(input);
    }

    /**
     * Generates embeddings for multiple input texts in a single request.
     * @param inputs - The texts to generate embeddings for.
     * @returns The embedding response containing one embedding vector per input.
     */
    public async generateEmbeddings(inputs: string[]): Promise<any> {
        this.validateInputs(inputs);
        return this.executeRequest(inputs);
    }
}
