import { describe, it } from 'mocha';
import { expect } from 'chai';
import { getTestManager } from './testUtils.js';

const HF_URL = 'https://huggingface.co/onnxruntime/Phi-3-mini-4k-instruct-onnx/tree/main/cpu_and_mobile/cpu-int4-rtn-block-32-acc-level-4';

describe('HuggingFace Catalog Tests', () => {
    it('should create huggingface catalog', async function() {
        const manager = getTestManager();
        const hfCatalog = await manager.addCatalog('https://huggingface.co');
        expect(hfCatalog.name).to.equal('HuggingFace');
    });

    it('should reject non-huggingface url', async function() {
        const manager = getTestManager();
        try {
            await manager.addCatalog('https://example.com');
            expect.fail('Should have thrown an error for non-HuggingFace URL');
        } catch (error) {
            expect(error).to.be.instanceOf(Error);
            expect((error as Error).message).to.include('Unsupported catalog URL');
        }
    });

    it('should register model', async function() {
        this.timeout(30000);
        const manager = getTestManager();
        const hfCatalog = await manager.addCatalog('https://huggingface.co');

        const model = await hfCatalog.registerModel(HF_URL);
        expect(model.alias).to.be.a('string');
        expect(model.alias.length).to.be.greaterThan(0);
        expect(model.id).to.be.a('string');
        expect(model.id.length).to.be.greaterThan(0);
    });

    it('should find registered model by identifier', async function() {
        this.timeout(30000);
        const manager = getTestManager();
        const hfCatalog = await manager.addCatalog('https://huggingface.co');

        await hfCatalog.registerModel(HF_URL);

        const found = await hfCatalog.getModel(HF_URL);
        expect(found).to.not.be.undefined;
    });

    it('should register then download model', async function() {
        this.timeout(600000);
        const manager = getTestManager();
        const hfCatalog = await manager.addCatalog('https://huggingface.co');

        const model = await hfCatalog.registerModel(HF_URL);
        expect(model.alias.length).to.be.greaterThan(0);

        // Now download the ONNX files
        await model.download();
    });

    it('should reject registration of plain alias', async function() {
        const manager = getTestManager();
        const hfCatalog = await manager.addCatalog('https://huggingface.co');

        try {
            await hfCatalog.registerModel('phi-3-mini');
            expect.fail('Should have thrown an error for plain alias');
        } catch (error) {
            expect(error).to.be.instanceOf(Error);
            expect((error as Error).message).to.include('not a valid HuggingFace URL');
        }
    });

    it('should list registered models', async function() {
        this.timeout(30000);
        const manager = getTestManager();
        const hfCatalog = await manager.addCatalog('https://huggingface.co');

        await hfCatalog.registerModel(HF_URL);

        const models = await hfCatalog.getModels();
        expect(models).to.be.an('array');
        expect(models.length).to.be.greaterThan(0);
    });
});
