import { Configuration, FoundryLocalConfig } from './configuration.js';
import { CoreInterop } from './detail/coreInterop.js';
import { ModelLoadManager } from './detail/modelLoadManager.js';
import { Catalog } from './catalog.js';
import { ResponsesClient } from './openai/responsesClient.js';

/**
 * The main entry point for the Foundry Local SDK.
 * Manages the initialization of the core system and provides access to the Catalog and ModelLoadManager.
 */
export class FoundryLocalManager {
    private static instance: FoundryLocalManager;
    private config: Configuration;
    private coreInterop: CoreInterop;
    private _modelLoadManager: ModelLoadManager;
    private _catalog: Catalog;
    private _urls: string[] = [];

    private constructor(config: Configuration) {
        this.config = config;
        this.coreInterop = new CoreInterop(this.config);
        this.coreInterop.executeCommand("initialize", { Params: this.config.params });
        this._modelLoadManager = new ModelLoadManager(this.coreInterop);
        this._catalog = new Catalog(this.coreInterop, this._modelLoadManager);
    }

    /**
     * Creates the FoundryLocalManager singleton with the provided configuration.
     * @param config - The configuration settings for the SDK (plain object).
     * @returns The initialized FoundryLocalManager instance.
     * @example
     * ```typescript
     * const manager = FoundryLocalManager.create({
     *   appName: 'MyApp',
     *   logLevel: 'info'
     * });
     * ```
     */
    public static create(config: FoundryLocalConfig): FoundryLocalManager {
        if (!FoundryLocalManager.instance) {
            const internalConfig = new Configuration(config);
            FoundryLocalManager.instance = new FoundryLocalManager(internalConfig);
        }
        return FoundryLocalManager.instance;
    }

    /**
     * Gets the Catalog instance for discovering and managing models.
     * @returns The Catalog instance.
     */
    public get catalog(): Catalog {
        return this._catalog;
    }

    /**
     * Gets the URLs where the web service is listening.
     * Returns an empty array if the web service is not running.
     * @returns An array of URLs.
     */
    public get urls(): string[] {
        return this._urls;
    }

    /**
     * Starts the local web service.
     * Use the `urls` property to retrieve the bound addresses after the service has started.
     * If no listener address is configured, the service defaults to `127.0.0.1:0` (binding to a random ephemeral port).
     * @throws Error - If starting the service fails.
     */
    public startWebService(): void {
        const response = this.coreInterop.executeCommand("start_service");
        try {
            this._urls = JSON.parse(response);
        } catch (error) {
            throw new Error(`Failed to decode JSON response from start_service: ${error}. Response was: ${response}`);
        }
    }

    /**
     * Stops the local web service.
     * @throws Error - If stopping the service fails.
     */
    public stopWebService(): void {
        if (this._urls.length > 0) {
            this.coreInterop.executeCommand("stop_service");
            this._urls = [];
        }
    }

    /**
     * Whether the web service is currently running.
     */
    public get isWebServiceRunning(): boolean {
        return this._urls.length > 0;
    }

    /**
     * Creates a ResponsesClient for interacting with the Responses API.
     * The web service must be started first via `startWebService()`.
     * @param modelId - Optional default model ID for requests.
     * @returns A ResponsesClient instance.
     * @throws Error - If the web service is not running.
     */
    public createResponsesClient(modelId?: string): ResponsesClient {
        if (this._urls.length === 0) {
            throw new Error(
                'Web service is not running. Call startWebService() before creating a ResponsesClient.'
            );
        }
        return new ResponsesClient(this._urls[0], modelId);
    }

    /**
     * Discovers the execution providers available for download and registration.
     * @returns An array of EP info objects with name and registration status.
     */
    public discoverEps(): { name: string; isRegistered: boolean }[] {
        const result = this.coreInterop.executeCommand("discover_eps");
        return result ? JSON.parse(result) : [];
    }

    /**
     * Ensures that the necessary execution providers (EPs) are downloaded and registered.
     * If EPs are already downloaded, this returns immediately. Otherwise it waits for
     * any in-progress or new downloads to complete.
     * @param names - Optional array of EP names to download. If omitted, all discoverable EPs are downloaded.
     * @param progressCallback - Optional callback receiving per-EP progress updates.
     *   Each update has `name` (EP name) and `percent` (0-100).
     * @returns A promise that resolves when all EPs are ready.
     */
    public async ensureEpsDownloaded(
        names?: string[],
        progressCallback?: (name: string | null, percent: number) => void
    ): Promise<void> {
        const params = names && names.length > 0 ? { Names: names.join(',') } : null;
        if (progressCallback) {
            await this.coreInterop.executeCommandStreaming("download_and_register_eps", params, (chunk: string) => {
                const sepIndex = chunk.indexOf('|');
                if (sepIndex >= 0) {
                    const name = chunk.substring(0, sepIndex) || null;
                    const percent = parseFloat(chunk.substring(sepIndex + 1)) || 0;
                    progressCallback(name, percent);
                }
            });
        } else {
            this.coreInterop.executeCommand("download_and_register_eps", params);
        }
    }
}
