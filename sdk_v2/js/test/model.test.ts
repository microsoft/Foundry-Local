import { describe, it } from 'mocha';
import { expect } from 'chai';
import { getTestManager, TEST_MODEL_ALIAS } from './testUtils.js';

describe('Model Tests', () => {
    it('should verify cached models from test-data-shared', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        const cachedModels = await catalog.getCachedModels();
        
        expect(cachedModels).to.be.an('array');
        expect(cachedModels.length).to.be.greaterThan(0);
        
        // Check for qwen model
        const qwenModel = cachedModels.find(m => m.alias === 'qwen2.5-0.5b');
        expect(qwenModel, 'qwen2.5-0.5b should be cached').to.not.be.undefined;
        expect(qwenModel?.isCached).to.be.true;
        
        // Check for whisper model
        const whisperModel = cachedModels.find(m => m.alias === 'whisper-tiny');
        expect(whisperModel, 'whisper-tiny should be cached').to.not.be.undefined;
        expect(whisperModel?.isCached).to.be.true;
    });

    it('should load and unload model', async function() {
        this.timeout(10000);
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        // Ensure cache is populated first
        const cachedModels = await catalog.getCachedModels();
        expect(cachedModels.length).to.be.greaterThan(0);

        const cachedVariant = cachedModels.find(m => m.alias === TEST_MODEL_ALIAS);
        expect(cachedVariant).to.not.be.undefined;

        const model = await catalog.getModel(TEST_MODEL_ALIAS);
        
        expect(model).to.not.be.undefined;
        if (!model || !cachedVariant) return;

        model.selectVariant(cachedVariant.id);

        // Ensure it's not loaded initially (or unload if it is)
        if (await model.isLoaded()) {
            await model.unload();
        }
        expect(await model.isLoaded()).to.be.false;

        await model.load();
        expect(await model.isLoaded()).to.be.true;

        await model.unload();
        expect(await model.isLoaded()).to.be.false;
    });

    it('should perform chat completion', async function() {
        this.timeout(10000);
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        // Ensure cache is populated first
        const cachedModels = await catalog.getCachedModels();
        expect(cachedModels.length).to.be.greaterThan(0);

        const cachedVariant = cachedModels.find(m => m.alias === TEST_MODEL_ALIAS);
        expect(cachedVariant).to.not.be.undefined;

        const model = await catalog.getModel(TEST_MODEL_ALIAS);
        
        expect(model).to.not.be.undefined;
        if (!model || !cachedVariant) return;

        model.selectVariant(cachedVariant.id);

        await model.load();
        
        try {
            const client = model.createChatClient();

            client.settings.maxTokens = 500;
            client.settings.temperature = 0.0; // for deterministic results

            const result = await client.completeChat([
                { role: 'user', content: 'You are a calculator. Be precise. What is the answer to 7 multiplied by 6?' }
            ]);
            
            expect(result).to.not.be.undefined;
            expect(result.choices).to.be.an('array');
            expect(result.choices.length).to.be.greaterThan(0);
            expect(result.choices[0].message).to.not.be.undefined;
            expect(result.choices[0].message.content).to.be.a('string');
            console.log(`Response: ${result.choices[0].message.content}`);
            expect(result.choices[0].message.content).to.include('42');
        } finally {
            await model.unload();
        }
    });

    it('should perform streaming chat completion', async function() {
        this.timeout(10000);
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        // Ensure cache is populated first
        const cachedModels = await catalog.getCachedModels();
        expect(cachedModels.length).to.be.greaterThan(0);

        const cachedVariant = cachedModels.find(m => m.alias === TEST_MODEL_ALIAS);
        expect(cachedVariant).to.not.be.undefined;

        const model = await catalog.getModel(TEST_MODEL_ALIAS);
        
        expect(model).to.not.be.undefined;
        if (!model || !cachedVariant) return;

        model.selectVariant(cachedVariant.id);

        await model.load();
        
        try {
            const client = model.createChatClient();

            client.settings.maxTokens = 500;
            client.settings.temperature = 0.0; // for deterministic results

            const messages: Array<{ role: string; content: string }> = [
                { role: 'user', content: 'You are a calculator. Be precise. What is the answer to 7 multiplied by 6?' }
            ];

            // First question
            let fullContent = '';
            let chunkCount = 0;

            await client.completeStreamingChat(messages, (chunk) => {
                chunkCount++;
                const content = chunk.choices?.[0]?.delta?.content;
                if (content) {
                    fullContent += content;
                }
            });
            
            expect(chunkCount).to.be.greaterThan(0);
            expect(fullContent).to.be.a('string');
            console.log(`First response: ${fullContent}`);
            expect(fullContent).to.include('42');

            // Add assistant's response and ask follow-up question
            messages.push({ role: 'assistant', content: fullContent });
            messages.push({ role: 'user', content: 'Add 25 to the previous answer. Think hard to be sure of the answer.' });

            // Second question
            fullContent = '';
            chunkCount = 0;

            await client.completeStreamingChat(messages, (chunk) => {
                chunkCount++;
                const content = chunk.choices?.[0]?.delta?.content;
                if (content) {
                    fullContent += content;
                }
            });

            expect(chunkCount).to.be.greaterThan(0);
            expect(fullContent).to.be.a('string');
            console.log(`Second response: ${fullContent}`);
            expect(fullContent).to.include('67');
        } finally {
            await model.unload();
        }
    });
});
