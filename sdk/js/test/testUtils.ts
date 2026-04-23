import type { FoundryLocalConfig } from '../src/configuration.js';
import { FoundryLocalManager } from '../src/foundryLocalManager.js';
import path from 'path';
import fs from 'fs';

function getGitRepoRoot(): string {
    let current = process.cwd();
    while (true) {
        if (fs.existsSync(path.join(current, '.git'))) {
            return current;
        }
        current = path.dirname(current);
    }
}

function getTestDataSharedPath(): string {
    // Use FOUNDRY_TEST_DATA_DIR env var if set (CI), otherwise look for
    // test-data-shared as a sibling of the git repo root (local dev).
    const envPath = process.env.FOUNDRY_TEST_DATA_DIR;
    if (envPath && fs.existsSync(envPath)) {
        return envPath;
    }
    const repoRoot = getGitRepoRoot();
    const testDataSharedPath = path.join(path.dirname(repoRoot), 'test-data-shared');
    return testDataSharedPath;
}

// Replicates the IsRunningInCI logic from C# utils
function isRunningInCI(): boolean {
    const azureDevOps = process.env.TF_BUILD || 'false';
    const githubActions = process.env.GITHUB_ACTIONS || 'false';
    var res = azureDevOps.toLowerCase() === 'true' || githubActions.toLowerCase() === 'true';   
    return azureDevOps.toLowerCase() === 'true' || githubActions.toLowerCase() === 'true';
}

export const IS_RUNNING_IN_CI = isRunningInCI();

export const TEST_CONFIG: FoundryLocalConfig = {
    appName: 'FoundryLocalTest',
    modelCacheDir: getTestDataSharedPath(),
    logLevel: 'warn',
    logsDir: path.join(getGitRepoRoot(), 'sdk', 'js', 'logs'),
    additionalSettings: { 'Bootstrap': 'false' }
};

export const TEST_MODEL_ALIAS = 'qwen2.5-0.5b';
export const EMBEDDING_MODEL_ALIAS = 'qwen3-0.6b-embedding-generic-cpu';

// Detect whether the native addon is available by checking for the file on disk,
// mirroring the exact paths that CoreInterop.loadAddon() searches. This avoids
// the side effects of calling FoundryLocalManager.create() at module load time.
function checkNativeAddonAvailable(): boolean {
    const platform = process.platform;
    const arch = process.arch;
    const platformKey = `${platform}-${arch}`;
    // dist/ is the compiled output root; from there the SDK root is one level up
    const sdkRoot = path.resolve(getGitRepoRoot(), 'sdk', 'js', 'dist');
    const prebuiltPath = path.join(sdkRoot, 'prebuilds', platformKey, 'foundry_local_napi.node');
    const devPath = path.join(sdkRoot, 'native', 'build', 'Release', 'foundry_local_napi.node');
    return fs.existsSync(prebuiltPath) || fs.existsSync(devPath);
}

export const IS_NATIVE_ADDON_AVAILABLE = checkNativeAddonAvailable();

export function getTestManager() {
    return FoundryLocalManager.create(TEST_CONFIG);
}

export function getMultiplyTool() {
    const multiplyTool = {
        type: 'function',
        function: {
            name: 'multiply_numbers',
            description: 'A tool for multiplying two numbers.',
            parameters: {
                type: 'object',
                properties: {
                    first: {
                        type: 'integer',
                        description: 'The first number in the operation'
                    },
                    second: {
                        type: 'integer',
                        description: 'The second number in the operation'
                    }
                },
                required: ['first', 'second']
            }
        }
    };
    return multiplyTool;
}
