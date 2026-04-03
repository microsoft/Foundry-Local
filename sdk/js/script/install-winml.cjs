// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Install script for foundry-local-sdk-winml variant.

'use strict';

const fs = require('fs');
const path = require('path');
const { NUGET_FEED, ORT_NIGHTLY_FEED, runInstall } = require('./install-utils.cjs');

const useNightly = process.env.npm_config_nightly === 'true';

// Read FLC version from FLC_VERSION_INFO.json (single source of truth)
const versionInfo = JSON.parse(fs.readFileSync(path.join(__dirname, '..', '..', '..', 'FLC_VERSION_INFO.json'), 'utf8'));
const flcVersion = versionInfo['Microsoft.AI.Foundry.Local.Core.WinML'];

const ARTIFACTS = [
    { name: 'Microsoft.AI.Foundry.Local.Core.WinML', version: flcVersion, feed: ORT_NIGHTLY_FEED, nightly: useNightly },
    { name: 'Microsoft.ML.OnnxRuntime.Foundry', version: '1.23.2.3', feed: NUGET_FEED, nightly: false },
    { name: 'Microsoft.ML.OnnxRuntimeGenAI.WinML', version: '0.13.0-dev-20260319-1131106-439ca0d5', feed: ORT_NIGHTLY_FEED, nightly: useNightly },
];

(async () => {
    try {
        await runInstall(ARTIFACTS);
    } catch (err) {
        console.error('Failed to install WinML artifacts:', err);
        process.exit(1);
    }
})();
