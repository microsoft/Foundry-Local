// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Install script for foundry-local-sdk (standard variant).

'use strict';

const os = require('os');
const { NUGET_FEED, ORT_NIGHTLY_FEED, runInstall, loadFlcVersion } = require('./install-utils.cjs');

const useNightly = process.env.npm_config_nightly === 'true';
const flcVersion = loadFlcVersion('Microsoft.AI.Foundry.Local.Core');

const ARTIFACTS = [
    { name: 'Microsoft.AI.Foundry.Local.Core', version: flcVersion, feed: ORT_NIGHTLY_FEED, nightly: useNightly },
    { name: os.platform() === 'linux' ? 'Microsoft.ML.OnnxRuntime.Gpu.Linux' : 'Microsoft.ML.OnnxRuntime.Foundry', version: '1.24.3', feed: NUGET_FEED, nightly: false },
    { name: 'Microsoft.ML.OnnxRuntimeGenAI.Foundry', version: '0.13.0-dev-20260319-1131106-439ca0d5', feed: ORT_NIGHTLY_FEED, nightly: useNightly },
];

(async () => {
    try {
        await runInstall(ARTIFACTS);
    } catch (err) {
        console.error('[foundry-local] Installation failed:', err instanceof Error ? err.message : err);
        process.exit(1);
    }
})();
