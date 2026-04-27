import { describe, it } from 'mocha';
import { expect } from 'chai';
import { getTestManager } from './testUtils.js';
import { FoundryLocalManager } from '../src/foundryLocalManager.js';

describe('Foundry Local Manager Tests', () => {
    it('should initialize successfully', function() {
        const manager = getTestManager();
        expect(manager).to.not.be.undefined;
    });

    it('should return catalog', function() {
        const manager = getTestManager();
        const catalog = manager.catalog;
        
        expect(catalog).to.not.be.undefined;
        // We don't assert the exact name as it might change, but we ensure it exists
        expect(catalog.name).to.be.a('string');
    });

    it('downloadAndRegisterEps should call command without params when names are omitted', async function() {
        const manager = getTestManager() as any;
        const calls: unknown[][] = [];
        const originalExecuteCommandStreaming = manager.coreInterop.executeCommandStreaming;

        manager.coreInterop.executeCommandStreaming = (...args: unknown[]) => {
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
            manager.coreInterop.executeCommandStreaming = originalExecuteCommandStreaming;
        }
    });

    it('downloadAndRegisterEps should send Names param when subset is provided', async function() {
        const manager = getTestManager() as any;
        const calls: unknown[][] = [];
        const originalExecuteCommandStreaming = manager.coreInterop.executeCommandStreaming;

        manager.coreInterop.executeCommandStreaming = (...args: unknown[]) => {
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
            manager.coreInterop.executeCommandStreaming = originalExecuteCommandStreaming;
        }
    });

    it('downloadAndRegisterEps should pass AbortSignal through to streaming interop', async function() {
        const calls: unknown[][] = [];
        const controller = new AbortController();
        const manager = Object.create(FoundryLocalManager.prototype) as any;
        manager.coreInterop = {
            executeCommandStreaming: (...args: unknown[]) => {
                calls.push(args);
                return Promise.resolve(JSON.stringify({
                    Success: true,
                    Status: 'All providers registered',
                    RegisteredEps: ['CUDAExecutionProvider'],
                    FailedEps: []
                }));
            }
        };
        manager._catalog = {
            invalidateCache: () => {}
        };

        await FoundryLocalManager.prototype.downloadAndRegisterEps.call(
            manager,
            ['CUDAExecutionProvider'],
            controller.signal
        );
        expect(calls.length).to.equal(1);
        expect(calls[0][0]).to.equal('download_and_register_eps');
        expect(calls[0][3]).to.equal(controller.signal);
    });
});
