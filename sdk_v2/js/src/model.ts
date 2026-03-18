import { ModelVariant } from './modelVariant.js';
import { ChatClient } from './openai/chatClient.js';
import { AudioClient } from './openai/audioClient.js';
import { ResponsesClient } from './openai/responsesClient.js';
import { IModel } from './imodel.js';

/**
 * Represents a high-level AI model that may have multiple variants (e.g., quantized versions, different formats).
 * Manages the selection and interaction with a specific model variant.
 */
export class Model implements IModel {
    private _alias: string;

    private _variants: ModelVariant[];
    private selectedVariant: ModelVariant;

    constructor(variant: ModelVariant) {
        this._alias = variant.alias;
        this._variants = [variant];
        this.selectedVariant = variant;
    }

    /**
     * Adds a new variant to this model.
     * Automatically selects the new variant if it is cached and the current one is not.
     * @param variant - The model variant to add.
     * @throws Error - If the variant's alias does not match the model's alias.
     */
    public addVariant(variant: ModelVariant): void {
        if (variant.alias !== this._alias) {
            throw new Error("Variant alias does not match model alias.");
        }
        this._variants.push(variant);

        // prefer the highest priority locally cached variant
        if (variant.isCached && !this.selectedVariant.isCached) {
            this.selectedVariant = variant;
        }
    }

    /**
     * Selects a specific variant.
     * @param variant - The model variant to select.
     * @throws Error - If the variant does not belong to this model.
     */
    public selectVariant(variant: ModelVariant): void {
        const matchingVariant = this._variants.find(v => v.id === variant.id);
        if (!matchingVariant) {
            throw new Error(`Model variant with ID ${variant.id} does not belong to model "${this._alias}".`);
        }
        this.selectedVariant = matchingVariant;
    }

    /**
     * Gets the ID of the currently selected variant.
     * @returns The ID of the selected variant.
     */
    public get id(): string {
        return this.selectedVariant.id;
    }

    /**
     * Gets the alias of the model.
     * @returns The model alias.
     */
    public get alias(): string {
        return this._alias;
    }

    /**
     * Checks if the currently selected variant is cached locally.
     * @returns True if cached, false otherwise.
     */
    public get isCached(): boolean {
        return this.selectedVariant.isCached;
    }

    /**
     * Checks if the currently selected variant is loaded in memory.
     * @returns True if loaded, false otherwise.
     */
    public async isLoaded(): Promise<boolean> {
        return await this.selectedVariant.isLoaded();
    }

    /**
     * Gets all available variants for this model.
     * @returns An array of ModelVariant objects.
     */
    public get variants(): ModelVariant[] {
        return this._variants;
    }

    /**
     * Downloads the currently selected variant.
     * @param progressCallback - Optional callback to report download progress.
     */
    public download(progressCallback?: (progress: number) => void): Promise<void> {
        return this.selectedVariant.download(progressCallback);
    }

    /**
     * Gets the local file path of the currently selected variant.
     * @returns The local file path.
     */
    public get path(): string {
        return this.selectedVariant.path;
    }

    /**
     * Loads the currently selected variant into memory.
     * @returns A promise that resolves when the model is loaded.
     */
    public async load(): Promise<void> {
        await this.selectedVariant.load();
    }

    /**
     * Removes the currently selected variant from the local cache.
     */
    public removeFromCache(): void {
        this.selectedVariant.removeFromCache();
    }

    /**
     * Unloads the currently selected variant from memory.
     * @returns A promise that resolves when the model is unloaded.
     */
    public async unload(): Promise<void> {
        await this.selectedVariant.unload();
    }

    /**
     * Creates a ChatClient for interacting with the model via chat completions.
     * @returns A ChatClient instance.
     */
    public createChatClient(): ChatClient {
        return this.selectedVariant.createChatClient();
    }

    /**
     * Creates an AudioClient for interacting with the model via audio operations.
     * @returns An AudioClient instance.
     */
    public createAudioClient(): AudioClient {
        return this.selectedVariant.createAudioClient();
    }

    /**
     * Creates a ResponsesClient for interacting with the model via the Responses API.
     * @param baseUrl - The base URL of the Foundry Local web service.
     * @returns A ResponsesClient instance.
     */
    public createResponsesClient(baseUrl: string): ResponsesClient {
        return this.selectedVariant.createResponsesClient(baseUrl);
    }
}
