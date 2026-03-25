import { CoreInterop } from './detail/coreInterop.js';
import { ModelLoadManager } from './detail/modelLoadManager.js';
import { ModelInfo } from './types.js';
import { ChatClient } from './openai/chatClient.js';
import { AudioClient } from './openai/audioClient.js';
import { ResponsesClient } from './openai/responsesClient.js';
import { IModel } from './imodel.js';

/**
 * Represents a specific variant of a model (e.g., a specific quantization or format).
 * Contains the low-level implementation for interacting with the model.
 */
export class ModelVariant implements IModel {
    private _modelInfo: ModelInfo;
    private coreInterop: CoreInterop;
    private modelLoadManager: ModelLoadManager;

    constructor(modelInfo: ModelInfo, coreInterop: CoreInterop, modelLoadManager: ModelLoadManager) {
        this._modelInfo = modelInfo;
        this.coreInterop = coreInterop;
        this.modelLoadManager = modelLoadManager;
    }

    /**
     * Gets the unique identifier of the model variant.
     * @returns The model ID.
     */
    public get id(): string {
        return this._modelInfo.id;
    }

    /**
     * Gets the alias of the model.
     * @returns The model alias.
     */
    public get alias(): string {
        return this._modelInfo.alias;
    }

    /**
     * Gets the detailed information about the model variant.
     * @returns The ModelInfo object.
     */
    public get modelInfo(): ModelInfo {
        return this._modelInfo;
    }

    /**
     * Checks if the model variant is cached locally.
     * @returns True if cached, false otherwise.
     */
    public get isCached(): boolean {
        const cachedModels: string[] = JSON.parse(this.coreInterop.executeCommand("get_cached_models"));
        return cachedModels.includes(this._modelInfo.id);
    }

    /**
     * Checks if the model variant is loaded in memory.
     * @returns True if loaded, false otherwise.
     */
    public async isLoaded(): Promise<boolean> {
        const loadedModels = await this.modelLoadManager.listLoaded();
        return loadedModels.includes(this._modelInfo.id);
    }

    /**
     * Downloads the model variant.
     * @param progressCallback - Optional callback to report download progress.
     *   Receives the status name (e.g. "Fetching model", "Downloading model") and
     *   a progress value. During the "Fetching model" phase the value is a heartbeat
     *   tick; during "Downloading model" it is the percentage (0-100).
     */
    public async download(progressCallback?: (name: string | null, progress: number) => void): Promise<void> {
        const request = { Params: { Model: this._modelInfo.id } };
        if (!progressCallback) {
            this.coreInterop.executeCommand("download_model", request);
        } else {
            await this.coreInterop.executeCommandStreaming("download_model", request, (chunk: string) => {
                const sepIndex = chunk.indexOf('|');
                if (sepIndex >= 0) {
                    const name = chunk.substring(0, sepIndex) || null;
                    const percent = parseFloat(chunk.substring(sepIndex + 1)) || 0;
                    progressCallback(name, percent);
                } else {
                    // Backwards compatibility: plain numeric progress
                    const progress = parseFloat(chunk);
                    if (!isNaN(progress)) {
                        progressCallback(null, progress);
                    }
                }
            });
        }
    }

    /**
     * Gets the local file path of the model variant.
     * @returns The local file path.
     */
    public get path(): string {
        const request = { Params: { Model: this._modelInfo.id } };
        return this.coreInterop.executeCommand("get_model_path", request);
    }

    /**
     * Loads the model variant into memory.
     * @returns A promise that resolves when the model is loaded.
     */
    public async load(): Promise<void> {
        await this.modelLoadManager.load(this._modelInfo.id);
    }

    /**
     * Removes the model variant from the local cache.
     */
    public removeFromCache(): void {
        this.coreInterop.executeCommand("remove_cached_model", { Params: { Model: this._modelInfo.id } });
    }

    /**
     * Unloads the model variant from memory.
     * @returns A promise that resolves when the model is unloaded.
     */
    public async unload(): Promise<void> {
        await this.modelLoadManager.unload(this._modelInfo.id);
    }

    /**
     * Creates a ChatClient for interacting with the model via chat completions.
     * @returns A ChatClient instance.
     */
    public createChatClient(): ChatClient {
        return new ChatClient(this._modelInfo.id, this.coreInterop);
    }

    /**
     * Creates an AudioClient for interacting with the model via audio operations.
     * @returns An AudioClient instance.
     */
    public createAudioClient(): AudioClient {
        return new AudioClient(this._modelInfo.id, this.coreInterop);
    }

    /**
     * Creates a ResponsesClient for interacting with the model via the Responses API.
     * @param baseUrl - The base URL of the Foundry Local web service.
     * @returns A ResponsesClient instance.
     */
    public createResponsesClient(baseUrl: string): ResponsesClient {
        return new ResponsesClient(baseUrl, this._modelInfo.id);
    }
}
