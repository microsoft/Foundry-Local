// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Install script for foundry-local-sdk-winml variant.
//
// Overwrites the standard native binaries inside foundry-local-sdk's own
// directory tree with the WinML variants (Core.WinML, ORT, GenAI).
// After this runs, everything lives under foundry-local-sdk — users import
// from 'foundry-local-sdk' and get WinML binaries transparently.

'use strict';

const fs = require('fs');
const path = require('path');
const { NUGET_FEED, ORT_NIGHTLY_FEED, runInstall } = require('./install-utils.cjs');

// WinML uses its own deps_versions_winml.json with the same key structure
// as the standard deps_versions.json — no variant-specific keys needed.
// deps_versions_winml.json lives at the package root when published, or at sdk/ in the repo.
const depsPath = fs.existsSync(path.resolve(__dirname, '..', 'deps_versions_winml.json'))
    ? path.resolve(__dirname, '..', 'deps_versions_winml.json')
    : path.resolve(__dirname, '..', '..', 'deps_versions_winml.json');
const deps = require(depsPath);
// Resolve foundry-local-sdk's binary directory
const sdkRoot = path.dirname(require.resolve('foundry-local-sdk/package.json'));
const platformKey = `${process.platform}-${process.arch}`;
const binDir = path.join(sdkRoot, 'node_modules', '@foundry-local-core', platformKey);

const ARTIFACTS = [
    { name: 'Microsoft.AI.Foundry.Local.Core.WinML', version: deps['foundry-local-core']['nuget'], feed: ORT_NIGHTLY_FEED },
    { name: 'Microsoft.ML.OnnxRuntime.Foundry', version: deps.onnxruntime.version, feed: NUGET_FEED },
    { name: 'Microsoft.ML.OnnxRuntimeGenAI.Foundry', version: deps['onnxruntime-genai']['version'], feed: ORT_NIGHTLY_FEED },
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
