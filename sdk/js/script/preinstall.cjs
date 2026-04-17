const fs = require('fs');
const path = require('path');

console.log('[foundry-local] Preinstall: creating platform package skeletons...');

// Derive all platform packages from optionalDependencies in package.json
// so this script stays in sync automatically.
const rootPackageJsonPath = path.join(__dirname, '..', 'package.json');
const rootPackageJson = JSON.parse(fs.readFileSync(rootPackageJsonPath, 'utf8'));
const optionalDependencies = rootPackageJson.optionalDependencies || {};
const platformPackagePrefix = '@foundry-local-core/';

const ALL_PLATFORMS = Object.keys(optionalDependencies)
  .filter((packageName) => packageName.startsWith(platformPackagePrefix))
  .map((packageName) => {
    const key = packageName.slice(platformPackagePrefix.length);
    const parts = key.split('-');
    const cpu = parts[parts.length - 1];
    const platformOs = parts.slice(0, -1).join('-');

    return {
      key,
      os: platformOs,
      cpu,
    };
  });

const packagesRoot = path.join(__dirname, '..', 'node_modules', '@foundry-local-core');

for (const platform of ALL_PLATFORMS) {
  const dir = path.join(packagesRoot, platform.key);

  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }

  const pkgJsonPath = path.join(dir, 'package.json');
  const pkgContent = {
    name: `@foundry-local-core/${platform.key}`,
    version: "0.0.0",
    description: `Native binaries for Foundry Local SDK (${platform.key})`,
    os: [platform.os],
    cpu: [platform.cpu],
    private: true
  };
  fs.writeFileSync(pkgJsonPath, JSON.stringify(pkgContent, null, 2));
  console.log(`  Created skeleton for ${platform.key}`);
}

console.log('[foundry-local] Preinstall complete.');
