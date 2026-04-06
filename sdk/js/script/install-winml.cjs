// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Install script for foundry-local-sdk-winml variant.
//
// This package is a thin wrapper around foundry-local-sdk. It downloads
// WinML-specific native binaries (Core.WinML, ORT, GenAI) into the shared
// node_modules/@foundry-local-core/<platform> directory, overriding the
// standard binaries that foundry-local-sdk's install script placed there.

'use strict';

const { NUGET_FEED, ORT_NIGHTLY_FEED, runInstall } = require('./install-utils.cjs');

const ARTIFACTS = [
    { name: 'Microsoft.AI.Foundry.Local.Core.WinML', version: '0.9.0-dev-202603310538-f6efa8d3', feed: ORT_NIGHTLY_FEED },
    { name: 'Microsoft.ML.OnnxRuntime.Foundry', version: '1.23.2.3', feed: NUGET_FEED },
    { name: 'Microsoft.ML.OnnxRuntimeGenAI.Foundry', version: '0.13.0', feed: NUGET_FEED },
];

(async () => {
    try {
        // Force override since standard binaries were already installed by foundry-local-sdk
        await runInstall(ARTIFACTS, { force: true });
    } catch (err) {
        console.error('Failed to install WinML artifacts:', err);
        process.exit(1);
    }
})();
