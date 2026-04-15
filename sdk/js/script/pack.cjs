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

// deps_versions.json lives in the parent sdk/ directory; copy it into the
// JS package root so that npm pack includes it in the tarball.
const pkgRoot = path.join(__dirname, '..');
const depsSource = path.join(pkgRoot, '..', 'deps_versions.json');
const depsDest = path.join(pkgRoot, 'deps_versions.json');
const depsWinmlSource = path.join(pkgRoot, '..', 'deps_versions_winml.json');
const depsWinmlDest = path.join(pkgRoot, 'deps_versions_winml.json');
const copiedFiles = [];

try {
    const pkg = JSON.parse(original);
    if (isWinML) {
        pkg.name = 'foundry-local-sdk-winml';
        pkg.description = 'Foundry Local JavaScript SDK – WinML variant';
        // The winml package is a thin wrapper: it depends on the standard SDK for all JS code
        // and only overrides the native binaries at install time.
        pkg.dependencies = { 'foundry-local-sdk': pkg.version };
        pkg.scripts = { install: 'node script/install-winml.cjs' };
        // No dist/ or preinstall needed — the standard SDK provides the JS code
        pkg.files = ['script/install-winml.cjs', 'script/install-utils.cjs', 'deps_versions_winml.json'];
        delete pkg.main;
        delete pkg.types;
        delete pkg.optionalDependencies;
        if (fs.existsSync(depsWinmlSource) && !fs.existsSync(depsWinmlDest)) {
            fs.copyFileSync(depsWinmlSource, depsWinmlDest);
            copiedFiles.push(depsWinmlDest);
        }
    } else {
        pkg.files = ['dist', 'prebuilds', 'script/install-standard.cjs', 'script/install-utils.cjs', 'script/preinstall.cjs', 'deps_versions.json'];
        if (fs.existsSync(depsSource) && !fs.existsSync(depsDest)) {
            fs.copyFileSync(depsSource, depsDest);
            copiedFiles.push(depsDest);
        }
    }
    fs.writeFileSync(pkgPath, JSON.stringify(pkg, null, 2));
    execSync('npm pack', { cwd: pkgRoot, stdio: 'inherit' });
} finally {
    // Always restore original package.json
    fs.writeFileSync(pkgPath, original);
    // Clean up copied deps_versions files
    for (const f of copiedFiles) {
        if (fs.existsSync(f)) fs.unlinkSync(f);
    }
}
