import { describe, it, before, after } from 'mocha';
import { expect } from 'chai';
import { getTestManager, TEST_MODEL_ALIAS, IS_RUNNING_IN_CI } from '../testUtils.js';
import { ResponsesClient, ResponsesClientSettings, getOutputText } from '../../src/openai/responsesClient.js';
import type {
    StreamingEvent,
    FunctionToolDefinition,
    ResponseInputItem,
    ResponseObject,
    MessageItem,
} from '../../src/types.js';
import { FoundryLocalManager } from '../../src/foundryLocalManager.js';
import { Model } from '../../src/detail/model.js';

describe('ResponsesClient Tests', () => {

    // ========================================================================
    // Settings serialization
    // ========================================================================

    describe('ResponsesClientSettings', () => {
        it('should serialize only defined settings', () => {
            const settings = new ResponsesClientSettings();
            settings.temperature = 0.5;
            settings.maxOutputTokens = 200;

            const result = settings._serialize();

            expect(result.temperature).to.equal(0.5);
            expect(result.max_output_tokens).to.equal(200);
            expect(result.top_p).to.be.undefined;
            expect(result.frequency_penalty).to.be.undefined;
            expect(result.instructions).to.be.undefined;
        });

        it('should serialize all settings including instructions', () => {
            const settings = new ResponsesClientSettings();
            settings.instructions = 'You are a helpful assistant.';
            settings.temperature = 0.7;
            settings.topP = 0.9;
            settings.maxOutputTokens = 500;
            settings.frequencyPenalty = 0.1;
            settings.presencePenalty = 0.2;
            settings.toolChoice = 'auto';
            settings.truncation = 'auto';
            settings.parallelToolCalls = true;
            settings.store = true;
            settings.metadata = { key: 'value' };
            settings.seed = 42;

            const result = settings._serialize();

            expect(result.instructions).to.equal('You are a helpful assistant.');
            expect(result.temperature).to.equal(0.7);
            expect(result.top_p).to.equal(0.9);
            expect(result.max_output_tokens).to.equal(500);
            expect(result.frequency_penalty).to.equal(0.1);
            expect(result.presence_penalty).to.equal(0.2);
            expect(result.tool_choice).to.equal('auto');
            expect(result.truncation).to.equal('auto');
            expect(result.parallel_tool_calls).to.be.true;
            expect(result.store).to.be.true;
            expect(result.metadata).to.deep.equal({ key: 'value' });
            expect(result.seed).to.equal(42);
        });

        it('should return empty object when no settings defined', () => {
            const settings = new ResponsesClientSettings();
            const result = settings._serialize();
            expect(Object.keys(result).length).to.equal(0);
        });
    });

    // ========================================================================
    // getOutputText helper
    // ========================================================================

    describe('getOutputText', () => {
        it('should extract text from string content', () => {
            const response: ResponseObject = {
                id: 'resp_1', object: 'response', created_at: 0, status: 'completed',
                model: 'test', output: [
                    { type: 'message', role: 'assistant', content: 'Hello world' } as MessageItem,
                ],
                tools: [], tool_choice: 'auto', truncation: 'disabled',
                parallel_tool_calls: false, text: {}, top_p: 1, temperature: 1,
                presence_penalty: 0, frequency_penalty: 0, store: false,
            };
            expect(getOutputText(response)).to.equal('Hello world');
        });

        it('should extract text from content parts array', () => {
            const response: ResponseObject = {
                id: 'resp_2', object: 'response', created_at: 0, status: 'completed',
                model: 'test', output: [
                    {
                        type: 'message', role: 'assistant',
                        content: [
                            { type: 'output_text', text: 'Part 1' },
                            { type: 'output_text', text: ' Part 2' },
                        ],
                    } as MessageItem,
                ],
                tools: [], tool_choice: 'auto', truncation: 'disabled',
                parallel_tool_calls: false, text: {}, top_p: 1, temperature: 1,
                presence_penalty: 0, frequency_penalty: 0, store: false,
            };
            expect(getOutputText(response)).to.equal('Part 1 Part 2');
        });

        it('should return empty string when no assistant message', () => {
            const response: ResponseObject = {
                id: 'resp_3', object: 'response', created_at: 0, status: 'completed',
                model: 'test', output: [],
                tools: [], tool_choice: 'auto', truncation: 'disabled',
                parallel_tool_calls: false, text: {}, top_p: 1, temperature: 1,
                presence_penalty: 0, frequency_penalty: 0, store: false,
            };
            expect(getOutputText(response)).to.equal('');
        });

        it('should skip non-assistant messages', () => {
            const response: ResponseObject = {
                id: 'resp_4', object: 'response', created_at: 0, status: 'completed',
                model: 'test', output: [
                    { type: 'message', role: 'user', content: 'User msg' } as MessageItem,
                    { type: 'message', role: 'assistant', content: 'Assistant msg' } as MessageItem,
                ],
                tools: [], tool_choice: 'auto', truncation: 'disabled',
                parallel_tool_calls: false, text: {}, top_p: 1, temperature: 1,
                presence_penalty: 0, frequency_penalty: 0, store: false,
            };
            expect(getOutputText(response)).to.equal('Assistant msg');
        });

        it('should skip refusal content parts', () => {
            const response: ResponseObject = {
                id: 'resp_5', object: 'response', created_at: 0, status: 'completed',
                model: 'test', output: [
                    {
                        type: 'message', role: 'assistant',
                        content: [
                            { type: 'refusal', refusal: 'Cannot do that' },
                            { type: 'output_text', text: 'But here is something' },
                        ],
                    } as MessageItem,
                ],
                tools: [], tool_choice: 'auto', truncation: 'disabled',
                parallel_tool_calls: false, text: {}, top_p: 1, temperature: 1,
                presence_penalty: 0, frequency_penalty: 0, store: false,
            };
            expect(getOutputText(response)).to.equal('But here is something');
        });
    });

    // ========================================================================
    // Constructor validation
    // ========================================================================

    describe('constructor', () => {
        it('should create client with valid baseUrl', () => {
            const client = new ResponsesClient('http://localhost:5273');
            expect(client).to.be.instanceOf(ResponsesClient);
        });

        it('should create client with baseUrl and modelId', () => {
            const client = new ResponsesClient('http://localhost:5273', 'test-model');
            expect(client).to.be.instanceOf(ResponsesClient);
        });

        it('should strip trailing slash from baseUrl', () => {
            const client = new ResponsesClient('http://localhost:5273/');
            expect(client).to.be.instanceOf(ResponsesClient);
        });

        it('should throw for empty baseUrl', () => {
            expect(() => new ResponsesClient('')).to.throw('baseUrl must be a non-empty string.');
        });

        it('should throw for null baseUrl', () => {
            expect(() => new ResponsesClient(null as any)).to.throw('baseUrl must be a non-empty string.');
        });
    });

    // ========================================================================
    // Input validation
    // ========================================================================

    describe('input validation', () => {
        const client = new ResponsesClient('http://localhost:5273', 'test-model');

        it('should throw for null input', async () => {
            try {
                await client.create(null as any);
                expect.fail('Should have thrown');
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
                expect((error as Error).message).to.include('Input cannot be null or undefined');
            }
        });

        it('should throw for undefined input', async () => {
            try {
                await client.create(undefined as any);
                expect.fail('Should have thrown');
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
                expect((error as Error).message).to.include('Input cannot be null or undefined');
            }
        });

        it('should throw for empty string input', async () => {
            try {
                await client.create('   ');
                expect.fail('Should have thrown');
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
                expect((error as Error).message).to.include('Input string cannot be empty');
            }
        });

        it('should throw for empty array input', async () => {
            try {
                await client.create([]);
                expect.fail('Should have thrown');
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
                expect((error as Error).message).to.include('Input items array cannot be empty');
            }
        });

        it('should throw for input items without type', async () => {
            try {
                await client.create([{ role: 'user', content: 'hi' } as any]);
                expect.fail('Should have thrown');
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
                expect((error as Error).message).to.include('must have a "type" property');
            }
        });
    });

    // ========================================================================
    // Tool validation
    // ========================================================================

    describe('tool validation', () => {
        const client = new ResponsesClient('http://localhost:5273', 'test-model');

        it('should throw for non-array tools', async () => {
            try {
                await client.create('Hello', { tools: 'not-array' as any });
                expect.fail('Should have thrown');
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
                expect((error as Error).message).to.include('Tools must be an array');
            }
        });

        it('should throw for tools with invalid type', async () => {
            try {
                await client.create('Hello', {
                    tools: [{ type: 'invalid', name: 'test' } as any]
                });
                expect.fail('Should have thrown');
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
                expect((error as Error).message).to.include('type "function"');
            }
        });

        it('should throw for tools without name', async () => {
            try {
                await client.create('Hello', {
                    tools: [{ type: 'function' } as any]
                });
                expect.fail('Should have thrown');
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
                expect((error as Error).message).to.include('"name" property');
            }
        });
    });

    // ========================================================================
    // Streaming callback validation
    // ========================================================================

    describe('streaming callback validation', () => {
        const client = new ResponsesClient('http://localhost:5273', 'test-model');

        it('should throw for null callback', async () => {
            try {
                await client.createStreaming('Hello', null as any);
                expect.fail('Should have thrown');
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
                expect((error as Error).message).to.include('Callback must be a valid function');
            }
        });

        it('should throw for non-function callback', async () => {
            try {
                await client.createStreaming('Hello', 'not-a-function' as any);
                expect.fail('Should have thrown');
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
                expect((error as Error).message).to.include('Callback must be a valid function');
            }
        });
    });

    // ========================================================================
    // Model ID validation
    // ========================================================================

    describe('model validation', () => {
        it('should throw when no model specified anywhere', async () => {
            const client = new ResponsesClient('http://localhost:5273');
            try {
                await client.create('Hello');
                expect.fail('Should have thrown');
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
                expect((error as Error).message).to.include('Model must be specified');
            }
        });
    });

    // ========================================================================
    // ID parameter validation
    // ========================================================================

    describe('ID parameter validation', () => {
        const client = new ResponsesClient('http://localhost:5273', 'test-model');

        const methods: Array<[string, (c: ResponsesClient, id: string) => Promise<any>]> = [
            ['get', (c, id) => c.get(id)],
            ['delete', (c, id) => c.delete(id)],
            ['cancel', (c, id) => c.cancel(id)],
            ['getInputItems', (c, id) => c.getInputItems(id)],
        ];

        for (const [methodName, fn] of methods) {
            it(`should throw for empty responseId on ${methodName}`, async () => {
                try {
                    await fn(client, '');
                    expect.fail('Should have thrown');
                } catch (error) {
                    expect(error).to.be.instanceOf(Error);
                    expect((error as Error).message).to.include('responseId must be a non-empty string');
                }
            });
        }

        it('should throw for excessively long responseId', async () => {
            const longId = 'a'.repeat(1025);
            try {
                await client.get(longId);
                expect.fail('Should have thrown');
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
                expect((error as Error).message).to.include('exceeds maximum length');
            }
        });
    });

    // ========================================================================
    // Integration tests (require running web service + loaded model)
    // ========================================================================

    describe('Integration (requires model + web service)', function() {
        let manager: FoundryLocalManager;
        let model: Model;
        let client: ResponsesClient;
        let skipped = false;

        before(async function() {
            this.timeout(30000);
            if (IS_RUNNING_IN_CI) {
                skipped = true;
                this.skip();
                return;
            }

            manager = getTestManager();
            const catalog = manager.catalog;

            const cachedModels = await catalog.getCachedModels();
            const cachedVariant = cachedModels.find(m => m.alias === TEST_MODEL_ALIAS);
            if (!cachedVariant) {
                skipped = true;
                this.skip();
                return;
            }

            model = await catalog.getModel(TEST_MODEL_ALIAS);
            model.selectVariant(cachedVariant);
            await model.load();
            manager.startWebService();
            client = manager.createResponsesClient(cachedVariant.id);
            client.settings.temperature = 0.0;
            client.settings.maxOutputTokens = 100;
        });

        after(async function() {
            if (skipped) return;
            try { manager.stopWebService(); } catch { /* ignore */ }
            try { await model.unload(); } catch { /* ignore */ }
        });

        it('should create a non-streaming response', async function() {
            this.timeout(30000);

            const response = await client.create('What is 2 + 2? Answer with just the number.');

            expect(response).to.not.be.undefined;
            expect(response.id).to.be.a('string');
            expect(response.status).to.equal('completed');
            expect(response.output).to.be.an('array');
            expect(response.output.length).to.be.greaterThan(0);

            const text = getOutputText(response);
            expect(text.length).to.be.greaterThan(0);
            console.log(`Response: ${text}`);
        });

        it('should create a streaming response', async function() {
            this.timeout(30000);

            const events: StreamingEvent[] = [];
            let textAccumulated = '';

            await client.createStreaming(
                'What is 3 + 5? Answer with just the number.',
                (event) => {
                    events.push(event);
                    if (event.type === 'response.output_text.delta') {
                        textAccumulated += event.delta;
                    }
                }
            );

            expect(events.length).to.be.greaterThan(0);

            // Should have lifecycle events
            expect(events.find(e => e.type === 'response.created')).to.not.be.undefined;
            expect(events.find(e => e.type === 'response.completed')).to.not.be.undefined;

            // Should have text deltas
            expect(events.some(e => e.type === 'response.output_text.delta')).to.be.true;

            expect(textAccumulated.length).to.be.greaterThan(0);
            console.log(`Streamed text: ${textAccumulated}`);
        });

        it('should create response with input items array', async function() {
            this.timeout(30000);

            const input: ResponseInputItem[] = [
                {
                    type: 'message',
                    role: 'user',
                    content: 'What is 10 minus 3? Answer with just the number.',
                } as MessageItem,
            ];

            const response = await client.create(input);

            expect(response.status).to.equal('completed');
            expect(response.output.length).to.be.greaterThan(0);

            const text = getOutputText(response);
            expect(text.length).to.be.greaterThan(0);
            console.log(`Input items response: ${text}`);
        });

        it('should use instructions from settings', async function() {
            this.timeout(30000);

            const instrClient = manager.createResponsesClient(model.id);
            instrClient.settings.temperature = 0.0;
            instrClient.settings.maxOutputTokens = 100;
            instrClient.settings.instructions = 'Always respond in exactly one word.';

            const response = await instrClient.create('What color is the sky?');

            expect(response.status).to.equal('completed');
            const text = getOutputText(response);
            expect(text.length).to.be.greaterThan(0);
            console.log(`With instructions: ${text}`);
        });

        it('should get and delete a stored response', async function() {
            this.timeout(30000);

            client.settings.store = true;
            try {
                const createResult = await client.create('Say hello');
                expect(createResult.id).to.be.a('string');

                // Retrieve it
                const retrieved = await client.get(createResult.id);
                expect(retrieved.id).to.equal(createResult.id);
                expect(retrieved.status).to.equal('completed');

                // Get input items
                const inputItems = await client.getInputItems(createResult.id);
                expect(inputItems).to.not.be.undefined;
                expect(inputItems.data).to.be.an('array');

                // Delete it
                const deleted = await client.delete(createResult.id);
                expect(deleted.deleted).to.be.true;
            } finally {
                client.settings.store = undefined;
            }
        });

        it('should chain responses via previous_response_id', async function() {
            this.timeout(30000);

            client.settings.store = true;
            try {
                const first = await client.create('Remember: the secret word is "banana".');
                expect(first.id).to.be.a('string');

                const second = await client.create('What is the secret word?', {
                    previous_response_id: first.id,
                });
                expect(second.previous_response_id).to.equal(first.id);

                const text = getOutputText(second);
                console.log(`Chained response: ${text}`);
            } finally {
                client.settings.store = undefined;
            }
        });

        it('should create response with tool calling', async function() {
            this.timeout(30000);

            const tools: FunctionToolDefinition[] = [{
                type: 'function',
                name: 'get_weather',
                description: 'Get the current weather for a location.',
                parameters: {
                    type: 'object',
                    properties: {
                        location: { type: 'string', description: 'City name' }
                    },
                    required: ['location']
                }
            }];

            const response = await client.create(
                'What is the weather in Seattle?',
                { tools, tool_choice: 'required' }
            );

            expect(response).to.not.be.undefined;
            expect(response.output.length).to.be.greaterThan(0);

            const functionCall = response.output.find((o: any) => o.type === 'function_call');
            if (functionCall) {
                console.log(`Tool call: ${JSON.stringify(functionCall)}`);
                expect((functionCall as any).name).to.equal('get_weather');
            }
        });
    });
});