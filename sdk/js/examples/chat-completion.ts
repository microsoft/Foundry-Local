// -------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// -------------------------------------------------------------------------

import { FoundryLocalManager } from '../src/index.js';
import path from 'path';

async function main() {
    let modelToLoad: any = null;

    try {
        // Initialize the Foundry Local SDK
        console.log('Initializing Foundry Local SDK...');
        
        const manager = FoundryLocalManager.create({
            appName: 'FoundryLocalAudioExample',
            logLevel: 'info'
        });
        console.log('✓ SDK initialized successfully');

        // Explore available models
        console.log('\nFetching available models...');
        const catalog = manager.catalog;
        const models = await catalog.getModels();
        
        console.log(`Found ${models.length} models:`);
        for (const model of models) {
            const variants = model.variants.map((v: any) => v.id).join(', ');
            console.log(`  - ${model.alias} (variants: ${variants})`);
        }

        const modelAlias = 'whisper-medium';
        
        // Get the Whisper model
        console.log(`\nLoading model ${modelAlias}...`);
        modelToLoad = await catalog.getModel(modelAlias);
        if (!modelToLoad) {
            throw new Error(`Model ${modelAlias} not found`);
        }

        // Download if not cached
        if (!modelToLoad.isCached) {
            console.log('Downloading model...');
            await modelToLoad.download((progress: number) => {
                process.stdout.write(`\rDownload: ${progress.toFixed(1)}%`);
            });
            console.log();
        }

        await modelToLoad.load();
        console.log('✓ Model loaded');

        // Create audio client
        console.log('\nCreating audio client...');
        const audioClient = modelToLoad.createAudioClient();
        
        // Configure settings
        audioClient.settings.language = 'en';
        // audioClient.settings.temperature = 0.0; // deterministic results
        
        console.log('✓ Audio client created');

        // Audio file path — update this to point to your audio file
        const audioFilePath = path.join(process.cwd(), '..', 'testdata', 'Recording.mp3');

        // Example: Standard transcription
        console.log('\nTesting standard transcription...');
        const result = await audioClient.transcribe(audioFilePath);
        console.log('\nTranscription result:');
        console.log(result.text);

        // Example streaming completion
        console.log('\nTesting streaming completion...');
        for await (const chunk of chatClient.completeStreamingChat(
            [{ role: 'user', content: 'Write a short poem about programming.' }]
        )) {
            const content = chunk.choices?.[0]?.message?.content;
            if (content) {
                process.stdout.write(content);
            }
        }
        console.log('\n');

        // Unload the model
        console.log('Unloading model...');
        await modelToLoad.unload();
        console.log(`✓ Model unloaded`);

        console.log('\n✓ Audio transcription example completed successfully');

    } catch (error) {
        console.log('Error running example:', error);
        if (error instanceof Error && error.stack) {
            console.log(error.stack);
        }
        // Best-effort cleanup
        if (modelToLoad) {
            try { await modelToLoad.unload(); } catch { /* ignore */ }
        }
        process.exit(1);
    }
}

// Run the example
main().catch(console.error);

export { main };
