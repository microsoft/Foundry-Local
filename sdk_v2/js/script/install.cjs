// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Adapted from onnxruntime\js\node\script\install-utils.js
// The file in packages/ are the original source of truth that we are downloading and "installing" into our project's source tree.
// The file in node_modules/... is a symlink created by NPM to mark them as dependencies of the overall package.

'use strict';

const fs = require('fs');
const path = require('path');
const os = require('os');
const https = require('https');
const AdmZip = require('adm-zip');

// Determine platform
const PLATFORM_MAP = {
  'win32-x64': 'win-x64',
  'win32-arm64': 'win-arm64',
  'linux-x64': 'linux-x64',
  'darwin-arm64': 'osx-arm64',
};
const platformKey = `${os.platform()}-${os.arch()}`;
const RID = PLATFORM_MAP[platformKey];

if (!RID) {
  console.warn(`[foundry-local] Unsupported platform: ${platformKey}. Skipping native library installation.`);
  process.exit(0);
}

// Write to the source 'packages' directory so binaries persist and link correctly via package.json
const BIN_DIR = path.join(__dirname, '..', 'packages', '@foundry-local-core', platformKey);
const REQUIRED_FILES = [
  'Microsoft.AI.Foundry.Local.Core.dll',
  'onnxruntime.dll',
  'onnxruntime-genai.dll',
].map(f => f.replace('.dll', os.platform() === 'win32' ? '.dll' : os.platform() === 'darwin' ? '.dylib' : '.so'));

// When you run npm install --winml, npm does not pass --winml as a command-line argument to your script. 
// Instead, it sets an environment variable named npm_config_winml to 'true'.
const useWinML = process.env.npm_config_winml === 'true';
const useNightly = process.env.npm_config_nightly === 'true';

console.log(`[foundry-local] WinML enabled: ${useWinML}`);
console.log(`[foundry-local] Nightly enabled: ${useNightly}`);

const NUGET_FEED = 'https://api.nuget.org/v3/index.json';
const ORT_FEED = 'https://pkgs.dev.azure.com/aiinfra/PublicPackages/_packaging/ORT/nuget/v3/index.json';
const ORT_NIGHTLY_FEED = 'https://pkgs.dev.azure.com/aiinfra/PublicPackages/_packaging/ORT-Nightly/nuget/v3/index.json';

// If nightly is requested, pull Core/GenAI from the ORT-Nightly feed where nightly builds are published.
// Otherwise use the standard NuGet.org feed.
const CORE_FEED = useNightly ? ORT_NIGHTLY_FEED : NUGET_FEED;

const ARTIFACTS = [
  { 
    name: useWinML ? 'Microsoft.AI.Foundry.Local.Core.WinML' : 'Microsoft.AI.Foundry.Local.Core', 
    version: useNightly ? undefined : '0.8.2.2', // Set later using resolveLatestVersion if undefined
    files: ['Microsoft.AI.Foundry.Local.Core'],
    feed: CORE_FEED
  },
  { 
    name: os.platform() === 'linux' ? 'Microsoft.ML.OnnxRuntime.Gpu.Linux' : 'Microsoft.ML.OnnxRuntime.Foundry', 
    version: '1.23.2', // Hardcoded stable version
    files: ['onnxruntime'],
    feed: os.platform() === 'linux' ? NUGET_FEED : ORT_FEED
  },
  { 
    name: useWinML ? 'Microsoft.ML.OnnxRuntimeGenAI.WinML' : 'Microsoft.ML.OnnxRuntimeGenAI.Foundry', 
    version: '0.12.0-dev-20260130-1069610-180ed1ac', // Hardcoded dev version (until we upload 0.12.0 to ORT or NuGet)
    files: ['onnxruntime-genai'],
    feed: ORT_NIGHTLY_FEED
  }
];

// Check if already installed
if (fs.existsSync(BIN_DIR) && REQUIRED_FILES.every(f => fs.existsSync(path.join(BIN_DIR, f)))) {
  if (useNightly) {
    console.log(`[foundry-local] Nightly requested. Forcing reinstall...`);
    fs.rmSync(BIN_DIR, { recursive: true, force: true });
  } else {
    console.log(`[foundry-local] Native libraries already installed.`);
    process.exit(0);
  }
}

console.log(`[foundry-local] Installing native libraries for ${RID}...`);
fs.mkdirSync(BIN_DIR, { recursive: true });

