const fs = require('fs');
const path = require('path');
const os = require('os');

console.log('[foundry-local] Preinstall: creating platform package skeletons...');

// All platforms referenced by optionalDependencies in package.json.
// Skeletons must exist for every entry so that npm can resolve the
// file: dependencies without crashing during tree resolution.
const ALL_PLATFORMS = [
  { key: 'darwin-arm64', os: 'darwin', cpu: 'arm64' },
  { key: 'linux-x64',   os: 'linux',  cpu: 'x64' },
  { key: 'win32-arm64',  os: 'win32',  cpu: 'arm64' },
  { key: 'win32-x64',    os: 'win32',  cpu: 'x64' },
];

const packagesRoot = path.join(__dirname, '..', 'packages', '@foundry-local-core');

for (const platform of ALL_PLATFORMS) {
  const dir = path.join(packagesRoot, platform.key);

  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }

  const pkgJsonPath = path.join(dir, 'package.json');
  if (!fs.existsSync(pkgJsonPath)) {
    const pkgContent = {
      name: `@foundry-local-core/${platform.key}`,
      version: "0.0.0", // Placeholder version, will be replaced during install.cjs
      description: `Native binaries for Foundry Local SDK (${platform.key})`,
      os: [platform.os],
      cpu: [platform.cpu],
      private: true
    };
    fs.writeFileSync(pkgJsonPath, JSON.stringify(pkgContent, null, 2));
    console.log(`  Created skeleton for ${platform.key}`);
  }
}

console.log('[foundry-local] Preinstall complete.');
