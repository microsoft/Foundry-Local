import path from 'path';
import fs from 'fs';
import { createRequire } from 'module';
import { fileURLToPath } from 'url';
import { Configuration } from '../configuration.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Load the prebuilt Node-API addon
const require = createRequire(import.meta.url);

interface NativeAddon {
    loadLibrary(corePath: string, depPaths?: string[]): void;
    executeCommand(command: string, dataJson: string): string;
    executeCommandWithBinary(command: string, dataJson: string, binaryBuffer: Buffer): string;
    executeCommandStreaming(command: string, dataJson: string, callback: (chunk: string) => void): Promise<string>;
}

function loadAddon(): NativeAddon {
    const platform = process.platform;
    const arch = process.arch;
    const platformKey = `${platform}-${arch}`;

    // The prebuilt addon ships inside the SDK package under prebuilds/<platform>/
    const sdkRoot = path.resolve(__dirname, '..', '..');
    const prebuiltPath = path.join(sdkRoot, 'prebuilds', platformKey, 'foundry_local_napi.node');

    if (fs.existsSync(prebuiltPath)) {
        return require(prebuiltPath) as NativeAddon;
    }

    // Fallback: development builds from node-gyp (sdk contributors)
    const devPath = path.join(sdkRoot, 'native', 'build', 'Release', 'foundry_local_napi.node');
    if (fs.existsSync(devPath)) {
        return require(devPath) as NativeAddon;
    }

    throw new Error(
        `Could not find foundry_local_napi.node for platform ${platformKey}. ` +
        `Searched: ${prebuiltPath}, ${devPath}. ` +
        `Please ensure the SDK was installed correctly or run 'npm run build:native' to compile from source.`
    );
}

export class CoreInterop {
    private addon: NativeAddon;

    private static _getLibraryExtension(): string {
        const platform = process.platform;
        if (platform === 'win32') return '.dll';
        if (platform === 'linux') return '.so';
        if (platform === 'darwin') return '.dylib';
        throw new Error(`Unsupported platform: ${platform}`);
    }

    private static _resolveDefaultCorePath(config: Configuration): string | null {
        const platform = process.platform;
        const arch = process.arch;
        const platformKey = `${platform}-${arch}`;

        // Resolve the platform package directory at node_modules/@foundry-local-core/<platform>,
        // the shared location where install scripts place the native binaries.
        const sdkRoot = path.resolve(__dirname, '..', '..');
        const packageDir = path.join(sdkRoot, 'node_modules', '@foundry-local-core', platformKey);
        const ext = CoreInterop._getLibraryExtension();
        
        const corePath = path.join(packageDir, `Microsoft.AI.Foundry.Local.Core${ext}`);
        if (fs.existsSync(corePath)) {
            config.params['FoundryLocalCorePath'] = corePath;

            // Auto-detect if WinML Bootstrap is needed by checking for Bootstrap DLL in FoundryLocalCorePath
            // Only auto-set if the user hasn't explicitly provided a value
            if (!('Bootstrap' in config.params)) {
                const bootstrapDllPath = path.join(packageDir, 'Microsoft.WindowsAppRuntime.Bootstrap.dll');
                if (fs.existsSync(bootstrapDllPath)) {
                    // WinML Bootstrap DLL found, enable bootstrapping
                    config.params['Bootstrap'] = 'true';
                }
            }
            
            return corePath;
        }

        return null;
    }

    constructor(config: Configuration) {
        this.addon = loadAddon();

        const corePath = config.params['FoundryLocalCorePath'] || CoreInterop._resolveDefaultCorePath(config);
        
        if (!corePath) {
            throw new Error("FoundryLocalCorePath not specified in configuration and could not auto-discover binaries. Please run 'npm install' to download native libraries.");
        }

        const coreDir = path.dirname(corePath);
        const ext = CoreInterop._getLibraryExtension();
        
        // On Windows, explicitly load dependencies to work around DLL resolution challenges
        const depPaths: string[] = [];
        if (process.platform === 'win32') {
            depPaths.push(path.join(coreDir, `onnxruntime${ext}`));
            depPaths.push(path.join(coreDir, `onnxruntime-genai${ext}`));
            process.env.PATH = `${coreDir};${process.env.PATH}`;
        }

        this.addon.loadLibrary(corePath, depPaths.length > 0 ? depPaths : undefined);
    }

    public executeCommand(command: string, params?: any): string {
        const dataStr = params ? JSON.stringify(params) : '';
        return this.addon.executeCommand(command, dataStr);
    }

    /**
     * Execute a native command with binary data (e.g., audio PCM bytes).
     * Uses the execute_command_with_binary native entry point which accepts
     * both JSON params and raw binary data via StreamingRequestBuffer.
     */
    public executeCommandWithBinary(command: string, params: any, binaryData: Uint8Array): string {
        const dataStr = params ? JSON.stringify(params) : '';
        const binBuf = Buffer.from(binaryData.buffer, binaryData.byteOffset, binaryData.byteLength);
        return this.addon.executeCommandWithBinary(command, dataStr, binBuf);
    }

    public executeCommandStreaming(command: string, params: any, callback: (chunk: string) => void): Promise<string> {
        const dataStr = params ? JSON.stringify(params) : '';
        return this.addon.executeCommandStreaming(command, dataStr, callback);
    }

}
