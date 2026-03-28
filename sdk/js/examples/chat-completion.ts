// -------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// -------------------------------------------------------------------------

import { FoundryLocalManager } from '../src/index.js';

async function main() {
    try {
        // Initialize the Foundry Local SDK
        console.log('Initializing Foundry Local SDK...');
        
        // NOTE: You must update libraryPath to point to your built DLL if not in standard location
        const manager = FoundryLocalManager.create({
            appName: 'FoundryLocalExample',
            serviceEndpoint: 'http://localhost:5000',
            logLevel: 'info'
        });
        console.log('✓ SDK initialized successfully');

        const availableEps = manager.discoverEps();
        console.log(`\nAvailable execution providers: ${availableEps.map((ep) => ep.name).join(', ')}`);

        console.log('\nDownloading and registering execution providers...');
        const downloadResult = manager.downloadAndRegisterEps();
        if (downloadResult.success) {
            console.log('✓ All execution providers registered successfully');
        } else {
            console.log(`⚠️ Some execution providers failed to download and/or register: ${downloadResult.failedEps.join(', ')}`);
        }

        // Explore available models
        console.log('\nFetching available models...');
        const catalog = manager.catalog;
        const models = await catalog.getModels();
        
        console.log(`Found ${models.length} models:`);
        for (const model of models) {
            const variants = model.variants.map((v: any) => v.id).join(', ');
            console.log(`  - ${model.alias} (variants: ${variants})`);
        }

        // Explore cached models
        console.log('\nFetching cached models...');
        const cachedModels = await catalog.getCachedModels();
        console.log(`Found ${cachedModels.length} cached models:`);
        for (const cachedModel of cachedModels) {
            console.log(`  - ${cachedModel.alias}`);
        }

        const modelAlias = 'qwen2.5-0.5b';
        
        // Load the model first
        console.log(`\nLoading model ${modelAlias}...`);
        const modelToLoad = await catalog.getModel(modelAlias);
        if (!modelToLoad) {
            throw new Error(`Model ${modelAlias} not found`);
        }

        await modelToLoad.load();
        console.log('✓ Model loaded');

        // Create chat client
        console.log('\nCreating chat client...');
        const chatClient = modelToLoad.createChatClient();
        
        // Configure chat settings
        chatClient.settings.temperature = 0.7;
        chatClient.settings.maxTokens = 800;
        
        console.log('✓ Chat client created');

        // Example chat completion
        console.log('\nTesting chat completion...');
        const completion = await chatClient.completeChat([
            { role: 'user', content: 'What is the capital of France?' }
        ]);

        console.log('\nChat completion result:');
        console.log(completion.choices[0]?.message?.content);

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

        // Model management example
        const model = await catalog.getModel(modelAlias);
        if (model) {
            console.log('\nModel management example:');
            // Already loaded above, but let's check status
            console.log(`Checking model: ${model.id}`);
            console.log(`✓ Model loaded: ${await model.isLoaded()}`);
            
            // Unload when done
            console.log('Unloading model...');
            await model.unload();
            console.log(`✓ Model loaded: ${await model.isLoaded()}`);
        }

        console.log('\n✓ Example completed successfully');

    } catch (error) {
        console.log('Error running example:', error);
        // Print stack trace if available
        if (error instanceof Error && error.stack) {
            console.log(error.stack);
        }
        process.exit(1);
    }
}

// Run the example
main().catch(console.error);

export { main };