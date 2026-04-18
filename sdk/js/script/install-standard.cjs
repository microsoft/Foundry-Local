// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Install script for foundry-local-sdk (standard variant).

'use strict';

const os = require('os');
const { NUGET_FEED, ORT_NIGHTLY_FEED, runInstall } = require('./install-utils.cjs');

const ARTIFACTS = [
    { name: 'Microsoft.AI.Foundry.Local.Core', version: '1.0.0-dev-202604162310-d77f1cbc', feed: ORT_NIGHTLY_FEED },
    { name: os.platform() === 'linux' ? 'Microsoft.ML.OnnxRuntime.Gpu.Linux' : 'Microsoft.ML.OnnxRuntime.Foundry', version: '1.25.0-dev-20260402-0015-6bbcde989a', feed: ORT_NIGHTLY_FEED },
    { name: 'Microsoft.ML.OnnxRuntimeGenAI.Foundry', version: '0.13.0', feed: NUGET_FEED },
];

(async () => {
    try {
        await runInstall(ARTIFACTS);
    } catch (err) {
        console.error('[foundry-local] Installation failed:', err instanceof Error ? err.message : err);
        process.exit(1);
    }
})();
