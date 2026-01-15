# Foundry Local JS SDK Tests

This directory contains the test suite for the Foundry Local JS SDK. The tests use `mocha` as the test runner and `chai` for assertions.

## Running Tests

To run all tests:

```bash
npm install && npm test
```

To run a specific test file:

```bash
npx mocha --import=tsx test/model.test.ts
```

## Adding Local Model Tests

To add tests that require specific local models:

1.  Ensure the model is available in your configured model cache and set the `modelCacheDir` configuration option.
2.  Use the `TEST_MODEL_ALIAS` constant in `testUtils.ts` or define your own alias.
3.  In your test, use `catalog.getCachedModels()` to verify the model is available before attempting to load it.

Example:

```typescript
it('should run inference on my-model', async function() {
    const manager = getTestManager();
    const catalog = manager.getCatalog();
    const model = catalog.getModel('my-model');
    
    if (!model) this.skip(); // Skip if model not found
    
    await model.load();
    // ... run inference ...
    await model.unload();
});
```
