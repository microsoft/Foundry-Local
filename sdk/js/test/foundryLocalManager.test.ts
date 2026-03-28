import { describe, it } from 'mocha';
import { expect } from 'chai';
import { getTestManager } from './testUtils.js';

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

    it('downloadAndRegisterEps should call command without params when names are omitted', function() {
        const manager = getTestManager() as any;
        const calls: unknown[][] = [];
        const originalExecuteCommand = manager.coreInterop.executeCommand;

        manager.coreInterop.executeCommand = (...args: unknown[]) => {
            calls.push(args);
            return JSON.stringify({
                Success: true,
                Status: 'All providers registered',
                RegisteredEps: ['CUDAExecutionProvider'],
                FailedEps: []
            });
        };

        try {
            const result = manager.downloadAndRegisterEps();
            expect(calls).to.deep.equal([['download_and_register_eps', undefined]]);
            expect(result).to.deep.equal({
                success: true,
                status: 'All providers registered',
                registeredEps: ['CUDAExecutionProvider'],
                failedEps: []
            });
        } finally {
            manager.coreInterop.executeCommand = originalExecuteCommand;
        }
    });

    it('downloadAndRegisterEps should send Names param when subset is provided', function() {
        const manager = getTestManager() as any;
        const calls: unknown[][] = [];
        const originalExecuteCommand = manager.coreInterop.executeCommand;

        manager.coreInterop.executeCommand = (...args: unknown[]) => {
            calls.push(args);
            return JSON.stringify({
                Success: false,
                Status: 'Some providers failed',
                RegisteredEps: ['CUDAExecutionProvider'],
                FailedEps: ['OpenVINOExecutionProvider']
            });
        };

        try {
            const result = manager.downloadAndRegisterEps(['CUDAExecutionProvider', 'OpenVINOExecutionProvider']);
            expect(calls).to.deep.equal([
                ['download_and_register_eps', { Params: { Names: 'CUDAExecutionProvider,OpenVINOExecutionProvider' } }]
            ]);
            expect(result).to.deep.equal({
                success: false,
                status: 'Some providers failed',
                registeredEps: ['CUDAExecutionProvider'],
                failedEps: ['OpenVINOExecutionProvider']
            });
        } finally {
            manager.coreInterop.executeCommand = originalExecuteCommand;
        }
    });
});
