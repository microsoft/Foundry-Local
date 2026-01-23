import { expect } from 'chai';
import { ModelLoadManager } from '../../src/detail/modelLoadManager.js';
import { getTestManager, TEST_MODEL_ALIAS, IS_RUNNING_IN_CI } from '../testUtils.js';

describe('ModelLoadManager', function() {
    let coreInterop: any;
    let modelId: string;
    let managerInstance: any;
    let serviceUrl: string;

    before(async function() {
        managerInstance = getTestManager();
        // Access private coreInterop using any cast
        coreInterop = (managerInstance as any).coreInterop;
        
        const catalog = managerInstance.catalog;
        const model = await catalog.getModel(TEST_MODEL_ALIAS);
        if (!model) {
            throw new Error(`Model ${TEST_MODEL_ALIAS} not found in catalog`);
        }
        modelId = model.id;

        // Start the real web service if not in CI
        if (!IS_RUNNING_IN_CI) {
            try {
                managerInstance.startWebService();
                const urls = managerInstance.urls;
                if (!urls || urls.length === 0) {
                    console.warn("Web service started but no URLs returned");
                } else {
                    serviceUrl = urls[0];
                }
            } catch (e: any) {
                console.warn(`Skipping web service tests: Failed to start web service (${e.message})`);
                // If start_web_service is not supported by the local core binary, we can't run these tests.
            }
        }
    });

    after(function() {
        if (!IS_RUNNING_IN_CI && managerInstance) {
            try {
                managerInstance.stopWebService();
            } catch (e) {
                console.warn("Failed to stop web service:", e);
            }
        }
    });

    it('should load model using core interop when no external url is provided', async function() {
        this.timeout(30000);
        const manager = new ModelLoadManager(coreInterop);
        
        await manager.load(modelId);
        
        const loaded = await manager.listLoaded();
        expect(loaded).to.include(modelId);
    });

    it('should unload model using core interop when no external url is provided', async function() {
        this.timeout(30000);
        const manager = new ModelLoadManager(coreInterop);
        
        await manager.load(modelId);
        let loaded = await manager.listLoaded();
        expect(loaded).to.include(modelId);

        await manager.unload(modelId);
        
        loaded = await manager.listLoaded();
        expect(loaded).to.not.include(modelId);
    });

    it('should list loaded models using core interop when no external url is provided', async function() {
        this.timeout(30000);
        const manager = new ModelLoadManager(coreInterop);
        
        await manager.load(modelId);
        
        const loaded = await manager.listLoaded();
        expect(loaded).to.be.an('array');
        expect(loaded).to.include(modelId);
    });

    it('should load and unload model using external service when url is provided', async function() {
        if (IS_RUNNING_IN_CI || !serviceUrl) {
            this.skip();
        }
        
        const manager = new ModelLoadManager(coreInterop, serviceUrl);
        
        // Load it first so we can unload it (can use core interop to setup state)
        // Creating a manager WITHOUT serviceUrl to force core interop usage for setup
        const setupManager = new ModelLoadManager(coreInterop);
        await setupManager.load(modelId); 
        
        let loaded = await setupManager.listLoaded();
        expect(loaded).to.include(modelId);

        // Unload via external service
        await manager.unload(modelId);
        
        // Verify via core interop
        loaded = await setupManager.listLoaded();
        expect(loaded).to.not.include(modelId);
    });

    it('should list loaded models using external service when url is provided', async function() {
        if (IS_RUNNING_IN_CI || !serviceUrl) {
            this.skip();
        }
        
        const manager = new ModelLoadManager(coreInterop, serviceUrl);
        const setupManager = new ModelLoadManager(coreInterop);
        
        // Setup: Load model via core
        await setupManager.load(modelId);

        // Verify: List via external service
        const loaded = await manager.listLoaded();
        expect(loaded).to.be.an('array');
        expect(loaded).to.include(modelId);
    });
});
