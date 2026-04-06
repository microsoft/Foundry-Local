// -------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// -------------------------------------------------------------------------

/**
 * Example: Cancelling a model download using Foundry Local JS/TS SDK.
 *
 * Demonstrates how to cancel a running model download using the standard
 * `AbortController` / `AbortSignal` pattern.
 */

import { FoundryLocalManager } from '../src/index.js';

async function main() {
    try {
        // 1. Initialize the SDK
        console.log('Initializing Foundry Local SDK...');
        const manager = FoundryLocalManager.create({
            appName: 'CancellableDownloadExample',
            logLevel: 'info'
        });
        console.log('✓ SDK initialized');

        // Register execution providers
        const epResult = await manager.downloadAndRegisterEps();
        console.log(`EP registration success: ${epResult.success}`);

        // 2. Pick a model
        const modelAlias = 'phi-4-mini';
        const model = await manager.catalog.getModel(modelAlias);
        if (!model) {
            throw new Error(`Model ${modelAlias} not found in catalog`);
        }

        if (model.isCached) {
            console.log(`Model '${modelAlias}' is already cached. Remove it first to test cancellation.`);
            return;
        }

        // 3. Create an AbortController
        const controller = new AbortController();

        // 4. Start the download (returns a promise)
        console.log('Starting download...');
        const downloadPromise = model.download(
            (progress: number) => {
                process.stdout.write(`\rDownload: ${progress.toFixed(1)}%`);
            },
            controller.signal  // pass the AbortSignal
        );

        // 5. Cancel after 3 seconds
        console.log('Will cancel in 3 seconds...');
        setTimeout(() => {
            console.log('\nCancelling download...');
            controller.abort();
        }, 3000);

        // 6. Await the result
        await downloadPromise;
        console.log('\nDownload completed before cancellation took effect.');

    } catch (error) {
        if (error instanceof Error && error.message === 'Operation cancelled') {
            console.log(`\nDownload was cancelled: ${error.message}`);
        } else {
            console.error('Error:', error);
            process.exit(1);
        }
    }
}

main().catch(console.error);

export { main };
