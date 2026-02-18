import { describe, it } from 'mocha';
import { expect } from 'chai';
import { getTestManager, TEST_MODEL_ALIAS } from '../testUtils.js';

describe('Chat Client Tests', () => {
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
        if (!cachedVariant) return;

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
        if (!cachedVariant) return;

        model.selectVariant(cachedVariant.id);

        await model.load();
        
        try {
            const client = model.createChatClient();

            client.settings.maxTokens = 500;
            client.settings.temperature = 0.0; // for deterministic results

            const messages = [
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

    it('should throw when completing chat with empty messages array', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        const model = await catalog.getModel(TEST_MODEL_ALIAS);

        const client = model.createChatClient();
        
        try {
            await client.completeChat([]);
            expect.fail('Should have thrown an error for empty messages array');
        } catch (error) {
            expect(error).to.be.instanceOf(Error);
            expect((error as Error).message).to.include('Messages array cannot be null, undefined, or empty.');
        }
    });

    it('should throw when completing chat with invalid message', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        const model = await catalog.getModel(TEST_MODEL_ALIAS);

        const client = model.createChatClient();
        
        try {
            await client.completeChat([{ role: 'user' } as any]);
            expect.fail('Should have thrown an error for message without content');
        } catch (error) {
            expect(error).to.be.instanceOf(Error);
            expect((error as Error).message).to.include('Each message must have a "content" property that is a non-empty string.');
        }

        try {
            await client.completeChat([{ content: 'hello' } as any]);
            expect.fail('Should have thrown an error for message without role');
        } catch (error) {
            expect(error).to.be.instanceOf(Error);
            expect((error as Error).message).to.include('Each message must have a "role" property that is a non-empty string.');
        }
    });

    it('should throw when completing streaming chat with empty messages array', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        const model = await catalog.getModel(TEST_MODEL_ALIAS);

        const client = model.createChatClient();
        
        try {
            await client.completeStreamingChat([], () => {});
            expect.fail('Should have thrown an error for empty messages array');
        } catch (error) {
            expect(error).to.be.instanceOf(Error);
            expect((error as Error).message).to.include('Messages array cannot be null, undefined, or empty.');
        }
    });

    it('should throw when completing streaming chat with invalid callback', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        const model = await catalog.getModel(TEST_MODEL_ALIAS);
        const client = model.createChatClient();
        const messages = [{ role: 'user', content: 'Hello' }];
        const invalidCallbacks: any[] = [null, undefined, {} as any, 'not a function' as any];
        for (const invalidCallback of invalidCallbacks) {
            try {
                await client.completeStreamingChat(messages as any, invalidCallback as any);
                expect.fail('Should have thrown an error for invalid callback');
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
            }
        }
    });
});
