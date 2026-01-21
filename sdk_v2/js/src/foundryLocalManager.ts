import { Configuration, FoundryLocalConfig } from './configuration.js';
import { CoreInterop } from './detail/coreInterop.js';
import { ModelLoadManager } from './detail/modelLoadManager.js';
import { Catalog } from './catalog.js';
import { ChatClient } from './openai/chatClient.js';
import { AudioClient } from './openai/audioClient.js';

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
     * Ensures that the necessary execution providers (EPs) are downloaded.
     * Also serves as a manual trigger for EP download if ManualEpDownload is enabled.
     */
    public ensureEpsDownloaded(): void {
        const manualEpDownload = this.config.params["ManualEpDownload"];
        if (manualEpDownload && manualEpDownload.toLowerCase() === "true") {
            this.coreInterop.executeCommand("ensure_eps_downloaded");
        } else {
            throw new Error("Manual EP download is not enabled in the configuration.");
        }
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
}
