import { describe, it } from 'mocha';
import { expect } from 'chai';
import { CoreInterop } from '../../src/detail/coreInterop.js';

describe('CoreInterop Tests', () => {
    it('executeCommandAsync should reject without calling native interop when signal is already aborted', async function() {
        const controller = new AbortController();
        controller.abort();
        const interop = Object.create(CoreInterop.prototype) as any;
        interop.addon = {
            executeCommandAsync: () => {
                throw new Error('native interop should not be called for an already aborted signal');
            }
        };

        let caught: unknown;
        try {
            await CoreInterop.prototype.executeCommandAsync.call(
                interop,
                'chat_completions',
                undefined,
                controller.signal
            );
        } catch (error) {
            caught = error;
        }

        expect(caught).to.be.instanceOf(Error);
        expect((caught as Error).name).to.equal('AbortError');
    });

    it('executeCommandAsync should cancel and release a native cancellation context', async function() {
        const controller = new AbortController();
        const calls: string[] = [];
        const interop = Object.create(CoreInterop.prototype) as any;
        interop.addon = {
            hasCancellableCommands: () => true,
            createCancellationContext: () => {
                calls.push('create');
                return 42;
            },
            cancelCancellationContext: (id: number) => {
                calls.push(`cancel:${id}`);
            },
            releaseCancellationContext: (id: number) => {
                calls.push(`release:${id}`);
            },
            executeCommandAsync: async (_command: string, _dataJson: string, contextId?: number) => {
                calls.push(`execute:${contextId}`);
                controller.abort();
                throw new Error("Command 'chat_completions' failed: Operation was cancelled by user");
            }
        };

        let caught: unknown;
        try {
            await CoreInterop.prototype.executeCommandAsync.call(
                interop,
                'chat_completions',
                undefined,
                controller.signal
            );
        } catch (error) {
            caught = error;
        }

        expect(calls).to.deep.equal(['create', 'execute:42', 'cancel:42', 'release:42']);
        expect(caught).to.be.instanceOf(Error);
        expect((caught as Error).name).to.equal('AbortError');
    });

    it('executeCommandStreaming should reject without calling native interop when signal is already aborted', async function() {
        const controller = new AbortController();
        controller.abort();
        const interop = Object.create(CoreInterop.prototype) as any;
        interop.addon = {
            executeCommandStreaming: () => {
                throw new Error('native interop should not be called for an already aborted signal');
            }
        };

        let caught: unknown;
        try {
            await CoreInterop.prototype.executeCommandStreaming.call(
                interop,
                'download_model',
                undefined,
                () => {},
                controller.signal
            );
        } catch (error) {
            caught = error;
        }

        expect(caught).to.be.instanceOf(Error);
        expect((caught as Error).name).to.equal('AbortError');
    });

    it('executeCommandStreaming should reject when signal is aborted before the next callback', async function() {
        const controller = new AbortController();
        const chunks: string[] = [];
        const interop = Object.create(CoreInterop.prototype) as any;
        interop.addon = {
            executeCommandStreaming: async (_command: string, _dataJson: string, callback: (chunk: string) => void) => {
                callback('50');
                callback('60');
                return 'ok';
            }
        };

        let caught: unknown;
        try {
            await CoreInterop.prototype.executeCommandStreaming.call(
                interop,
                'download_model',
                undefined,
                (chunk: string) => {
                    chunks.push(chunk);
                    controller.abort();
                },
                controller.signal
            );
        } catch (error) {
            caught = error;
        }

        expect(chunks).to.deep.equal(['50']);
        expect(caught).to.be.instanceOf(Error);
        expect((caught as Error).name).to.equal('AbortError');
    });

    it('executeCommandStreaming should not reject when signal aborts after the final observed callback', async function() {
        const controller = new AbortController();
        const interop = Object.create(CoreInterop.prototype) as any;
        interop.addon = {
            executeCommandStreaming: async (_command: string, _dataJson: string, callback: (chunk: string) => void) => {
                callback('100');
                return 'ok';
            }
        };

        const result = await CoreInterop.prototype.executeCommandStreaming.call(
            interop,
            'download_model',
            undefined,
            () => controller.abort(),
            controller.signal
        );

        expect(result).to.equal('ok');
    });

    it('executeCommandStreaming should cancel and release a native cancellation context', async function() {
        const controller = new AbortController();
        const calls: string[] = [];
        const interop = Object.create(CoreInterop.prototype) as any;
        interop.addon = {
            hasCancellableCommands: () => true,
            createCancellationContext: () => {
                calls.push('create');
                return 7;
            },
            cancelCancellationContext: (id: number) => {
                calls.push(`cancel:${id}`);
            },
            releaseCancellationContext: (id: number) => {
                calls.push(`release:${id}`);
            },
            executeCommandStreaming: async (
                _command: string,
                _dataJson: string,
                _callback: (chunk: string) => void,
                contextId?: number
            ) => {
                calls.push(`execute:${contextId}`);
                controller.abort();
                throw new Error("Command 'chat_completions' failed: Operation was cancelled by user");
            }
        };

        let caught: unknown;
        try {
            await CoreInterop.prototype.executeCommandStreaming.call(
                interop,
                'chat_completions',
                undefined,
                () => {},
                controller.signal
            );
        } catch (error) {
            caught = error;
        }

        expect(calls).to.deep.equal(['create', 'execute:7', 'cancel:7', 'release:7']);
        expect(caught).to.be.instanceOf(Error);
        expect((caught as Error).name).to.equal('AbortError');
    });
});
