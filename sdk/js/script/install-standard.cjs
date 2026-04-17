// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Install script for foundry-local-sdk (standard variant).

'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { NUGET_FEED, runInstall } = require('./install-utils.cjs');

// deps_versions.json lives at the package root when published, or at sdk/ in the repo.
const depsPath = fs.existsSync(path.resolve(__dirname, '..', 'deps_versions.json'))
    ? path.resolve(__dirname, '..', 'deps_versions.json')
    : path.resolve(__dirname, '..', '..', 'deps_versions.json');
const deps = require(depsPath);

const ARTIFACTS = [
    { name: 'Microsoft.AI.Foundry.Local.Core', version: deps['foundry-local-core'].nuget, feed: NUGET_FEED },
    { name: os.platform() === 'linux' ? 'Microsoft.ML.OnnxRuntime.Gpu.Linux' : 'Microsoft.ML.OnnxRuntime.Foundry', version: deps.onnxruntime.version, feed: NUGET_FEED },
    { name: 'Microsoft.ML.OnnxRuntimeGenAI.Foundry', version: deps['onnxruntime-genai'].version, feed: NUGET_FEED },
];

(async () => {
    try {
        await runInstall(ARTIFACTS);
    } catch (err) {
        console.error('[foundry-local] Installation failed:', err instanceof Error ? err.message : err);
        process.exit(1);
    }
})();
