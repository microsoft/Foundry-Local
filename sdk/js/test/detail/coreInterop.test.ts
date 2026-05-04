import { describe, it } from 'mocha';
import { expect } from 'chai';
import { CoreInterop } from '../../src/detail/coreInterop.js';

describe('CoreInterop Tests', () => {
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
});
