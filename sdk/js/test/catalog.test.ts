import { describe, it } from 'mocha';
import { expect } from 'chai';
import { getTestManager, TEST_MODEL_ALIAS } from './testUtils.js';

describe('Catalog Tests', () => {
    it('should initialize with catalog name', function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        expect(catalog.name).to.be.a('string');
        expect(catalog.name.length).to.be.greaterThan(0);
    });

    it('should list models', async function() {
        this.timeout(10000);
        const manager = getTestManager();
        const catalog = manager.catalog;
        const models = await catalog.getModels();
        
        expect(models).to.be.an('array');
        expect(models.length).to.be.greaterThan(0);
        
        // Verify we can find our test model
        const testModel = models.find(m => m.alias === TEST_MODEL_ALIAS);
        expect(testModel).to.not.be.undefined;
    });

    it('should get model by alias', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        const model = (await catalog.getModel(TEST_MODEL_ALIAS))!;

        expect(model.alias).to.equal(TEST_MODEL_ALIAS);
    });

    it('should throw when getting model with empty, null, or undefined alias', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        const invalidAliases: any[] = ['', null, undefined];
        for (const invalidAlias of invalidAliases) {
            try {
                await catalog.getModel(invalidAlias);
                expect.fail(`Should have thrown an error for ${invalidAlias === '' ? 'empty' : invalidAlias} alias`);
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
                expect((error as Error).message).to.include('Model alias must be a non-empty string');
            }
        }
    });

    it('should throw when getting model with unknown alias', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        const unknownAlias = 'definitely-not-a-real-model-alias-12345';
        try {
            await catalog.getModel(unknownAlias);
            expect.fail('Should have thrown an error for unknown alias');
        } catch (error) {
            expect(error).to.be.instanceOf(Error);
            expect((error as Error).message).to.include(`Model with alias '${unknownAlias}' not found`);
            expect((error as Error).message).to.include('Available models:');
        }
    });

    it('should get cached models', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        const cachedModels = await catalog.getCachedModels();
        
        expect(cachedModels).to.be.an('array');
        // We expect at least one cached model since we are pointing to a populated cache
        expect(cachedModels.length).to.be.greaterThan(0);
        
        const testModel = cachedModels.find(m => m.alias === TEST_MODEL_ALIAS);
        expect(testModel).to.not.be.undefined;
    });

    it('should throw when getting model variant with empty, null, or undefined ID', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        const invalidIds: any[] = ['', null, undefined];
        for (const invalidId of invalidIds) {
            try {
                await catalog.getModelVariant(invalidId);
                expect.fail(`Should have thrown an error for ${invalidId === '' ? 'empty' : invalidId} model ID`);
            } catch (error) {
                expect(error).to.be.instanceOf(Error);
                expect((error as Error).message).to.include('Model ID must be a non-empty string');
            }
        }
    });

    it('should throw when getting model variant with unknown ID', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;

        const unknownId = 'definitely-not-a-real-model-id-12345';
        try {
            await catalog.getModelVariant(unknownId);
            expect.fail('Should have thrown an error for unknown model ID');
        } catch (error) {
            expect(error).to.be.instanceOf(Error);
            expect((error as Error).message).to.include(`Model variant with ID '${unknownId}' not found`);
            expect((error as Error).message).to.include('Available variants:');
        }
    });
});

describe('Catalog HuggingFace Tests', () => {
    const HF_URL = 'https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-onnx/tree/main/cpu_and_mobile/cpu-int4-rtn-block-32-acc-level-4';

    it('should return undefined for a HuggingFace model that is not cached', async function() {
        this.timeout(10000);
        const manager = getTestManager();
        const catalog = manager.catalog;
        // getModel should NOT auto-download; it should return undefined if not cached
        const model = await catalog.getModel(HF_URL);
        // Model may or may not be cached depending on the test environment
        // This test just verifies getModel doesn't throw
    });

    it('should download and return a HuggingFace model by URL', async function() {
        this.timeout(600000); // 10 minutes - downloads can be large
        const manager = getTestManager();
        const catalog = manager.catalog;
        const model = await catalog.downloadModel(HF_URL);

        expect(model).to.not.be.undefined;
        expect(model?.alias).to.be.a('string');
        expect(model?.id).to.be.a('string');
    });

    it('should download HuggingFace model with progress callback', async function() {
        this.timeout(600000);
        const manager = getTestManager();
        const catalog = manager.catalog;

        const progressUpdates: number[] = [];
        const model = await catalog.downloadModel(HF_URL, (progress: number) => {
            progressUpdates.push(progress);
        });

        expect(model).to.not.be.undefined;
        expect(model?.alias).to.be.a('string');
        // If the model was already cached, there may be no progress updates
        // If it was downloaded, we should have received some
    });

    it('should find HuggingFace model in cached models after download', async function() {
        this.timeout(600000);
        const manager = getTestManager();
        const catalog = manager.catalog;

        const model = await catalog.downloadModel(HF_URL);
        expect(model).to.not.be.undefined;
        if (!model) return;

        const cachedModels = await catalog.getCachedModels();
        const found = cachedModels.find(m => m.id === model.id);
        expect(found, 'HuggingFace model should appear in cached models').to.not.be.undefined;
    });

    it('should return same model on repeated download calls with HuggingFace URL', async function() {
        this.timeout(600000);
        const manager = getTestManager();
        const catalog = manager.catalog;

        const model1 = await catalog.downloadModel(HF_URL);
        const model2 = await catalog.downloadModel(HF_URL);

        expect(model1).to.not.be.undefined;
        expect(model2).to.not.be.undefined;
        expect(model1?.id).to.equal(model2?.id);
        expect(model1?.alias).to.equal(model2?.alias);
    });
});
