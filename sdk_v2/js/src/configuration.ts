/**
 * Configuration options for the Foundry Local SDK.
 * Use a plain object with these properties to configure the SDK.
 */
export interface FoundryLocalConfig {
    /**
     * **REQUIRED** The name of the application using the SDK.
     * Used for identifying the application in logs and telemetry.
     */
    appName: string;

    /**
     * The directory where application data should be stored.
     * Optional. Defaults to `{user_home}/.{appName}`.
     */
    appDataDir?: string;

    /**
     * The directory where models are downloaded and cached.
     * Optional. Defaults to `{appDataDir}/cache/models`.
     */
    modelCacheDir?: string;

    /**
     * The directory where log files are written.
     * Optional. Defaults to `{appDataDir}/logs`.
     */
    logsDir?: string;

    /**
     * The logging level for the SDK.
     * Optional. Valid values: 'trace', 'debug', 'info', 'warn', 'error', 'fatal'.
     * Defaults to 'warn'.
     */
    logLevel?: 'trace' | 'debug' | 'info' | 'warn' | 'error' | 'fatal';

    /**
     * The URL(s) for the local web service to bind to.
     * Optional. Multiple URLs can be separated by semicolons.
     * Example: "http://127.0.0.1:8080"
     */
    webServiceUrls?: string;

    /**
     * The external URL if the web service is running in a separate process.
     * Optional. This is used to connect to an existing service instance.
     */
    serviceEndpoint?: string;

    /**
     * The path to the directory containing the native Foundry Local Core libraries.
     * Optional. This directory must contain `Microsoft.AI.Foundry.Local.Core`, `onnxruntime`, and `onnxruntime-genai` binaries.
     * If not provided, the SDK attempts to discover them in standard locations.
     */
    libraryPath?: string;

    /**
     * Additional settings to pass to the core.
     * Optional. Internal use only.
     */
    additionalSettings?: { [key: string]: string };
}

// Log level mapping from JS-style to C#-style
const LOG_LEVEL_MAP: { [key: string]: string } = {
    'trace': 'Verbose',
    'debug': 'Debug',
    'info': 'Information',
    'warn': 'Warning',
    'error': 'Error',
    'fatal': 'Fatal'
};

// Internal Configuration class (not exported)
export class Configuration {
    public params: { [key: string]: string };

    constructor(config: FoundryLocalConfig) {
        if (!config) {
            throw new Error("Configuration must be provided.");
        }

        if (!config.appName || config.appName.trim() === "") {
            throw new Error("appName must be set to a valid application name.");
        }

        this.params = {
            'AppName': config.appName
        };

        if (config.appDataDir) this.params['AppDataDir'] = config.appDataDir;
        if (config.modelCacheDir) this.params['ModelCacheDir'] = config.modelCacheDir;
        if (config.logsDir) this.params['LogsDir'] = config.logsDir;
        if (config.logLevel) this.params['LogLevel'] = LOG_LEVEL_MAP[config.logLevel] || config.logLevel;
        if (config.webServiceUrls) this.params['WebServiceUrls'] = config.webServiceUrls;
        if (config.serviceEndpoint) this.params['WebServiceExternalUrl'] = config.serviceEndpoint;
        if (config.libraryPath) this.params['FoundryLocalCorePath'] = config.libraryPath;

        // Flatten additional settings into params
        if (config.additionalSettings) {
            for (const key in config.additionalSettings) {
                this.params[key] = config.additionalSettings[key];
            }
        }
    }
}

