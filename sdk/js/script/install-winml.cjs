// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Install script for foundry-local-sdk-winml variant.

'use strict';

const path = require('path');
const { NUGET_FEED, ORT_NIGHTLY_FEED, runInstall } = require('./install-utils.cjs');

const useNightly = process.env.npm_config_nightly === 'true';
const deps = require(path.resolve(__dirname, '..', '..', 'deps_versions.json'));

const ARTIFACTS = [
    { name: 'Microsoft.AI.Foundry.Local.Core.WinML', version: deps['foundry-local-core']['nuget-winml'], feed: ORT_NIGHTLY_FEED, nightly: useNightly },
    { name: 'Microsoft.ML.OnnxRuntime.Foundry', version: deps.onnxruntime.winml, feed: NUGET_FEED, nightly: false },
    { name: 'Microsoft.ML.OnnxRuntimeGenAI.WinML', version: deps['onnxruntime-genai']['nuget'], feed: ORT_NIGHTLY_FEED, nightly: useNightly },
];

(async () => {
    try {
        await runInstall(ARTIFACTS);
    } catch (err) {
        console.error('Failed to install WinML artifacts:', err);
        process.exit(1);
    }
})();
