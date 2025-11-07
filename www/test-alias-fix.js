function extractAlias(modelName) {
    const deviceSuffixes = ['-cuda-gpu', '-cuda', '-gpu', '-generic-cpu', '-cpu', '-npu', '-fpga', '-asic'];
    let alias = modelName.toLowerCase();
    for (const suffix of deviceSuffixes) {
        if (alias.endsWith(suffix)) {
            alias = alias.slice(0, -suffix.length);
            break;
        }
    }
    return alias;
}

function createDisplayName(alias) {
    return alias.split('-').map((word) => word.charAt(0).toUpperCase() + word.slice(1)).join(' ');
}

// OLD WAY (broken)
const fullName = 'gpt-oss-20b-cuda-gpu:1';
const aliasOld = extractAlias(fullName);
console.log('❌ OLD WAY (broken):');
console.log('  Full name:', fullName);
console.log('  Alias:', aliasOld);
console.log('  Display name:', createDisplayName(aliasOld));

// NEW WAY (fixed)
const baseName = fullName.split(':')[0];
const aliasNew = extractAlias(baseName);
console.log('\n✅ NEW WAY (fixed):');
console.log('  Full name:', fullName);
console.log('  Base name:', baseName);
console.log('  Alias:', aliasNew);
console.log('  Display name:', createDisplayName(aliasNew));
console.log('  Search for "gpt" matches:', createDisplayName(aliasNew).toLowerCase().includes('gpt'));
