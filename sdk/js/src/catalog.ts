import { CoreInterop } from './detail/coreInterop.js';
import { ModelLoadManager } from './detail/modelLoadManager.js';
import { Model } from './model.js';
import { ModelVariant } from './modelVariant.js';
import { ModelInfo } from './types.js';

/**
 * Represents a catalog of AI models available in the system.
 * Provides methods to discover, list, and retrieve models and their variants.
 */
export class Catalog {
    private _name: string;
    private coreInterop: CoreInterop;
    private modelLoadManager: ModelLoadManager;
    private _models: Model[] = [];
    private modelAliasToModel: Map<string, Model> = new Map();
    private modelIdToModelVariant: Map<string, ModelVariant> = new Map();
    private lastFetch: number = 0;

    constructor(coreInterop: CoreInterop, modelLoadManager: ModelLoadManager) {
        this.coreInterop = coreInterop;
        this.modelLoadManager = modelLoadManager;
        this._name = this.coreInterop.executeCommand("get_catalog_name");
    }

    /**
     * Gets the name of the catalog.
     * @returns The name of the catalog.
     */
    public get name(): string {
        return this._name;
    }

    private async updateModels(): Promise<void> {
        // TODO: make this configurable
        if ((Date.now() - this.lastFetch) < 6 * 60 * 60 * 1000) { // 6 hours
            return;
        }

        // Potential network call to fetch model list
        const modelListJson = this.coreInterop.executeCommand("get_model_list");
        let modelsInfo: ModelInfo[] = [];
        try {
            modelsInfo = JSON.parse(modelListJson);
        } catch (error) {
            throw new Error(`Failed to parse model list JSON: ${error}`);
        }

        this.modelAliasToModel.clear();
        this.modelIdToModelVariant.clear();
        this._models = [];

        for (const info of modelsInfo) {
            const variant = new ModelVariant(info, this.coreInterop, this.modelLoadManager);
            let model = this.modelAliasToModel.get(info.alias);
            
            if (!model) {
                model = new Model(variant);
                this.modelAliasToModel.set(info.alias, model);
                this._models.push(model);
            } else {
                model.addVariant(variant);
            }

            this.modelIdToModelVariant.set(variant.id, variant);
        }

        this.lastFetch = Date.now();
    }

    /**
     * Lists all available models in the catalog.
     * This method is asynchronous as it may fetch the model list from a remote service or perform file I/O.
     * @returns A Promise that resolves to an array of Model objects.
     */
    public async getModels(): Promise<Model[]> {
        await this.updateModels();
        return this._models;
    }

    /**
     * Retrieves a model by its alias, HuggingFace URL, or org/repo identifier.
     * For plain aliases, throws if the model is not found.
     * For HuggingFace URLs or org/repo identifiers, returns undefined if not found.
     * @param alias - The alias of the model, a HuggingFace URL, or an org/repo identifier.
     * @returns A Promise that resolves to the Model object if found.
     * @throws Error - If alias is null, undefined, or empty.
     * @throws Error - If a plain alias is not found in the catalog.
     */
    public async getModel(alias: string): Promise<Model | undefined> {
        if (typeof alias !== 'string' || alias.trim() === '') {
            throw new Error('Model alias must be a non-empty string.');
        }

        const hfUrl = Catalog.normalizeToHuggingFaceUrl(alias);
        if (hfUrl) {
            // Force a fresh catalog refresh for HuggingFace lookups
            this.lastFetch = 0;
            await this.updateModels();

            for (const [, variant] of this.modelIdToModelVariant) {
                if (variant.modelInfo.uri.toLowerCase() === hfUrl.toLowerCase()) {
                    return this.modelAliasToModel.get(variant.alias);
                }
            }

            return undefined;
        }

        await this.updateModels();
        const model = this.modelAliasToModel.get(alias);
        if (!model) {
            const availableAliases = Array.from(this.modelAliasToModel.keys()).join(', ');
            throw new Error(`Model with alias '${alias}' not found. Available models: ${availableAliases || '(none)'}`);
        }
        return model;
    }

    /**
     * Downloads a model by its HuggingFace URL or org/repo identifier and adds it to the catalog.
     * If the model is already cached, this is a no-op and returns the existing model.
     * @param modelUri - A HuggingFace URL (https://huggingface.co/org/repo) or org/repo identifier.
     * @param progressCallback - Optional callback invoked with download progress percentage (0-100).
     * @returns A Promise that resolves to the downloaded Model.
     * @throws Error if the URI is not a valid HuggingFace identifier or if the download fails.
     */
    public async downloadModel(modelUri: string, progressCallback?: (progress: number) => void): Promise<Model> {
        // Validate that this is a HuggingFace identifier
        if (!Catalog.normalizeToHuggingFaceUrl(modelUri)) {
            throw new Error(`'${modelUri}' is not a valid HuggingFace URL or org/repo identifier.`);
        }

        // Send the original URI to Core — it handles full URLs with /tree/revision/
        // and raw org/repo/subdir strings. Do NOT send the normalized form, as Core's
        // URL parser expects /tree/revision/ when the https:// prefix is present.
        const request = { Params: { Model: modelUri } };
        let resultData: string;

        if (progressCallback) {
            resultData = await this.coreInterop.executeCommandWithCallback(
                "download_model",
                request,
                (progressString: string) => {
                    try {
                        const progress = JSON.parse(progressString);
                        if (progress.percent != null) {
                            progressCallback(progress.percent);
                        }
                    } catch { /* ignore malformed progress */ }
                }
            );
        } else {
            resultData = this.coreInterop.executeCommand("download_model", request);
        }

        // Force a catalog refresh to pick up the newly downloaded model
        this.lastFetch = 0;
        await this.updateModels();

        // The backend returns the org/model[/subpath] identifier as resultData
        const expectedUri = `https://huggingface.co/${resultData}`;
        for (const [, variant] of this.modelIdToModelVariant) {
            if (variant.modelInfo.uri.toLowerCase() === expectedUri.toLowerCase()) {
                const model = this.modelAliasToModel.get(variant.alias);
                if (model) {
                    return model;
                }
            }
        }

        throw new Error(`Model '${modelUri}' was downloaded but could not be found in the catalog.`);
    }

