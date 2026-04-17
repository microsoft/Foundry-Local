// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Copies the locally-built Node-API addon into the prebuilds directory
// so that CoreInterop can find it at runtime during development.

'use strict';

const fs = require('fs');
const path = require('path');

const platformKey = `${process.platform}-${process.arch}`;
const source = path.join(__dirname, '..', 'native', 'build', 'Release', 'foundry_local_napi.node');
const destDir = path.join(__dirname, '..', 'prebuilds', platformKey);
const dest = path.join(destDir, 'foundry_local_napi.node');

if (!fs.existsSync(source)) {
    console.warn(`[copy-addon] Addon not found at ${source}. Run 'npm run build:native' first.`);
    process.exit(1);
}

fs.mkdirSync(destDir, { recursive: true });
fs.copyFileSync(source, dest);
console.log(`[copy-addon] Copied addon to ${dest}`);
