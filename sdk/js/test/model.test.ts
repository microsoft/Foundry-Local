import { describe, it } from 'mocha';
import { expect } from 'chai';
import { getTestManager, TEST_MODEL_ALIAS } from './testUtils.js';
import { Model } from '../src/detail/model.js';
import { ModelVariant } from '../src/detail/modelVariant.js';
import { DeviceType, type ModelInfo } from '../src/types.js';

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

        // Select the cached variant by finding it in the model's variants
        const matchingVariant = model.variants.find(v => v.id === cachedVariant.id);
        expect(matchingVariant).to.not.be.undefined;
        if (matchingVariant) {
            model.selectVariant(matchingVariant);
        }

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

    it('download should use streaming interop when only an AbortSignal is provided', async function() {
        const calls: unknown[][] = [];
        const controller = new AbortController();
        const fakeCoreInterop = {
            executeCommand: () => {
                throw new Error('download should not use executeCommand when a signal is provided');
            },
            executeCommandStreaming: (...args: unknown[]) => {
                calls.push(args);
                return Promise.resolve('');
            }
        };
        const modelInfo: ModelInfo = {
            id: 'test-model-cpu:1',
            name: 'test-model-cpu',
            version: 1,
            alias: TEST_MODEL_ALIAS,
            providerType: 'AzureFoundry',
            uri: 'azureml://registries/azureml/models/test-model-cpu/versions/1',
            modelType: 'ONNX',
            cached: false,
            createdAtUnix: 0,
            runtime: {
                deviceType: DeviceType.CPU,
                executionProvider: 'CPUExecutionProvider'
            }
        };
        const variant = new ModelVariant(modelInfo, fakeCoreInterop as any, {} as any);
        const model = new Model(variant);

        await model.download(undefined, controller.signal);
        expect(calls.length).to.equal(1);
        expect(calls[0][0]).to.equal('download_model');
        expect(calls[0][3]).to.equal(controller.signal);
    });
});
