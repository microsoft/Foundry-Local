import { FoundryLocalManager } from 'foundry-local-sdk';

// Test multiple processes downloading the same model concurrently.
// Run multiple instances of this script simultaneously to test cross-process locking.

const MODEL_ALIAS = process.argv[2] || 'qwen2.5-0.5b';
const PROCESS_ID = process.argv[3] || process.pid;

async function main() {
    const startTime = Date.now();
    console.log(`[P${PROCESS_ID}] Starting — model: ${MODEL_ALIAS}`);

    const manager = FoundryLocalManager.create({
        appName: 'ParallelDownloadTest',
        logLevel: 'warning',
        additionalSettings: {
            NumModelDownloadThreads: '8'
        }
    });

    const model = await manager.catalog.getModel(MODEL_ALIAS);
    console.log(`[P${PROCESS_ID}] Model resolved (cached: ${model.isCached})`);

    console.log(`[P${PROCESS_ID}] Starting download...`);
    const dlStart = Date.now();

    await model.download((progress) => {
        process.stdout.write(`\r[P${PROCESS_ID}] ${progress.toFixed(1)}%   `);
    });

    const dlElapsed = ((Date.now() - dlStart) / 1000).toFixed(1);
    const totalElapsed = ((Date.now() - startTime) / 1000).toFixed(1);
    console.log(`\n[P${PROCESS_ID}] ✓ Download complete in ${dlElapsed}s (total: ${totalElapsed}s)`);
}

main().catch(err => {
    console.error(`\n[P${PROCESS_ID}] ✗ Failed: ${err.message}`);
    process.exit(1);
});
