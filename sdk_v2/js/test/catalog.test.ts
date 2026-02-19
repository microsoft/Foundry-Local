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

    it('should throw when getting model with empty, null, undefined, or unknown alias', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        const invalidAliases: any[] = ['', null, undefined, 'definitely-not-a-real-model-alias-12345'];
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

    it('should throw when getting model variant with empty, null, undefined, or unknown modelId', async function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        const invalidIds: any[] = ['', null, undefined, 'definitely-not-a-real-model-id-12345'];
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
});
