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
});
