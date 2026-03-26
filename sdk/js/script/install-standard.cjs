// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Install script for foundry-local-sdk (standard variant).

'use strict';

const os = require('os');
const { NUGET_FEED, ORT_NIGHTLY_FEED, runInstall } = require('./install-utils.cjs');

const useNightly = process.env.npm_config_nightly === 'true';

const ARTIFACTS = [
    { name: 'Microsoft.AI.Foundry.Local.Core', version: '0.9.0.8-rc3', feed: ORT_NIGHTLY_FEED, nightly: useNightly },
    { name: os.platform() === 'linux' ? 'Microsoft.ML.OnnxRuntime.Gpu.Linux' : 'Microsoft.ML.OnnxRuntime.Foundry', version: '1.24.3', feed: NUGET_FEED, nightly: false },
    { name: 'Microsoft.ML.OnnxRuntimeGenAI.Foundry', version: '0.12.2', feed: NUGET_FEED, nightly: false },
];

runInstall(ARTIFACTS);
