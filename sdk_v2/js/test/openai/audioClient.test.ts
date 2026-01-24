import { describe, it } from 'mocha';
import { expect } from 'chai';
import { getTestManager } from '../testUtils.js';
import path from 'path';

describe('Audio Client Tests', () => {
    const WHISPER_MODEL_ALIAS = 'whisper-tiny';
    const AUDIO_FILE_PATH = path.join(process.cwd(), '..', 'testdata', 'Recording.mp3');
    const EXPECTED_TEXT = ' And lots of times you need to give people more than one link at a time. You a band could give their fans a couple new videos from the live concert behind the scenes photo gallery and album to purchase like these next few links.';

    it('should transcribe audio without streaming', async function() {
        this.timeout(30000);
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        const cachedModels = await catalog.getCachedModels();
        expect(cachedModels.length).to.be.greaterThan(0);

        const cachedVariant = cachedModels.find(m => m.alias === WHISPER_MODEL_ALIAS);
        expect(cachedVariant, 'whisper-tiny should be cached').to.not.be.undefined;

        const model = await catalog.getModel(WHISPER_MODEL_ALIAS);
        expect(model).to.not.be.undefined;
        if (!model || !cachedVariant) return;

        model.selectVariant(cachedVariant.id);
        await model.load();
        
        try {
            const audioClient = model.createAudioClient();
            expect(audioClient).to.not.be.undefined;

            const response = await audioClient.transcribe(AUDIO_FILE_PATH, "en");

            expect(response).to.not.be.undefined;
            expect(response.text).to.not.be.undefined;
            expect(response.text).to.be.a('string');
            expect(response.text.length).to.be.greaterThan(0);
            expect(response.text).to.equal(EXPECTED_TEXT);
            console.log(`Response: ${response.text}`);
        } finally {
            await model.unload();
        }
    });

    it('should transcribe audio without streaming with temperature', async function() {
        this.timeout(30000);
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        const cachedModels = await catalog.getCachedModels();
        expect(cachedModels.length).to.be.greaterThan(0);

        const cachedVariant = cachedModels.find(m => m.alias === WHISPER_MODEL_ALIAS);
        expect(cachedVariant, 'whisper-tiny should be cached').to.not.be.undefined;

        const model = await catalog.getModel(WHISPER_MODEL_ALIAS);
        expect(model).to.not.be.undefined;
        if (!model || !cachedVariant) return;

        model.selectVariant(cachedVariant.id);
        await model.load();
        
        try {
            const audioClient = model.createAudioClient();
            expect(audioClient).to.not.be.undefined;

            const response = await audioClient.transcribe(AUDIO_FILE_PATH, "en", 0.1);

            expect(response).to.not.be.undefined;
            expect(response.text).to.not.be.undefined;
            expect(response.text).to.be.a('string');
            expect(response.text.length).to.be.greaterThan(0);
            expect(response.text).to.equal(EXPECTED_TEXT);
            console.log(`Response: ${response.text}`);
        } finally {
            await model.unload();
        }
    });

    it('should transcribe audio with streaming', async function() {
        this.timeout(30000);
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        const cachedModels = await catalog.getCachedModels();
        expect(cachedModels.length).to.be.greaterThan(0);

        const cachedVariant = cachedModels.find(m => m.alias === WHISPER_MODEL_ALIAS);
        expect(cachedVariant, 'whisper-tiny should be cached').to.not.be.undefined;

        const model = await catalog.getModel(WHISPER_MODEL_ALIAS);
        expect(model).to.not.be.undefined;
        if (!model || !cachedVariant) return;

        model.selectVariant(cachedVariant.id);
        await model.load();
        
        try {
            const audioClient = model.createAudioClient();
            expect(audioClient).to.not.be.undefined;

            let fullResponse = '';
            await audioClient.transcribeStreaming(AUDIO_FILE_PATH, (chunk) => {
                expect(chunk).to.not.be.undefined;
                expect(chunk.text).to.not.be.undefined;
                expect(chunk.text).to.be.a('string');
                expect(chunk.text.length).to.be.greaterThan(0);
                fullResponse += chunk.text;
            }, "en");

            console.log(`Full response: ${fullResponse}`);
            expect(fullResponse).to.equal(EXPECTED_TEXT);
        } finally {
            await model.unload();
        }
    });

    it('should transcribe audio with streaming with temperature', async function() {
        this.timeout(30000);
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        const cachedModels = await catalog.getCachedModels();
        expect(cachedModels.length).to.be.greaterThan(0);

        const cachedVariant = cachedModels.find(m => m.alias === WHISPER_MODEL_ALIAS);
        expect(cachedVariant, 'whisper-tiny should be cached').to.not.be.undefined;

        const model = await catalog.getModel(WHISPER_MODEL_ALIAS);
        expect(model).to.not.be.undefined;
        if (!model || !cachedVariant) return;

        model.selectVariant(cachedVariant.id);
        await model.load();
        
        try {
            const audioClient = model.createAudioClient();
            expect(audioClient).to.not.be.undefined;

            let fullResponse = '';
            await audioClient.transcribeStreaming(AUDIO_FILE_PATH, (chunk) => {
                expect(chunk).to.not.be.undefined;
                expect(chunk.text).to.not.be.undefined;
                expect(chunk.text).to.be.a('string');
                expect(chunk.text.length).to.be.greaterThan(0);
                fullResponse += chunk.text;
            }, "en", 0.1);

            console.log(`Full response: ${fullResponse}`);
            expect(fullResponse).to.equal(EXPECTED_TEXT);
        } finally {
            await model.unload();
        }
    });
});
