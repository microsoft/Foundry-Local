import { Configuration, FoundryLocalConfig } from './configuration.js';
import { CoreInterop } from './detail/coreInterop.js';
import { ModelLoadManager } from './detail/modelLoadManager.js';
import { Catalog } from './catalog.js';
import { ResponsesClient } from './openai/responsesClient.js';
import { EpInfo, EpDownloadResult } from './types.js';

function isAbortSignal(value: unknown): value is AbortSignal {
    return typeof value === 'object'
        && value !== null
        && 'aborted' in value
        && typeof (value as AbortSignal).aborted === 'boolean';
}

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
     * Discovers available execution providers (EPs) and their registration status.
     * @returns An array of EpInfo describing each available EP.
     */
    public discoverEps(): EpInfo[] {
        const response = this.coreInterop.executeCommand("discover_eps");
        type RawEpInfo = {
            Name: string;
            IsRegistered: boolean;
        };

        try {
            const raw = JSON.parse(response) as RawEpInfo[];
            return raw.map((ep) => ({
                name: ep.Name,
                isRegistered: ep.IsRegistered
            }));
        } catch (error) {
            throw new Error(`Failed to decode JSON response from discover_eps: ${error}. Response was: ${response}`);
        }
    }

    /**
     * Downloads and registers execution providers.
     * @returns A promise that resolves with an EpDownloadResult describing the outcome.
     */
    public downloadAndRegisterEps(): Promise<EpDownloadResult>;
    /**
     * Downloads and registers execution providers.
     * @param signal - Optional AbortSignal used to cancel an in-progress download.
     * @returns A promise that resolves with an EpDownloadResult describing the outcome.
     */
    public downloadAndRegisterEps(signal: AbortSignal): Promise<EpDownloadResult>;
    /**
     * Downloads and registers execution providers.
     * @param names - Array of EP names to download.
     * @returns A promise that resolves with an EpDownloadResult describing the outcome.
     */
    public downloadAndRegisterEps(names: string[]): Promise<EpDownloadResult>;
    /**
     * Downloads and registers execution providers.
     * @param names - Array of EP names to download.
     * @param signal - Optional AbortSignal used to cancel an in-progress download.
     * @returns A promise that resolves with an EpDownloadResult describing the outcome.
     */
    public downloadAndRegisterEps(names: string[], signal: AbortSignal): Promise<EpDownloadResult>;
    /**
     * Downloads and registers execution providers, reporting progress.
     * @param progressCallback - Callback invoked with (epName, percent) as each EP downloads. Percent is 0-100.
     * @returns A promise that resolves with an EpDownloadResult describing the outcome.
     */
    public downloadAndRegisterEps(progressCallback: (epName: string, percent: number) => void): Promise<EpDownloadResult>;
    /**
     * Downloads and registers execution providers, reporting progress.
     * @param progressCallback - Callback invoked with (epName, percent) as each EP downloads. Percent is 0-100.
     * @param signal - Optional AbortSignal used to cancel an in-progress download.
     * @returns A promise that resolves with an EpDownloadResult describing the outcome.
     */
    public downloadAndRegisterEps(progressCallback: (epName: string, percent: number) => void, signal: AbortSignal): Promise<EpDownloadResult>;
    /**
     * Downloads and registers execution providers, reporting progress.
     * @param names - Array of EP names to download.
     * @param progressCallback - Callback invoked with (epName, percent) as each EP downloads. Percent is 0-100.
     * @returns A promise that resolves with an EpDownloadResult describing the outcome.
     */
    public downloadAndRegisterEps(names: string[], progressCallback: (epName: string, percent: number) => void): Promise<EpDownloadResult>;
    /**
     * Downloads and registers execution providers, reporting progress.
     * @param names - Array of EP names to download.
     * @param progressCallback - Callback invoked with (epName, percent) as each EP downloads. Percent is 0-100.
     * @param signal - Optional AbortSignal used to cancel an in-progress download.
     * @returns A promise that resolves with an EpDownloadResult describing the outcome.
     */
    public downloadAndRegisterEps(names: string[], progressCallback: (epName: string, percent: number) => void, signal: AbortSignal): Promise<EpDownloadResult>;
    public async downloadAndRegisterEps(
        namesOrCallbackOrSignal?: string[] | ((epName: string, percent: number) => void) | AbortSignal,
        progressCallbackOrSignal?: ((epName: string, percent: number) => void) | AbortSignal,
        maybeSignal?: AbortSignal
    ): Promise<EpDownloadResult> {
        let progressCallback: ((epName: string, percent: number) => void) | undefined;
        let names: string[] | undefined;
        let signal: AbortSignal | undefined;

        if (Array.isArray(namesOrCallbackOrSignal)) {
            names = namesOrCallbackOrSignal;
            if (typeof progressCallbackOrSignal === 'function') {
                progressCallback = progressCallbackOrSignal;
                signal = maybeSignal;
            } else if (isAbortSignal(progressCallbackOrSignal)) {
                signal = progressCallbackOrSignal;
            }
        } else if (typeof namesOrCallbackOrSignal === 'function') {
            progressCallback = namesOrCallbackOrSignal;
            if (isAbortSignal(progressCallbackOrSignal)) {
                signal = progressCallbackOrSignal;
            }
        } else if (isAbortSignal(namesOrCallbackOrSignal)) {
            signal = namesOrCallbackOrSignal;
        } else {
            signal = maybeSignal;
        }

        const params: { Params?: { Names: string } } = {};
        if (names && names.length > 0) {
            params.Params = { Names: names.join(",") };
        }

        type RawEpDownloadResult = {
            Success: boolean;
            Status: string;
            RegisteredEps: string[];
            FailedEps: string[];
        };

        let response: string;

        if (progressCallback) {
            response = await this.coreInterop.executeCommandStreaming(
                "download_and_register_eps",
                Object.keys(params).length > 0 ? params : undefined,
                (chunk: string) => {
                    const sepIndex = chunk.indexOf('|');
                    if (sepIndex >= 0) {
                        const epName = chunk.substring(0, sepIndex);
                        const percent = parseFloat(chunk.substring(sepIndex + 1));
                        if (!isNaN(percent)) {
                            progressCallback(epName || '', percent);
                        }
                    }
                },
                signal
            );
        } else {
            response = await this.coreInterop.executeCommandStreaming(
                "download_and_register_eps",
                Object.keys(params).length > 0 ? params : undefined,
                () => {}, // no-op callback
                signal
            );
        }

        let epResult: EpDownloadResult;
        try {
            const raw = JSON.parse(response) as RawEpDownloadResult;
            epResult = {
                success: raw.Success,
                status: raw.Status,
                registeredEps: raw.RegisteredEps,
                failedEps: raw.FailedEps
            };
        } catch (error) {
            throw new Error(`Failed to decode JSON response from download_and_register_eps: ${error}. Response was: ${response}`);
        }

        // Invalidate the catalog cache if any EP was newly registered so the next access
        // re-fetches models with the updated set of available EPs.
        if (epResult.success || epResult.registeredEps.length > 0) {
            this._catalog.invalidateCache();
        }

        return epResult;
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
}