    /**
     * Normalizes a model identifier to a canonical HuggingFace URL, or returns null if it's a plain alias.
     * Strips /tree/{revision}/ from full browser URLs so the result matches the stored Info.Uri format.
     * Handles:
     *   - "https://huggingface.co/org/repo/tree/main/sub" -> "https://huggingface.co/org/repo/sub"
     *   - "https://huggingface.co/org/repo" -> returned as-is
     *   - "org/repo[/sub]" -> "https://huggingface.co/org/repo[/sub]"
     *   - "phi-3-mini" (plain alias) -> null
     */
    private static normalizeToHuggingFaceUrl(input: string): string | null {
        const hfPrefix = "https://huggingface.co/";

        if (input.toLowerCase().startsWith(hfPrefix)) {
            // Strip /tree/{revision}/ to match the canonical form stored by Core
            const path = input.substring(hfPrefix.length);
            const parts = path.split('/');
            if (parts.length >= 4 && parts[2].toLowerCase() === 'tree') {
                // parts[0]=org, parts[1]=repo, parts[2]="tree", parts[3]=revision, parts[4..]=subpath
                const org = parts[0];
                const repo = parts[1];
                const subPath = parts.length > 4 ? parts.slice(4).join('/') : null;
                return subPath
                    ? `${hfPrefix}${org}/${repo}/${subPath}`
                    : `${hfPrefix}${org}/${repo}`;
            }

            return input;
        }

        if (input.includes('/') && !input.toLowerCase().startsWith("azureml://")) {
            return hfPrefix + input;
        }

        return null;
    }

    /**
     * Retrieves a specific model variant by its ID.
     * This method is asynchronous as it may ensure the catalog is up-to-date by fetching from a remote service.
     * @param modelId - The unique identifier of the model variant.
     * @returns A Promise that resolves to the ModelVariant object if found, otherwise throws an error.
     * @throws Error - If modelId is null, undefined, or empty.
     */
    public async getModelVariant(modelId: string): Promise<ModelVariant> {
        if (typeof modelId !== 'string' || modelId.trim() === '') {
            throw new Error('Model ID must be a non-empty string.');
        }
        await this.updateModels();
        const variant = this.modelIdToModelVariant.get(modelId);
        if (!variant) {
            const availableIds = Array.from(this.modelIdToModelVariant.keys()).join(', ');
            throw new Error(`Model variant with ID '${modelId}' not found. Available variants: ${availableIds || '(none)'}`);
        }
        return variant;
    }

    /**
     * Retrieves a list of all locally cached model variants.
     * This method is asynchronous as it may involve file I/O or querying the underlying core.
     * @returns A Promise that resolves to an array of cached ModelVariant objects.
     */
    public async getCachedModels(): Promise<ModelVariant[]> {
        await this.updateModels();
        const cachedModelListJson = this.coreInterop.executeCommand("get_cached_models");
        let cachedModelIds: string[] = [];
        try {
            cachedModelIds = JSON.parse(cachedModelListJson);
        } catch (error) {
            throw new Error(`Failed to parse cached model list JSON: ${error}`);
        }
        const cachedModels: Set<ModelVariant> = new Set();
        
        for (const modelId of cachedModelIds) {
            const variant = this.modelIdToModelVariant.get(modelId);
            if (variant) {
                cachedModels.add(variant);
            }
        }
        return Array.from(cachedModels);
    }

    /**
     * Retrieves a list of all currently loaded model variants.
     * This operation is asynchronous because checking the loaded status may involve querying
     * the underlying core or an external service, which can be an I/O bound operation.
     * @returns A Promise that resolves to an array of loaded ModelVariant objects.
     */
    public async getLoadedModels(): Promise<ModelVariant[]> {
        await this.updateModels();
        let loadedModelIds: string[] = [];
        try {
            loadedModelIds = await this.modelLoadManager.listLoaded();
        } catch (error) {
            throw new Error(`Failed to list loaded models: ${error}`);
        }
        const loadedModels: ModelVariant[] = [];
        
        for (const modelId of loadedModelIds) {
            const variant = this.modelIdToModelVariant.get(modelId);
            if (variant) {
                loadedModels.push(variant);
            }
        }
        return loadedModels;
    }
}