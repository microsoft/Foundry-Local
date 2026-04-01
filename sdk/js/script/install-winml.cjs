// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Install script for foundry-local-sdk-winml variant.

'use strict';

const { NUGET_FEED, ORT_NIGHTLY_FEED, runInstall } = require('./install-utils.cjs');

const useNightly = process.env.npm_config_nightly === 'true';

const ARTIFACTS = [
    { name: 'Microsoft.AI.Foundry.Local.Core.WinML', version: '0.9.0-dev-202603310538-f6efa8d3', feed: ORT_NIGHTLY_FEED, nightly: useNightly },
    { name: 'Microsoft.ML.OnnxRuntime.Foundry', version: '1.25.0-dev-20260212-0905-d7f32e2595', feed: ORT_NIGHTLY_FEED, nightly: useNightly },
    { name: 'Microsoft.ML.OnnxRuntimeGenAI.WinML', version: '0.13.0-dev-20260319-1131106-439ca0d5', feed: NUGET_FEED, nightly: false },
];

(async () => {
    try {
        await runInstall(ARTIFACTS);
    } catch (err) {
        console.error('Failed to install WinML artifacts:', err);
        process.exit(1);
    }
})();
