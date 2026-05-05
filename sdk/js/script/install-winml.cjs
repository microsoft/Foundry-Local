// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Install script for foundry-local-sdk-winml variant.
//
// Overwrites the standard native binaries inside foundry-local-sdk's own
// directory tree with the WinML variants (Core.WinML, ORT, GenAI).
// After this runs, everything lives under foundry-local-sdk — users import
// from 'foundry-local-sdk' and get WinML binaries transparently.

'use strict';

const fs = require('fs');
const path = require('path');
const { runInstall } = require('./install-utils.cjs');

// deps_versions_winml.json lives at the package root when published, or at sdk/ in the repo.
const depsPath = fs.existsSync(path.resolve(__dirname, '..', 'deps_versions_winml.json'))
    ? path.resolve(__dirname, '..', 'deps_versions_winml.json')
    : path.resolve(__dirname, '..', '..', 'deps_versions_winml.json');
const deps = require(depsPath);

function resolveFoundryLocalSdkRoot() {
    try {
        return path.dirname(require.resolve('foundry-local-sdk/package.json'));
    } catch (err) {
        const packageRoot = path.resolve(__dirname, '..');
        const packageJson = path.join(packageRoot, 'package.json');
        if (fs.existsSync(packageJson)) {
            const pkg = JSON.parse(fs.readFileSync(packageJson, 'utf8'));
            if (pkg.name === 'foundry-local-sdk') {
                return packageRoot;
            }
        }

        throw err;
    }
}

// Resolve foundry-local-sdk's binary directory
const sdkRoot = resolveFoundryLocalSdkRoot();
const platformKey = `${process.platform}-${process.arch}`;
const binDir = path.join(sdkRoot, 'foundry-local-core', platformKey);

function resolveWindowsAiMachineLearningVersion() {
    const override = process.env.FOUNDRY_WINDOWS_AI_MACHINELEARNING_VERSION;
    if (override) {
        return override;
    }

    const dep = deps['windows-ai-machinelearning'];
    if (!dep || !dep.version) {
        throw new Error('deps_versions_winml.json is missing windows-ai-machinelearning.version');
    }
    return dep.version;
}

const ARTIFACTS = [
    { name: 'Microsoft.AI.Foundry.Local.Core.WinML', version: deps['foundry-local-core']['nuget'] },
    { name: 'Microsoft.ML.OnnxRuntime.Foundry', version: deps.onnxruntime.version },
    { name: 'Microsoft.ML.OnnxRuntimeGenAI.Foundry', version: deps['onnxruntime-genai']['version'] },
];

if (process.platform === 'win32') {
    ARTIFACTS.push({
        name: 'Microsoft.Windows.AI.MachineLearning',
        version: resolveWindowsAiMachineLearningVersion(),
    });
}

(async () => {
    try {
        // Force override into foundry-local-sdk's binary directory
        await runInstall(ARTIFACTS, { force: true, binDir });
    } catch (err) {
        console.error('Failed to install WinML artifacts:', err);
        process.exit(1);
    }
})();
