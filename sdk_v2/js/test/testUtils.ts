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
    logsDir: path.join(getGitRepoRoot(), 'sdk_v2', 'js', 'logs')
};

export const TEST_MODEL_ALIAS = 'qwen2.5-0.5b';

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
