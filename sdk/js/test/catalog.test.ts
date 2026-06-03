import { describe, it } from 'mocha';
import { expect } from 'chai';
import { Catalog } from '../src/catalog.js';
import { DeviceType, type ModelInfo } from '../src/types.js';
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

    it('should resolve latest version for model and variant inputs', async function() {
        // Mirror the C# test by using synthetic model data sorted by version descending.
        const testModelInfos: ModelInfo[] = [
            {
                id: 'test-model:3',
                name: 'test-model',
                version: 3,
                alias: 'test-alias',
                displayName: 'Test Model',
                providerType: 'test',
                uri: 'test://model/3',
                modelType: 'ONNX',
                runtime: { deviceType: DeviceType.CPU, executionProvider: 'CPUExecutionProvider' },
                cached: false,
                createdAtUnix: 1700000003
            },
            {
                id: 'test-model:2',
                name: 'test-model',
                version: 2,
                alias: 'test-alias',
                displayName: 'Test Model',
                providerType: 'test',
                uri: 'test://model/2',
                modelType: 'ONNX',
                runtime: { deviceType: DeviceType.CPU, executionProvider: 'CPUExecutionProvider' },
                cached: false,
                createdAtUnix: 1700000002
            },
            {
                id: 'test-model:1',
                name: 'test-model',
                version: 1,
                alias: 'test-alias',
                displayName: 'Test Model',
                providerType: 'test',
                uri: 'test://model/1',
                modelType: 'ONNX',
                runtime: { deviceType: DeviceType.CPU, executionProvider: 'CPUExecutionProvider' },
                cached: false,
                createdAtUnix: 1700000001
            }
        ];

        const mockCoreInterop = {
            executeCommand(command: string): string {
                if (command === 'get_model_list') {
                    return JSON.stringify(testModelInfos);
                }
                if (command === 'get_cached_models') {
                    return '[]';
                }
                throw new Error(`Unexpected command: ${command}`);
            },
            executeCommandAsync(command: string): Promise<string> {
                return Promise.resolve(this.executeCommand(command));
            }
        } as any;

        const mockLoadManager = {
            listLoaded: async () => []
        } as any;

        const catalog = new Catalog(mockCoreInterop, mockLoadManager, 'TestCatalog');

        const model = await catalog.getModel('test-alias');
        expect(model).to.not.be.undefined;

        const variants = model.variants;
        expect(variants).to.have.length(3);

        const latestVariant = variants[0];
        const middleVariant = variants[1];
        const oldestVariant = variants[2];

        expect(latestVariant.id).to.equal('test-model:3');
        expect(middleVariant.id).to.equal('test-model:2');
        expect(oldestVariant.id).to.equal('test-model:1');

        const result1 = await catalog.getLatestVersion(latestVariant);
        expect(result1.id).to.equal('test-model:3');

        const result2 = await catalog.getLatestVersion(middleVariant);
        expect(result2.id).to.equal('test-model:3');

        const result3 = await catalog.getLatestVersion(oldestVariant);
        expect(result3.id).to.equal('test-model:3');

        model.selectVariant(latestVariant);
        const resultFromModel = await catalog.getLatestVersion(model);
        expect(resultFromModel).to.equal(model);
    });

    it('should self-heal getModel and getModelVariant on cache miss', async function() {
        // Mirrors the Python self-heal test: warm catalog returns no models;
        // the second (forced) refresh from getModel/getModelVariant surfaces
        // the BYOM that was dropped into the cache after the SDK warmed up.
        const byomModelInfos: ModelInfo[] = [
            {
                id: 'byom-self-heal:1',
                name: 'byom-self-heal',
                version: 1,
                alias: 'byom-self-heal',
                displayName: 'BYOM Self Heal',
                providerType: 'local',
                uri: 'local://byom-self-heal/1',
                modelType: 'ONNX',
                runtime: { deviceType: DeviceType.CPU, executionProvider: 'CPUExecutionProvider' },
                cached: true,
                createdAtUnix: 1700000001
            }
        ];

        let modelListCalls = 0;

        const mockCoreInterop = {
            executeCommand(command: string): string {
                if (command === 'get_catalog_name') {
                    return 'TestCatalog';
                }
                throw new Error(`Unexpected sync command: ${command}`);
            },
            executeCommandAsync(command: string): Promise<string> {
                if (command === 'get_model_list') {
                    modelListCalls += 1;
                    return Promise.resolve(modelListCalls > 1 ? JSON.stringify(byomModelInfos) : '[]');
                }
                if (command === 'get_cached_models') {
                    return Promise.resolve('[]');
                }
                return Promise.reject(new Error(`Unexpected async command: ${command}`));
            }
        } as any;

        const mockLoadManager = {
            listLoaded: async () => []
        } as any;

        const catalog = new Catalog(mockCoreInterop, mockLoadManager, 'TestCatalog');

        const model = await catalog.getModel('byom-self-heal');
        expect(model).to.not.be.undefined;
        expect(model.alias).to.equal('byom-self-heal');

        const variant = await catalog.getModelVariant('byom-self-heal:1');
        expect(variant).to.not.be.undefined;
        expect(variant.id).to.equal('byom-self-heal:1');

        // First lookup: warm + forced refresh (2 calls).
        // Second lookup finds the now-populated alias/variant map; the inner
        // TTL-gated updateModels() short-circuits, so no extra fetches.
        expect(modelListCalls).to.equal(2);
    });

    it('should self-heal getCachedModels when core returns an unknown id', async function() {
        // Core always reports the BYOM as cached; the SDK has to self-heal in
        // resolveModelIds (forced updateModels) to learn it exists in the
        // catalog and surface it from getCachedModels.
        const byomModelInfos: ModelInfo[] = [
            {
                id: 'byom-cached:1',
                name: 'byom-cached',
                version: 1,
                alias: 'byom-cached',
                displayName: 'BYOM Cached',
                providerType: 'local',
                uri: 'local://byom-cached/1',
                modelType: 'ONNX',
                runtime: { deviceType: DeviceType.CPU, executionProvider: 'CPUExecutionProvider' },
                cached: true,
                createdAtUnix: 1700000001
            }
        ];

        let modelListCalls = 0;

        const mockCoreInterop = {
            executeCommand(command: string): string {
                if (command === 'get_catalog_name') {
                    return 'TestCatalog';
                }
                throw new Error(`Unexpected sync command: ${command}`);
            },
            executeCommandAsync(command: string): Promise<string> {
                if (command === 'get_model_list') {
                    modelListCalls += 1;
                    return Promise.resolve(modelListCalls > 1 ? JSON.stringify(byomModelInfos) : '[]');
                }
                if (command === 'get_cached_models') {
                    return Promise.resolve('["byom-cached:1"]');
                }
                return Promise.reject(new Error(`Unexpected async command: ${command}`));
            }
        } as any;

        const mockLoadManager = {
            listLoaded: async () => []
        } as any;

        const catalog = new Catalog(mockCoreInterop, mockLoadManager, 'TestCatalog');

        const cached = await catalog.getCachedModels();
        expect(cached).to.have.length(1);
        expect(cached[0].id).to.equal('byom-cached:1');
        expect(modelListCalls).to.equal(2);
    });
});
