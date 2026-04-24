// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Install script for foundry-local-sdk (standard variant).

'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');

// If foundry-local-sdk-winml is also being installed, skip the standard binary
// download entirely — the winml install script will handle all binary provisioning.
// npm extracts all packages before running lifecycle scripts, so this check is reliable.
const winmlPkgJson = path.join(__dirname, '..', '..', 'foundry-local-sdk-winml', 'package.json');
if (fs.existsSync(winmlPkgJson)) {
    console.log('[foundry-local] foundry-local-sdk-winml detected. Deferring binary install to winml variant.');
    process.exit(0);
}

const { runInstall } = require('./install-utils.cjs');

// deps_versions.json lives at the package root when published, or at sdk/ in the repo.
const depsPath = fs.existsSync(path.resolve(__dirname, '..', 'deps_versions.json'))
    ? path.resolve(__dirname, '..', 'deps_versions.json')
    : path.resolve(__dirname, '..', '..', 'deps_versions.json');
const deps = require(depsPath);

const ARTIFACTS = [
    { name: 'Microsoft.AI.Foundry.Local.Core', version: deps['foundry-local-core'].nuget },
    { name: os.platform() === 'linux' ? 'Microsoft.ML.OnnxRuntime.Gpu.Linux' : 'Microsoft.ML.OnnxRuntime.Foundry', version: deps.onnxruntime.version },
    { name: 'Microsoft.ML.OnnxRuntimeGenAI.Foundry', version: deps['onnxruntime-genai'].version },
];

(async () => {
    try {
        await runInstall(ARTIFACTS);
    } catch (err) {
        console.error('[foundry-local] Installation failed:', err instanceof Error ? err.message : err);
        process.exit(1);
    }
})();
