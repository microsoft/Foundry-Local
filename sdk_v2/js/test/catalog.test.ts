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
        const model = await catalog.getModel(TEST_MODEL_ALIAS);
        
        expect(model.alias).to.equal(TEST_MODEL_ALIAS);
    });

    it('should throw when getting model with empty alias', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        try {
            await catalog.getModel('');
            expect.fail('Should have thrown an error for empty alias');
        } catch (error) {
            expect(error).to.be.instanceOf(Error);
            expect((error as Error).message).to.include('Model alias must be a non-empty string');
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

    it('should throw when getting model variant with empty ID', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        try {
            await catalog.getModelVariant('');
            expect.fail('Should have thrown an error for empty model ID');
        } catch (error) {
            expect(error).to.be.instanceOf(Error);
            expect((error as Error).message).to.include('Model ID must be a non-empty string');
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
