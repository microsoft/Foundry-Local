// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Install script for foundry-local-sdk (standard variant).

'use strict';

const os = require('os');
const path = require('path');
const { NUGET_FEED, ORT_NIGHTLY_FEED, runInstall } = require('./install-utils.cjs');

const useNightly = process.env.npm_config_nightly === 'true';
const deps = require(path.resolve(__dirname, '..', '..', 'deps_versions.json'));

const ARTIFACTS = [
    { name: 'Microsoft.AI.Foundry.Local.Core', version: deps['foundry-local-core'].nuget, feed: ORT_NIGHTLY_FEED, nightly: useNightly },
    { name: os.platform() === 'linux' ? 'Microsoft.ML.OnnxRuntime.Gpu.Linux' : 'Microsoft.ML.OnnxRuntime.Foundry', version: deps.onnxruntime.version, feed: NUGET_FEED, nightly: false },
    { name: 'Microsoft.ML.OnnxRuntimeGenAI.Foundry', version: deps['onnxruntime-genai'].version, feed: ORT_NIGHTLY_FEED, nightly: useNightly },
];

(async () => {
    try {
        await runInstall(ARTIFACTS);
    } catch (err) {
        console.error('[foundry-local] Installation failed:', err instanceof Error ? err.message : err);
        process.exit(1);
    }
})();
