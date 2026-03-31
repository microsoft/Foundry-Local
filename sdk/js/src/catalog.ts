import { CoreInterop } from './detail/coreInterop.js';
import { ModelLoadManager } from './detail/modelLoadManager.js';
import { Model } from './detail/model.js';
import { ModelVariant } from './detail/modelVariant.js';
import { ModelInfo } from './types.js';
import { IModel } from './imodel.js';

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
     * @returns A Promise that resolves to an array of IModel objects.
     */
    public async getModels(): Promise<IModel[]> {
        await this.updateModels();
        return this._models;
    }

    /**
     * Retrieves a model by its alias.
     * This method is asynchronous as it may ensure the catalog is up-to-date by fetching from a remote service.
     * @param alias - The alias of the model to retrieve.
     * @returns A Promise that resolves to the IModel object if found, otherwise throws an error.
     * @throws Error - If alias is null, undefined, or empty.
     */
    public async getModel(alias: string): Promise<IModel> {
        if (typeof alias !== 'string' || alias.trim() === '') {
            throw new Error('Model alias must be a non-empty string.');
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
     * Retrieves a specific model variant by its ID.
     * NOTE: This will return an IModel with a single variant. Use getModel to get an IModel with all available
     * variants.
     * This method is asynchronous as it may ensure the catalog is up-to-date by fetching from a remote service.
     * @param modelId - The unique identifier of the model variant.
     * @returns A Promise that resolves to the IModel object if found, otherwise throws an error.
     * @throws Error - If modelId is null, undefined, or empty.
     */
    public async getModelVariant(modelId: string): Promise<IModel> {
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
     * @returns A Promise that resolves to an array of cached IModel objects.
     */
    public async getCachedModels(): Promise<IModel[]> {
        await this.updateModels();
        const cachedModelListJson = this.coreInterop.executeCommand("get_cached_models");
        let cachedModelIds: string[] = [];
        try {
            cachedModelIds = JSON.parse(cachedModelListJson);
        } catch (error) {
            throw new Error(`Failed to parse cached model list JSON: ${error}`);
        }
        const cachedModels: Set<IModel> = new Set();
        
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
     * @returns A Promise that resolves to an array of loaded IModel objects.
     */
    public async getLoadedModels(): Promise<IModel[]> {
        await this.updateModels();
        let loadedModelIds: string[] = [];
        try {
            loadedModelIds = await this.modelLoadManager.listLoaded();
        } catch (error) {
            throw new Error(`Failed to list loaded models: ${error}`);
        }
        const loadedModels: IModel[] = [];
        
        for (const modelId of loadedModelIds) {
            const variant = this.modelIdToModelVariant.get(modelId);
            if (variant) {
                loadedModels.push(variant);
            }
        }
        return loadedModels;
    }

    /**
     * Get the latest version of a model.
     * This is used to check if a newer version of a model is available in the catalog for download.
     * @param modelOrModelVariant - The model to check for the latest version.
     * @returns The latest version of the model. Will match the input if it is the latest version.
     */
    public async getLatestVersion(modelOrModelVariant: IModel): Promise<IModel> {
        await this.updateModels();

        // Resolve to the parent Model by alias
        const model = this.modelAliasToModel.get(modelOrModelVariant.alias);
        if (!model) {
            throw new Error(`Model with alias '${modelOrModelVariant.alias}' not found in catalog.`);
        }

        // variants are sorted by version, so the first one matching the name is the latest version
        const latest = model.variants.find(v => v.info.name === modelOrModelVariant.info.name);
        if (!latest) {
            throw new Error(
                `Internal error. Mismatch between model (alias:${model.alias}) and ` +
                `model variant (alias:${modelOrModelVariant.alias}).`
            );
        }

        // if input was the latest return the input (could be model or model variant)
        // otherwise return the latest model variant
        return latest.id === modelOrModelVariant.id ? modelOrModelVariant : latest;
    }
}