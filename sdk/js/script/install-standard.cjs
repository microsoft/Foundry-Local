// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Install script for foundry-local-sdk (standard variant).

'use strict';

const fs = require('fs');
const path = require('path');
const os = require('os');

// If foundry-local-sdk-winml is also being installed, skip the standard binary
// download entirely — the winml install script will handle all binary provisioning.
// npm extracts all packages before running lifecycle scripts, so this check is reliable.
const winmlPkgJson = path.join(__dirname, '..', '..', 'foundry-local-sdk-winml', 'package.json');
if (fs.existsSync(winmlPkgJson)) {
    console.log('[foundry-local] foundry-local-sdk-winml detected. Deferring binary install to winml variant.');
    process.exit(0);
}

const { NUGET_FEED, ORT_NIGHTLY_FEED, runInstall } = require('./install-utils.cjs');

const ARTIFACTS = [
    { name: 'Microsoft.AI.Foundry.Local.Core', version: '1.0.0-rc5', feed: ORT_NIGHTLY_FEED },
    { name: os.platform() === 'linux' ? 'Microsoft.ML.OnnxRuntime.Gpu.Linux' : 'Microsoft.ML.OnnxRuntime.Foundry', version: '1.24.4', feed: NUGET_FEED },
    { name: 'Microsoft.ML.OnnxRuntimeGenAI.Foundry', version: '0.13.1', feed: NUGET_FEED },
];

(async () => {
    try {
        await runInstall(ARTIFACTS);
    } catch (err) {
        console.error('[foundry-local] Installation failed:', err instanceof Error ? err.message : err);
        process.exit(1);
    }
})();
