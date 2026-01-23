const fs = require('fs');
const path = require('path');
const os = require('os');

console.log('[foundry-local] Preinstall: creating platform package skeletons...');

const platformKey = `${os.platform()}-${os.arch()}`;

const packagesRoot = path.join(__dirname, '..', 'packages', '@foundry-local-core');

const dir = path.join(packagesRoot, platformKey);

if (!fs.existsSync(dir)) {
  fs.mkdirSync(dir, { recursive: true });
}

const pkgJsonPath = path.join(dir, 'package.json');
if (!fs.existsSync(pkgJsonPath)) {
  const pkgContent = {
    name: `@foundry-local-core/${platformKey}`,
    version: "0.8.2",
    description: `Native binaries for Foundry Local SDK (${platformKey})`,
    os: [os.platform()],
    cpu: [os.arch()],
    private: true
  };
  fs.writeFileSync(pkgJsonPath, JSON.stringify(pkgContent, null, 2));
  console.log(`  Created skeleton for ${platformKey}`);
}

console.log('[foundry-local] Preinstall complete.');
