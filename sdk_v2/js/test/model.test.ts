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
});
