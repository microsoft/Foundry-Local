// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Usage:
//   node script/pack.cjs          -> foundry-local-sdk-<version>.tgz
//   node script/pack.cjs winml    -> foundry-local-sdk-winml-<version>.tgz

'use strict';

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const pkgPath = path.join(__dirname, '..', 'package.json');
const original = fs.readFileSync(pkgPath, 'utf8');
const isWinML = process.argv[2] === 'winml';

try {
    const pkg = JSON.parse(original);
    if (isWinML) {
        pkg.name = '@prathikrao/foundry-local-sdk-winml';
        pkg.scripts.install = 'node script/install-winml.cjs';
        pkg.files = ['dist', 'script/install-winml.cjs', 'script/install-utils.cjs', 'script/preinstall.cjs'];
    } else {
        pkg.files = ['dist', 'script/install-standard.cjs', 'script/install-utils.cjs', 'script/preinstall.cjs'];
    }
    fs.writeFileSync(pkgPath, JSON.stringify(pkg, null, 2));
    execSync('npm pack', { cwd: path.join(__dirname, '..'), stdio: 'inherit' });
} finally {
    // Always restore original package.json
    fs.writeFileSync(pkgPath, original);
}