async function downloadWithRetryAndRedirects(url, destStream = null) {
    const maxRedirects = 5;
    let currentUrl = url;
    let redirects = 0;

    while (redirects < maxRedirects) {
        const response = await new Promise((resolve, reject) => {
            https.get(currentUrl, (res) => resolve(res))
                 .on('error', reject);
        });

        // When you request a file from api.nuget.org, it rarely serves the file directly. 
        // Instead, it usually responds with a 302 Found or 307 Temporary Redirect pointing to a Content Delivery Network (CDN) 
        // or a specific Storage Account where the actual file lives. Node.js treats a redirect as a completed request so we
        // need to explicitly handle it here.
        if (response.statusCode >= 300 && response.statusCode < 400 && response.headers.location) {
            currentUrl = response.headers.location;
            response.resume(); // Consume/discard response data to free up socket
            redirects++;
            console.log(`  Following redirect to ${new URL(currentUrl).host}...`);
            continue;
        }

        if (response.statusCode !== 200) {
            throw new Error(`Download failed with status ${response.statusCode}: ${currentUrl}`);
        }

        // destStream is null when the function is used to download JSON data (like NuGet feed index or package metadata) rather than a file
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
    const data = await downloadWithRetryAndRedirects(url);
    return JSON.parse(data);
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


// Map to cache service index resources
const serviceIndexCache = new Map();

async function getBaseAddress(feedUrl) {
  // 1. Get Service Index
  if (!serviceIndexCache.has(feedUrl)) {
    const index = await downloadJson(feedUrl);
    serviceIndexCache.set(feedUrl, index);
  }
  
  const serviceIndex = serviceIndexCache.get(feedUrl);
  
  // 2. Find PackageBaseAddress/3.0.0
  const resources = serviceIndex.resources || [];
  const baseAddressRes = resources.find(r => r['@type'] && r['@type'].startsWith('PackageBaseAddress/3.0.0'));
  
  if (!baseAddressRes) {
    throw new Error('Could not find PackageBaseAddress/3.0.0 in NuGet feed.');
  }

  const baseAddress = baseAddressRes['@id'];
  // Ensure trailing slash
  return baseAddress.endsWith('/') ? baseAddress : baseAddress + '/';
}

async function resolveLatestVersion(feedUrl, packageName) {
    const baseAddress = await getBaseAddress(feedUrl);
    const nameLower = packageName.toLowerCase();
    
    // Fetch version list: {baseAddress}/{lower_id}/index.json
    const versionsUrl = `${baseAddress}${nameLower}/index.json`;
    try {
        const versionData = await downloadJson(versionsUrl);
        const versions = versionData.versions || [];

        if (versions.length === 0) {
            throw new Error('No versions found');
        }

        // Sort descending to prioritize latest date-based versions (e.g. 0.9.0-dev.YYYYMMDD...)
        versions.sort((a, b) => b.localeCompare(a));

        const latestVersion = versions[0];
        console.log(`[foundry-local] Installing latest version of Foundry Local Core: ${latestVersion}`);
        return latestVersion;
    } catch (e) {
        throw new Error(`Failed to fetch versions for ${packageName} from ${versionsUrl}: ${e.message}`);
    }
}

async function resolvePackageRawUrl(feedUrl, packageName, version) {
  const properBase = await getBaseAddress(feedUrl);
  
  // 3. Construct .nupkg URL (lowercase is standard for V3)
  const nameLower = packageName.toLowerCase();
  const verLower = version.toLowerCase();
  
  return `${properBase}${nameLower}/${verLower}/${nameLower}.${verLower}.nupkg`;
}

async function installPackage(artifact, tempDir) {
    const pkgName = artifact.name;
    const feedUrl = artifact.feed;
    
    // Resolve version if not specified
    let pkgVer = artifact.version;
    if (!pkgVer) {
        console.log(`  Resolving latest version for ${pkgName}...`);
        pkgVer = await resolveLatestVersion(feedUrl, pkgName);
    }
    
    console.log(`  Resolving ${pkgName} ${pkgVer}...`);
    const downloadUrl = await resolvePackageRawUrl(feedUrl, pkgName, pkgVer);
    
    const nupkgPath = path.join(tempDir, `${pkgName}.${pkgVer}.nupkg`);
    
    console.log(`  Downloading ${downloadUrl}...`);
    await downloadFile(downloadUrl, nupkgPath);
    
    console.log(`  Extracting...`);
    const zip = new AdmZip(nupkgPath);
    const zipEntries = zip.getEntries();
    
    // Pattern: runtimes/{RID}/native/{file}.{ext}
    const ext = os.platform() === 'win32' ? '.dll' : os.platform() === 'darwin' ? '.dylib' : '.so';
    const targetPathPrefix = `runtimes/${RID}/native/`.toLowerCase();
    
    let found = false;
    for (const fileBase of artifact.files) {
        const fileName = `${fileBase}${ext}`;
        // Look for entry ending with fileName and containing runtimes/RID/native/
        const entry = zipEntries.find(e => {
            const entryPathLower = e.entryName.toLowerCase();
            return entryPathLower.includes(targetPathPrefix) && entryPathLower.endsWith(fileName.toLowerCase());
        });
        
        if (entry) {
            console.log(`    Found ${entry.entryName}`);
            zip.extractEntryTo(entry, BIN_DIR, false, true);
            console.log(`    Extracted via AdmZip to ${path.join(BIN_DIR, entry.name)}`);
            found = true;
        } else {
             console.warn(`    ⚠ File ${fileName} not found for RID ${RID} in package.`);
        }
    }
}

async function main() {
    const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'foundry-install-'));
    try {
        for (const artifact of ARTIFACTS) {
            await installPackage(artifact, tempDir);
        }
        console.log('[foundry-local] ✓ Installation complete.');
    } catch (e) {
        console.error(`[foundry-local] Installation failed: ${e.message}`);
        process.exit(1);
    } finally {
        try {
            fs.rmSync(tempDir, { recursive: true, force: true });
        } catch {}
    }
}

main();