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
    // Try to find test-data-shared relative to the git repo root
    const repoRoot = getGitRepoRoot();
    const testDataSharedPath = path.join(path.dirname(repoRoot), 'test-data-shared');
    return testDataSharedPath;
}

function getCoreLibPath(): string {
    let ext = '';
    if (process.platform === 'win32') ext = '.dll';
    else if (process.platform === 'linux') ext = '.so';
    else if (process.platform === 'darwin') ext = '.dylib';
    else throw new Error(`Unsupported platform: ${process.platform}`);

    // Look in node_modules/Microsoft.AI.Foundry.Local
    const coreDir = path.join(__dirname, '..', 'node_modules', 'Microsoft.AI.Foundry.Local');
    return path.join(coreDir, `Microsoft.AI.Foundry.Local.Core${ext}`);
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
    libraryPath: getCoreLibPath(),
    logLevel: 'warn'
};

export const TEST_MODEL_ALIAS = 'qwen2.5-0.5b';

export function getTestManager() {
    return FoundryLocalManager.create(TEST_CONFIG);
}
