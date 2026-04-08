// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Shared NuGet download and extraction utilities for install scripts.

'use strict';

const fs = require('fs');
const path = require('path');
const os = require('os');
const https = require('https');
const AdmZip = require('adm-zip');

const PLATFORM_MAP = {
  'win32-x64': 'win-x64',
  'win32-arm64': 'win-arm64',
  'linux-x64': 'linux-x64',
  'darwin-arm64': 'osx-arm64',
};
const platformKey = `${os.platform()}-${os.arch()}`;
const RID = PLATFORM_MAP[platformKey];
// Install binaries into node_modules/@foundry-local-core/<platform> so they
// are shared across foundry-local-sdk and foundry-local-sdk-winml.
const BIN_DIR = path.join(__dirname, '..', 'node_modules', '@foundry-local-core', platformKey);
const EXT = os.platform() === 'win32' ? '.dll' : os.platform() === 'darwin' ? '.dylib' : '.so';

const REQUIRED_FILES = [
  `Microsoft.AI.Foundry.Local.Core${EXT}`,
  `${os.platform() === 'win32' ? '' : 'lib'}onnxruntime${EXT}`,
  `${os.platform() === 'win32' ? '' : 'lib'}onnxruntime-genai${EXT}`,
];

const NUGET_FEED = 'https://api.nuget.org/v3/index.json';
const ORT_NIGHTLY_FEED = 'https://pkgs.dev.azure.com/aiinfra/PublicPackages/_packaging/ORT-Nightly/nuget/v3/index.json';

// --- Download helpers ---

async function downloadWithRetryAndRedirects(url, destStream = null) {
    const maxRedirects = 5;
    let currentUrl = url;
    let redirects = 0;

    while (redirects < maxRedirects) {
        const response = await new Promise((resolve, reject) => {
            https.get(currentUrl, (res) => resolve(res))
                 .on('error', reject);
        });

        if (response.statusCode >= 300 && response.statusCode < 400 && response.headers.location) {
            currentUrl = response.headers.location;
            response.resume();
            redirects++;
            console.log(`  Following redirect to ${new URL(currentUrl).host}...`);
            continue;
        }

        if (response.statusCode !== 200) {
            throw new Error(`Download failed with status ${response.statusCode}: ${currentUrl}`);
        }

        if (destStream) {
            response.pipe(destStream);
            return new Promise((resolve, reject) => {
                destStream.on('finish', resolve);
                destStream.on('error', reject);
                response.on('error', reject);
            });
        } else {
            let data = '';
            response.on('data', chunk => data += chunk);
            return new Promise((resolve, reject) => {
                response.on('end', () => resolve(data));
                response.on('error', reject);
            });
        }
    }
    throw new Error('Too many redirects');
}

async function downloadJson(url) {
    return JSON.parse(await downloadWithRetryAndRedirects(url));
}

async function downloadFile(url, dest) {
    const file = fs.createWriteStream(dest);
    try {
        await downloadWithRetryAndRedirects(url, file);
        file.close();
    } catch (e) {
        file.close();
        if (fs.existsSync(dest)) fs.unlinkSync(dest);
        throw e;
    }
}

const serviceIndexCache = new Map();

async function getBaseAddress(feedUrl) {
    if (!serviceIndexCache.has(feedUrl)) {
        serviceIndexCache.set(feedUrl, await downloadJson(feedUrl));
    }
    const resources = serviceIndexCache.get(feedUrl).resources || [];
    const res = resources.find(r => r['@type'] && r['@type'].startsWith('PackageBaseAddress/3.0.0'));
    if (!res) throw new Error('Could not find PackageBaseAddress/3.0.0 in NuGet feed.');
    const baseAddress = res['@id'];
    return baseAddress.endsWith('/') ? baseAddress : baseAddress + '/';
}

async function installPackage(artifact, tempDir, binDir) {
    const pkgName = artifact.name;
    const pkgVer = artifact.version;

    const baseAddress = await getBaseAddress(artifact.feed);
    const nameLower = pkgName.toLowerCase();
    const verLower = pkgVer.toLowerCase();
    const downloadUrl = `${baseAddress}${nameLower}/${verLower}/${nameLower}.${verLower}.nupkg`;

    const nupkgPath = path.join(tempDir, `${pkgName}.${pkgVer}.nupkg`);
    console.log(`  Downloading ${pkgName} ${pkgVer}...`);
    await downloadFile(downloadUrl, nupkgPath);

    console.log(`  Extracting...`);
    const zip = new AdmZip(nupkgPath);
    const targetPathPrefix = `runtimes/${RID}/native/`.toLowerCase();
    const entries = zip.getEntries().filter(e => {
        const p = e.entryName.toLowerCase();
        return p.includes(targetPathPrefix) && p.endsWith(EXT);
    });

    if (entries.length > 0) {
        entries.forEach(entry => {
            zip.extractEntryTo(entry, binDir, false, true);
            console.log(`    Extracted ${entry.name}`);
        });
    } else {
        console.warn(`    No files found for RID ${RID} in ${pkgName}.`);
    }

    // Update platform package.json version for Core packages
    if (pkgName.startsWith('Microsoft.AI.Foundry.Local.Core')) {
        const pkgJsonPath = path.join(binDir, 'package.json');
        if (fs.existsSync(pkgJsonPath)) {
            const pkgJson = JSON.parse(fs.readFileSync(pkgJsonPath, 'utf8'));
            pkgJson.version = pkgVer;
            fs.writeFileSync(pkgJsonPath, JSON.stringify(pkgJson, null, 2));
        }
    }
}

async function runInstall(artifacts, options) {
    if (!RID) {
        console.warn(`[foundry-local] Unsupported platform: ${platformKey}. Skipping.`);
        return;
    }

    const force = options && options.force;
    const binDir = (options && options.binDir) || BIN_DIR;

    if (!force && fs.existsSync(binDir) && REQUIRED_FILES.every(f => fs.existsSync(path.join(binDir, f)))) {
        console.log(`[foundry-local] Native libraries already installed.`);
        return;
    }

    console.log(`[foundry-local] Installing native libraries for ${RID}...`);
    // Clear existing binaries so stale DLLs from a prior variant (e.g. standard)
    // don't persist when a different variant (e.g. WinML) is installed.
    if (fs.existsSync(binDir)) {
        for (const file of fs.readdirSync(binDir)) {
            const filePath = path.join(binDir, file);
            fs.unlinkSync(filePath);
        }
    }
    fs.mkdirSync(binDir, { recursive: true });

    const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'foundry-install-'));
    try {
        for (const artifact of artifacts) {
            await installPackage(artifact, tempDir, binDir);
        }
        console.log('[foundry-local] Installation complete.');
    } finally {
        try { fs.rmSync(tempDir, { recursive: true, force: true }); } catch {}
    }
}

module.exports = { NUGET_FEED, ORT_NIGHTLY_FEED, runInstall };
