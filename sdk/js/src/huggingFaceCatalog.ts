import * as fs from 'fs';
import * as path from 'path';

import { CoreInterop } from './detail/coreInterop.js';
import { ModelLoadManager } from './detail/modelLoadManager.js';
import { Model } from './model.js';
import { ModelVariant } from './modelVariant.js';
import { ModelInfo } from './types.js';

/** Filename for the HuggingFace registration persistence file. */
const REGISTRATIONS_FILENAME = 'huggingface.modelinfo.json';

/**
 * Normalizes a model identifier to a canonical HuggingFace URL, or returns null if it's a plain alias.
 * Strips /tree/{revision}/ from full browser URLs so the result matches the stored Info.Uri format.
 */
function normalizeToHuggingFaceUrl(input: string): string | null {
    const hfPrefix = "https://huggingface.co/";

    if (input.toLowerCase().startsWith(hfPrefix)) {
        const urlPath = input.substring(hfPrefix.length);
        const parts = urlPath.split('/');
        if (parts.length >= 4 && parts[2].toLowerCase() === 'tree') {
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
        const parts = input.split('/');
        if (parts.length >= 4 && parts[2].toLowerCase() === 'tree') {
            const org = parts[0];
            const repo = parts[1];
            const subPath = parts.length > 4 ? parts.slice(4).join('/') : null;
            return subPath
                ? `${hfPrefix}${org}/${repo}/${subPath}`
                : `${hfPrefix}${org}/${repo}`;
        }
        return hfPrefix + input;
    }

    return null;
}

/**
 * A catalog for HuggingFace models.
 *
 * Created via {@link FoundryLocalManager.addCatalog}. Each call creates a new
 * instance with registrations loaded from disk.
 *
 * Three-step flow:
 * ```typescript
 * const hf = await manager.addCatalog("https://huggingface.co");
 * const model = await hf.registerModel("org/repo");   // config files only
 * await model.download();                              // ONNX files
 * ```
 */
export class HuggingFaceCatalog {
    private coreInterop: CoreInterop;
    private modelLoadManager: ModelLoadManager;
    private token: string | undefined;
    private variantsById: Map<string, ModelVariant> = new Map();
    private modelsById: Map<string, Model> = new Map();

    private constructor(coreInterop: CoreInterop, modelLoadManager: ModelLoadManager, token?: string) {
        this.coreInterop = coreInterop;
        this.modelLoadManager = modelLoadManager;
        this.token = token;
    }

    /**
     * Creates a new HuggingFaceCatalog and loads persisted registrations.
     * @internal
     */
    static async create(coreInterop: CoreInterop, modelLoadManager: ModelLoadManager, token?: string): Promise<HuggingFaceCatalog> {
        const catalog = new HuggingFaceCatalog(coreInterop, modelLoadManager, token);
        catalog.loadRegistrations();
        return catalog;
    }

    /**
     * Gets the catalog name.
     */
    public get name(): string {
        return "HuggingFace";
    }

    /**
     * Register a HuggingFace model by downloading its config files only (~50KB).
     *
     * Sends the `register_model` FFI command to the native core, which downloads
     * config files (genai_config.json, config.json, tokenizer_config.json, etc.)
     * and generates metadata. Returns a Model with `cached: false`.
     *
     * After registration, call `model.download()` to download the ONNX files.
     *
     * @param modelIdentifier - A HuggingFace URL or org/repo identifier.
     * @returns The registered Model.
     */
    public async registerModel(modelIdentifier: string): Promise<Model> {
        if (!normalizeToHuggingFaceUrl(modelIdentifier)) {
            throw new Error(`'${modelIdentifier}' is not a valid HuggingFace URL or org/repo identifier.`);
        }

        const request = {
            Params: {
                Model: modelIdentifier,
                Token: this.token ?? ""
            }
        };

        const result = this.coreInterop.executeCommand("register_model", request);

        let modelInfo: ModelInfo;
        try {
            modelInfo = JSON.parse(result);
        } catch {
            throw new Error(`Failed to parse register_model response: ${result}`);
        }

        const variant = new ModelVariant(modelInfo, this.coreInterop, this.modelLoadManager, this.token);
        this.variantsById.set(modelInfo.id, variant);

        const model = new Model(variant);
        this.modelsById.set(modelInfo.id, model);

        this.saveRegistrations();

        return model;
    }

    /**
     * Look up a model by its ID, alias, or HuggingFace URL.
     *
     * Uses three-tier lookup:
     * 1. Direct ID match
     * 2. Alias match (case-insensitive)
     * 3. URI-based match (normalize to HuggingFace URL and compare)
     *
     * @param identifier - Model ID, alias, or HuggingFace URL/identifier.
     * @returns The Model if found, undefined otherwise.
     */
    public async getModel(identifier: string): Promise<Model | undefined> {
        if (typeof identifier !== 'string' || identifier.trim() === '') {
            throw new Error('Model identifier must be a non-empty string.');
        }

        // 1. Direct ID match
        const byId = this.modelsById.get(identifier);
        if (byId) return byId;

        // 2. Alias match (case-insensitive)
        for (const model of this.modelsById.values()) {
            if (model.alias.toLowerCase() === identifier.toLowerCase()) {
                return model;
            }
        }

        // 3. URI-based match
        const normalizedUrl = normalizeToHuggingFaceUrl(identifier);
        if (normalizedUrl) {
            const normalizedLower = normalizedUrl.toLowerCase();
            const normalizedWithSlash = normalizedLower.replace(/\/+$/, '') + '/';
            for (const variant of this.variantsById.values()) {
                const uriLower = variant.modelInfo.uri.toLowerCase();
                if (uriLower === normalizedLower || uriLower.startsWith(normalizedWithSlash)) {
                    return this.modelsById.get(variant.id);
                }
            }
        }

        return undefined;
    }

    /**
     * Downloads a HuggingFace model's ONNX files.
     *
     * The model should have been previously registered via {@link registerModel}.
     *
     * @param modelUri - A HuggingFace URL or org/repo identifier.
     * @param progressCallback - Optional callback invoked with download progress percentage (0-100).
     * @returns The downloaded Model.
     */
    public async downloadModel(modelUri: string, progressCallback?: (progress: number) => void): Promise<Model> {
        if (!normalizeToHuggingFaceUrl(modelUri)) {
            throw new Error(`'${modelUri}' is not a valid HuggingFace URL or org/repo identifier.`);
        }

        const request = {
            Params: {
                Model: modelUri,
                Token: this.token ?? ""
            }
        };

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

        // Match result against registered models by URI
        const expectedUri = `https://huggingface.co/${resultData}`;
        const expectedLower = expectedUri.toLowerCase();
        const expectedWithSlash = expectedLower.replace(/\/+$/, '') + '/';

        for (const variant of this.variantsById.values()) {
            const uriLower = variant.modelInfo.uri.toLowerCase();
            if (uriLower === expectedLower
                || uriLower.startsWith(expectedWithSlash)
                || expectedLower.startsWith(uriLower.replace(/\/+$/, '') + '/')) {
                const model = this.modelsById.get(variant.id);
                if (model) return model;
            }
        }

        throw new Error(`Model '${modelUri}' was downloaded but could not be found in the catalog.`);
    }

    /**
     * Returns all registered models.
     */
    public async getModels(): Promise<Model[]> {
        return Array.from(this.modelsById.values());
    }

    /**
     * Look up a specific model variant by its unique ID.
     */
    public async getModelVariant(modelId: string): Promise<ModelVariant | undefined> {
        return this.variantsById.get(modelId);
    }

    /**
     * Returns only the model variants that are currently cached on disk.
     */
    public async getCachedModels(): Promise<ModelVariant[]> {
        const cachedModelListJson = this.coreInterop.executeCommand("get_cached_models");
        let cachedModelIds: string[];
        try {
            cachedModelIds = JSON.parse(cachedModelListJson);
        } catch {
            return [];
        }
        return cachedModelIds
            .map(id => this.variantsById.get(id))
            .filter((v): v is ModelVariant => v !== undefined);
    }

    /**
     * Returns model variants that are currently loaded into memory.
     */
    public async getLoadedModels(): Promise<ModelVariant[]> {
        const loadedModelIds = await this.modelLoadManager.listLoaded();
        return loadedModelIds
            .map(id => this.variantsById.get(id))
            .filter((v): v is ModelVariant => v !== undefined);
    }

    // ── Persistence ──────────────────────────────────────────────────────

    private get registrationsPath(): string {
        const cacheDir = this.coreInterop.executeCommand("get_cache_directory").trim().replace(/^"|"$/g, '');
        return path.join(cacheDir, 'HuggingFace', REGISTRATIONS_FILENAME);
    }

    private loadRegistrations(): void {
        try {
            const filePath = this.registrationsPath;
            if (!fs.existsSync(filePath)) return;

            const json = fs.readFileSync(filePath, 'utf-8');
            if (!json.trim()) return;

            const infos: ModelInfo[] = JSON.parse(json);
            for (const info of infos) {
                const variant = new ModelVariant(info, this.coreInterop, this.modelLoadManager, this.token);
                this.variantsById.set(info.id, variant);
                this.modelsById.set(info.id, new Model(variant));
            }
        } catch {
            // Gracefully skip on any error — empty catalog is valid
        }
    }

    private saveRegistrations(): void {
        try {
            const filePath = this.registrationsPath;
            const dir = path.dirname(filePath);
            fs.mkdirSync(dir, { recursive: true });

            const infos = Array.from(this.variantsById.values()).map(v => v.modelInfo);
            fs.writeFileSync(filePath, JSON.stringify(infos, null, 2));
        } catch {
            // Non-critical — loss of persistence file is not fatal
        }
    }
}
