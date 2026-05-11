import { before, describe, it } from 'mocha';
import { expect } from 'chai';
import { getTestManager } from './testUtils.js';

describe('Foundry Local Manager Tests', function() {
    this.timeout(10000);

    let manager: ReturnType<typeof getTestManager>;

    before(function() {
        manager = getTestManager();
    });

    it('should initialize successfully', function() {
        expect(manager).to.not.be.undefined;
    });

    it('should return catalog', function() {
        const catalog = manager.catalog;
        
        expect(catalog).to.not.be.undefined;
        // We don't assert the exact name as it might change, but we ensure it exists
        expect(catalog.name).to.be.a('string');
    });

    it('downloadAndRegisterEps should call command without params when names are omitted', async function() {
        const internalManager = manager as any;
        const calls: unknown[][] = [];
        const originalExecuteCommandStreaming = internalManager.coreInterop.executeCommandStreaming;

        internalManager.coreInterop.executeCommandStreaming = (...args: unknown[]) => {
            calls.push(args);
            return Promise.resolve(JSON.stringify({
                Success: true,
                Status: 'All providers registered',
                RegisteredEps: ['CUDAExecutionProvider'],
                FailedEps: []
            }));
        };

        try {
            const result = await manager.downloadAndRegisterEps();
            expect(calls.length).to.equal(1);
            expect(calls[0][0]).to.equal('download_and_register_eps');
            expect(calls[0][1]).to.be.undefined;
            expect(result).to.deep.equal({
                success: true,
                status: 'All providers registered',
                registeredEps: ['CUDAExecutionProvider'],
                failedEps: []
            });
        } finally {
            internalManager.coreInterop.executeCommandStreaming = originalExecuteCommandStreaming;
        }
    });

    it('downloadAndRegisterEps should send Names param when subset is provided', async function() {
        const internalManager = manager as any;
        const calls: unknown[][] = [];
        const originalExecuteCommandStreaming = internalManager.coreInterop.executeCommandStreaming;

        internalManager.coreInterop.executeCommandStreaming = (...args: unknown[]) => {
            calls.push(args);
            return Promise.resolve(JSON.stringify({
                Success: false,
                Status: 'Some providers failed',
                RegisteredEps: ['CUDAExecutionProvider'],
                FailedEps: ['OpenVINOExecutionProvider']
            }));
        };

        try {
            const result = await manager.downloadAndRegisterEps(['CUDAExecutionProvider', 'OpenVINOExecutionProvider']);
            expect(calls.length).to.equal(1);
            expect(calls[0][0]).to.equal('download_and_register_eps');
            expect(calls[0][1]).to.deep.equal({ Params: { Names: 'CUDAExecutionProvider,OpenVINOExecutionProvider' } });
            expect(result).to.deep.equal({
                success: false,
                status: 'Some providers failed',
                registeredEps: ['CUDAExecutionProvider'],
                failedEps: ['OpenVINOExecutionProvider']
            });
        } finally {
            internalManager.coreInterop.executeCommandStreaming = originalExecuteCommandStreaming;
        }
    });
});
