import { before, describe, it } from 'mocha';
import { expect } from 'chai';
import fs from 'fs';
import path from 'path';
import { getTestManager, TEST_MODEL_ALIAS } from './testUtils.js';
import { Model } from '../src/detail/model.js';
import { ModelVariant } from '../src/detail/modelVariant.js';
import { DeviceType, type ModelInfo } from '../src/types.js';

describe('Model Tests', function() {
    this.timeout(10000);

    let manager: ReturnType<typeof getTestManager>;

    before(function() {
        manager = getTestManager();
    });

    it('should verify cached models from test-data-shared', async function() {
        const catalog = manager.catalog;
        const cachedModels = await catalog.getCachedModels();

        // Check for qwen model
        const qwenModel = cachedModels.find(m => m.alias === 'qwen2.5-0.5b');

        // Check for whisper model
        const whisperModel = cachedModels.find(m => m.alias === 'whisper-tiny');
        if (!qwenModel || !whisperModel) {
            console.log('  (skipped: required cached test-data-shared models not available)');
            this.skip();
            return;
        }

        expect(cachedModels).to.be.an('array');
        expect(cachedModels.length).to.be.greaterThan(0);
        expect(qwenModel.isCached).to.be.true;
        expect(whisperModel.isCached).to.be.true;
    });

    it('should load and unload model', async function() {
        const catalog = manager.catalog;
        
        // Ensure cache is populated first
        const cachedModels = await catalog.getCachedModels();
        const cachedVariant = cachedModels.find(m => m.alias === TEST_MODEL_ALIAS);
        if (!cachedVariant) {
            console.log(`  (skipped: ${TEST_MODEL_ALIAS} model not cached)`);
            this.skip();
            return;
        }

        const model = await catalog.getModel(TEST_MODEL_ALIAS);
        expect(model).to.not.be.undefined;
        if (!model) return;

        // Select the cached variant by finding it in the model's variants
        const matchingVariant = model.variants.find(v => v.id === cachedVariant.id);
        expect(matchingVariant).to.not.be.undefined;
        if (matchingVariant) {
            model.selectVariant(matchingVariant);
        }

        const modelPath = model.path;
        const modelFile = path.join(modelPath, 'model.onnx');
        if (!fs.existsSync(modelFile)) {
            console.log(`  (skipped: cached model payload missing on disk: ${modelFile})`);
            this.skip();
            return;
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
            executeCommandAsync: () => {
                throw new Error('download should not use executeCommandAsync when a signal is provided');
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

        await model.download(controller.signal);
        expect(calls.length).to.equal(1);
        expect(calls[0][0]).to.equal('download_model');
        expect(calls[0][3]).to.equal(controller.signal);
    });

    it('download should preserve undefined progress callback with AbortSignal overload', async function() {
        const calls: unknown[][] = [];
        const controller = new AbortController();
        const fakeCoreInterop = {
            executeCommandAsync: () => {
                throw new Error('download should not use executeCommandAsync when a signal is provided');
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

    it('download should parse a numeric progress chunk', async function() {
        const progress: number[] = [];
        const fakeCoreInterop = {
            executeCommandAsync: () => {
                throw new Error('download should use streaming interop when progress is provided');
            },
            executeCommandStreaming: async (
                _command: string,
                _request: unknown,
                callback: (chunk: string) => void
            ) => {
                callback('12.5');
                return '';
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

        await model.download(progress.push.bind(progress));

        expect(progress).to.deep.equal([12.5]);
    });
});
