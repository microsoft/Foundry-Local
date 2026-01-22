// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

'use strict';

const fs = require('fs');
const path = require('path');
const os = require('os');
const { execSync } = require('child_process');

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

const BIN_DIR = path.join(__dirname, '..', 'node_modules', 'Microsoft.AI.Foundry.Local');
const REQUIRED_FILES = [
  'Microsoft.AI.Foundry.Local.Core.dll',
  'onnxruntime.dll',
  'onnxruntime-genai.dll',
].map(f => f.replace('.dll', os.platform() === 'win32' ? '.dll' : os.platform() === 'darwin' ? '.dylib' : '.so'));

// When you run npm install --winml, npm does not pass --winml as a command-line argument to your script. 
// Instead, it sets an environment variable named npm_config_winml to 'true'.
const useWinML = process.env.npm_config_winml === 'true';

console.log(`[foundry-local] WinML enabled: ${useWinML}`);

// NuGet package definitions
const MAIN_PACKAGE = {
  name: useWinML ? 'Microsoft.AI.Foundry.Local.Core.WinML' : 'Microsoft.AI.Foundry.Local.Core',
  version: '0.8.2.2',
  feed: 'https://pkgs.dev.azure.com/microsoft/windows.ai.toolkit/_packaging/Neutron/nuget/v3/index.json'
};

// Check if already installed
if (fs.existsSync(BIN_DIR) && REQUIRED_FILES.every(f => fs.existsSync(path.join(BIN_DIR, f)))) {
  console.log(`[foundry-local] Native libraries already installed.`);
  process.exit(0);
}

console.log(`[foundry-local] Installing native libraries for ${RID}...`);

// Ensure bin directory exists
fs.mkdirSync(BIN_DIR, { recursive: true });

const ARTIFACTS = [
  { name: MAIN_PACKAGE.name, files: ['Microsoft.AI.Foundry.Local.Core'] },
  { name: 'Microsoft.ML.OnnxRuntime.Foundry', files: ['onnxruntime'] },
  { name: useWinML ? 'Microsoft.ML.OnnxRuntimeGenAI.WinML' : 'Microsoft.ML.OnnxRuntimeGenAI.Foundry', files: ['onnxruntime-genai'] },
];

// Download and extract using NuGet CLI
function installPackages() {
  const tempDir = path.join(__dirname, '..', 'node_modules', 'FoundryLocalCorePath', 'temp');

  // Clean temp dir
  if (fs.existsSync(tempDir)) {
    fs.rmSync(tempDir, { recursive: true, force: true });
  }
  fs.mkdirSync(tempDir, { recursive: true });

  console.log(`  Installing ${MAIN_PACKAGE.name} version ${MAIN_PACKAGE.version}...`);
  
  try {
    // We install only the main package, dependencies are automatically installed
    const cmd = `C:\\foundry-local\\nuget.exe install ${MAIN_PACKAGE.name} -Version ${MAIN_PACKAGE.version} -Source "${MAIN_PACKAGE.feed}" -OutputDirectory "${tempDir}" -Prerelease -NonInteractive`;
    execSync(cmd, { stdio: 'inherit' });

    // Copy files from installed packages
    for (const artifact of ARTIFACTS) {
      findAndCopyArtifact(tempDir, artifact);
    }
    
    // Cleanup
    try {
      fs.rmSync(tempDir, { recursive: true, force: true });
    } catch (e) {
      console.warn(`  ⚠ Warning: Failed to clean up temporary files: ${e.message}`);
    }

  } catch (err) {
    throw new Error(`Failed to install packages: ${err.message}`);
  }
}

function findAndCopyArtifact(tempDir, artifact) {
  // Find directory starting with package name (e.g. Microsoft.AI.Foundry.Local.Core.0.8.2.2)
  const entries = fs.readdirSync(tempDir);
  // Sort to get latest version if multiple (though we expect one)
  const pkgDirName = entries
    .filter(e => e.toLowerCase().startsWith(artifact.name.toLowerCase()) && fs.statSync(path.join(tempDir, e)).isDirectory())
    .sort().pop();

  if (!pkgDirName) {
    console.warn(`  ⚠ Package folder not found for ${artifact.name}`);
    return;
  }

  const installedDir = path.join(tempDir, pkgDirName);
  const ext = os.platform() === 'win32' ? '.dll' : os.platform() === 'darwin' ? '.dylib' : '.so';
  const nativeDir = path.join(installedDir, 'runtimes', RID, 'native');
  
  for (const fileBase of artifact.files) {
    const srcPath = path.join(nativeDir, `${fileBase}${ext}`);
    const destPath = path.join(BIN_DIR, `${fileBase}${ext}`);
    
    if (fs.existsSync(srcPath)) {
      fs.copyFileSync(srcPath, destPath);
      console.log(`  ✓ Installed ${fileBase}${ext} from ${pkgDirName}`);
    } else {
      console.warn(`  ⚠ File not found: ${srcPath}`);
    }
  }
}

// Install all packages
try {
  installPackages();
  console.log('[foundry-local] ✓ Installation complete.');
} catch (err) {
  console.error('[foundry-local] Installation failed:', err.message);
  process.exit(1);
}