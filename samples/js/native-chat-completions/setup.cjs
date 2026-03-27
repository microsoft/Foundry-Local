// Cross-platform dependency installer
// Automatically uses --winml on Windows for hardware-accelerated inference
const { execSync } = require('child_process');

const isWindows = process.platform === 'win32';
const cmd = isWindows
  ? 'npm install --foreground-scripts --winml'
  : 'npm install --foreground-scripts';

console.log(`Running: ${cmd}`);
execSync(cmd, { stdio: 'inherit' });
