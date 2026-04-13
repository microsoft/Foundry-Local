// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Install script for foundry-local-sdk-winml variant.
//
// Overwrites the standard native binaries inside foundry-local-sdk's own
// directory tree with the WinML variants (Core.WinML, ORT, GenAI).
// After this runs, everything lives under foundry-local-sdk — users import
// from 'foundry-local-sdk' and get WinML binaries transparently.

'use strict';

const path = require('path');
const { NUGET_FEED, ORT_NIGHTLY_FEED, runInstall } = require('./install-utils.cjs');

// Resolve foundry-local-sdk's binary directory
const sdkRoot = path.dirname(require.resolve('foundry-local-sdk/package.json'));
const platformKey = `${process.platform}-${process.arch}`;
const binDir = path.join(sdkRoot, 'node_modules', '@foundry-local-core', platformKey);

const ARTIFACTS = [
    { name: 'Microsoft.AI.Foundry.Local.Core.WinML', version: '1.0.0-dev-20260411T003630-592f019', feed: ORT_NIGHTLY_FEED },
    { name: 'Microsoft.ML.OnnxRuntime.Foundry', version: '1.24.4', feed: NUGET_FEED },
    { name: 'Microsoft.ML.OnnxRuntimeGenAI.Foundry', version: '0.13.1', feed: NUGET_FEED },
];

(async () => {
    try {
        // Force override into foundry-local-sdk's binary directory
        await runInstall(ARTIFACTS, { force: true, binDir });
    } catch (err) {
        console.error('Failed to install WinML artifacts:', err);
        process.exit(1);
    }
})();
