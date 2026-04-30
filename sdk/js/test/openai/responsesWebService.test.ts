import { describe, it, before, after } from 'mocha';
import { expect } from 'chai';
import { getTestManager, TEST_MODEL_ALIAS, IS_RUNNING_IN_CI } from '../testUtils.js';
import { FoundryLocalManager } from '../../src/foundryLocalManager.js';
import type { IModel } from '../../src/imodel.js';

function getOutputText(response: any): string {
    if (typeof response.output_text === 'string') {
        return response.output_text;
    }

    return (response.output ?? [])
        .flatMap((item: any) => Array.isArray(item.content) ? item.content : [])
        .filter((part: any) => part.type === 'output_text' && typeof part.text === 'string')
        .map((part: any) => part.text)
        .join('');
}

async function postResponse(baseUrl: string, body: Record<string, unknown>): Promise<any> {
    const res = await fetch(`${baseUrl}/v1/responses`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
    });

    const text = await res.text();
    expect(res.ok, text).to.equal(true);
    return JSON.parse(text);
}

async function postStreamingResponse(baseUrl: string, body: Record<string, unknown>): Promise<any[]> {
    const res = await fetch(`${baseUrl}/v1/responses`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Accept': 'text/event-stream',
        },
        body: JSON.stringify({ ...body, stream: true }),
    });

    if (!res.ok) {
        const errorText = await res.text().catch(() => res.statusText);
        expect.fail(errorText);
    }
    expect(res.body).to.not.equal(null);

    const reader = res.body!.getReader();
    const decoder = new TextDecoder();
    const events: any[] = [];
    let buffer = '';

    try {
        while (true) {
            const { value, done } = await reader.read();
            if (done) break;

            buffer += decoder.decode(value, { stream: true });
            const blocks = buffer.split('\n\n');
            buffer = blocks.pop() ?? '';

            for (const block of blocks) {
                const data = block
                    .split('\n')
                    .filter((line) => line.startsWith('data: '))
                    .map((line) => line.slice(6))
                    .join('\n')
                    .trim();

                if (!data) continue;
                if (data === '[DONE]') return events;
                events.push(JSON.parse(data));
            }
        }
    } finally {
        reader.releaseLock();
    }

    return events;
}

describe('Responses web service Integration', function() {
    let manager: FoundryLocalManager;
    let model: IModel;
    let modelId: string;
    let baseUrl: string;
    let skipped = false;

    before(async function() {
        this.timeout(30000);
        if (IS_RUNNING_IN_CI) {
            skipped = true;
            this.skip();
            return;
        }

        manager = getTestManager();
        const cachedModels = await manager.catalog.getCachedModels();
        const cachedVariant = cachedModels.find((m) => m.alias === TEST_MODEL_ALIAS);
        if (!cachedVariant) {
            skipped = true;
            this.skip();
            return;
        }

        model = await manager.catalog.getModel(TEST_MODEL_ALIAS);
        model.selectVariant(cachedVariant);
        modelId = cachedVariant.id;

        await model.load();
        manager.startWebService();
        baseUrl = manager.urls[0];
    });

    after(async function() {
        if (skipped) return;
        try { manager.stopWebService(); } catch { /* ignore cleanup errors */ }
        try { await model.unload(); } catch { /* ignore cleanup errors */ }
    });

    it('should create a response through the OpenAI-compatible web service', async function() {
        this.timeout(30000);

        const response = await postResponse(baseUrl, {
            model: modelId,
            input: 'What is 2 + 2? Answer with just the number.',
            temperature: 0,
            max_output_tokens: 64,
            store: false,
        });

        expect(response.object).to.equal('response');
        expect(response.status).to.equal('completed');
        expect(getOutputText(response).length).to.be.greaterThan(0);
    });

    it('should stream response events through the OpenAI-compatible web service', async function() {
        this.timeout(30000);

        const events = await postStreamingResponse(baseUrl, {
            model: modelId,
            input: 'Count from 1 to 3.',
            temperature: 0,
            max_output_tokens: 64,
            store: false,
        });

        expect(events.some((event) => event.type === 'response.created')).to.equal(true);
        expect(events.some((event) => event.type === 'response.output_text.delta')).to.equal(true);
        expect(events.some((event) => event.type === 'response.completed')).to.equal(true);
    });

    it('should support Responses function calling through the web service', async function() {
        this.timeout(30000);
        if (model.supportsToolCalling === false) {
            this.skip();
            return;
        }

        const tools = [{
            type: 'function',
            name: 'get_weather',
            description: 'Get the current weather. This test always returns Seattle weather.',
            parameters: {
                type: 'object',
                properties: {},
                additionalProperties: false,
            },
        }];

        const toolResponse = await postResponse(baseUrl, {
            model: modelId,
            input: 'Use the get_weather tool and then answer with the weather.',
            tools,
            tool_choice: 'required',
            temperature: 0,
            max_output_tokens: 64,
            store: true,
        });

        const functionCall = toolResponse.output?.find((item: any) => item.type === 'function_call');
        expect(functionCall, JSON.stringify(toolResponse.output)).to.not.equal(undefined);
        expect(functionCall.name).to.equal('get_weather');
        expect(functionCall.call_id).to.be.a('string');

        const finalResponse = await postResponse(baseUrl, {
            model: modelId,
            previous_response_id: toolResponse.id,
            input: [{
                type: 'function_call_output',
                call_id: functionCall.call_id,
                output: JSON.stringify({ location: 'Seattle', weather: '72 degrees F and sunny' }),
            }],
            tools,
            temperature: 0,
            max_output_tokens: 64,
            store: false,
        });

        expect(finalResponse.status).to.equal('completed');
        expect(getOutputText(finalResponse).length).to.be.greaterThan(0);
    });
});
