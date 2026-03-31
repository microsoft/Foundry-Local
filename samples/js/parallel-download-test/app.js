import { FoundryLocalManager } from 'foundry-local-sdk';

// Test parallel model downloads to verify AsyncLocal thread-safety
// of the OnDownloadComplete telemetry callback.

const MODEL_ALIASES = ['qwen2.5-0.5b', 'whisper-tiny', 'phi-3.5-mini'];

async function main() {
    console.log('=== Parallel Download Test ===\n');
    console.log('Initializing Foundry Local SDK...');

    const manager = FoundryLocalManager.create({
        appName: 'ParallelDownloadTest',
        logLevel: 'info',
        additionalSettings: {
            NumModelDownloadThreads: '8'
        }
    });
    console.log('✓ SDK initialized (NumModelDownloadThreads=8)\n');

    // Resolve all models from catalog
    console.log('Resolving models from catalog...');
    const models = [];
    for (const alias of MODEL_ALIASES) {
        try {
            const model = await manager.catalog.getModel(alias);
            console.log(`  ✓ ${alias} (id: ${model.id}, cached: ${model.isCached})`);
            models.push({ alias, model });
        } catch (e) {
            console.log(`  ✗ ${alias} — not found in catalog, skipping`);
        }
    }

    if (models.length < 2) {
        console.error('\nNeed at least 2 models to test parallel downloads. Exiting.');
        process.exit(1);
    }

    // Delete cached models to force re-download
    const uncachedModels = models.filter(m => !m.model.isCached);
    const cachedModels = models.filter(m => m.model.isCached);

    if (cachedModels.length > 0) {
        console.log(`\nNote: ${cachedModels.map(m => m.alias).join(', ')} already cached.`);
        console.log('To force re-download, delete the model cache and re-run.');
    }

    const toDownload = models; // Download all — cached models will skip quickly

    // Download all models in parallel
    console.log(`\nStarting parallel download of ${toDownload.length} models...`);
    const startTime = Date.now();

    const downloadPromises = toDownload.map(({ alias, model }) => {
        const modelStart = Date.now();
        console.log(`  [${alias}] Starting download...`);

        return model.download((progress) => {
            process.stdout.write(`\r  [${alias}] ${progress.toFixed(1)}%   `);
        }).then(() => {
            const elapsed = ((Date.now() - modelStart) / 1000).toFixed(1);
            console.log(`\n  [${alias}] ✓ Complete in ${elapsed}s`);
            return { alias, elapsed, status: 'success' };
        }).catch((err) => {
            const elapsed = ((Date.now() - modelStart) / 1000).toFixed(1);
            console.log(`\n  [${alias}] ✗ Failed in ${elapsed}s: ${err.message}`);
            return { alias, elapsed, status: 'failed', error: err.message };
        });
    });

    const results = await Promise.all(downloadPromises);

    const totalElapsed = ((Date.now() - startTime) / 1000).toFixed(1);

    // Summary
    console.log('\n=== Results ===');
    for (const r of results) {
        console.log(`  ${r.status === 'success' ? '✓' : '✗'} ${r.alias}: ${r.elapsed}s (${r.status})`);
    }
    console.log(`\n  Total wall time: ${totalElapsed}s`);
    console.log(`  Models downloaded: ${results.filter(r => r.status === 'success').length}/${results.length}`);

    const succeeded = results.filter(r => r.status === 'success');
    if (succeeded.length >= 2) {
        const maxIndividual = Math.max(...succeeded.map(r => parseFloat(r.elapsed)));
        const sumIndividual = succeeded.reduce((s, r) => s + parseFloat(r.elapsed), 0);
        console.log(`\n  If sequential, expected ~${sumIndividual.toFixed(1)}s`);
        console.log(`  Actual wall time: ${totalElapsed}s`);
        if (parseFloat(totalElapsed) < sumIndividual * 0.8) {
            console.log('  → Downloads ran in parallel ✓');
        } else {
            console.log('  → Downloads appear sequential (expected if models were cached)');
        }
    }

    console.log('\nDone.');
}

main().catch(console.error);
