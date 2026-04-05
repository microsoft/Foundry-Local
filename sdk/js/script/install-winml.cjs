// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Install script for foundry-local-sdk-winml variant.
//
// This package is a thin wrapper around foundry-local-sdk. It:
//   1. Installs foundry-local-sdk with --ignore-scripts so the standard
//      native binary download is skipped.
//   2. Downloads the WinML-specific native binaries (Core.WinML, ORT, GenAI)
//      into the standard SDK's platform package directory so that
//      `require('foundry-local-sdk')` picks them up at runtime.

'use strict';

const path = require('path');
const { NUGET_FEED, ORT_NIGHTLY_FEED, runInstall } = require('./install-utils.cjs');

const useNightly = process.env.npm_config_nightly === 'true';

// Step 1: Install the standard SDK dependency without running its install script.

// Run the preinstall from the standard SDK to create the platform package skeleton
const sdkRoot = path.dirname(require.resolve('foundry-local-sdk/package.json'));
const preinstallPath = path.join(sdkRoot, 'script', 'preinstall.cjs');
try {
    require(preinstallPath);
} catch (e) {
    console.log('[foundry-local-winml] Note: preinstall skeleton already exists or not needed.');
}

// Step 2: Download WinML native binaries into the standard SDK's platform directory.
const ARTIFACTS = [
    { name: 'Microsoft.AI.Foundry.Local.Core.WinML', version: '0.9.0-dev-202603310538-f6efa8d3', feed: ORT_NIGHTLY_FEED, nightly: useNightly },
    { name: 'Microsoft.ML.OnnxRuntime.Foundry', version: '1.23.2.3', feed: NUGET_FEED, nightly: false },
    { name: 'Microsoft.ML.OnnxRuntimeGenAI.WinML', version: '0.13.0-dev-20260319-1131106-439ca0d5', feed: ORT_NIGHTLY_FEED, nightly: useNightly },
];

(async () => {
    try {
        await runInstall(ARTIFACTS, sdkRoot);
    } catch (err) {
        console.error('Failed to install WinML artifacts:', err);
        process.exit(1);
    }
})();
