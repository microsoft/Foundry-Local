# Foundry Local JS SDK Tests

This directory contains the test suite for the Foundry Local JS SDK. The tests use `mocha` as the test runner and `chai` for assertions.

## Prerequisites

Before running the tests, ensure you have:

1.  **Prepared the Core Libraries**: Run the `download_core_nuget.ps1` script located in `sdk_v2/nuget/`. This script downloads the necessary NuGet packages and extracts the native libraries (dll/so/dylib) into the `sdk_v2/nuget/packages/core/` directory, which the tests are configured to use.
    ```powershell
    ../nuget/download_core_nuget.ps1
    ```
2.  Configured the `modelCacheDir` if you want to run tests against real models in the `FoundryLocalConfig`.

## Running Tests

To run all tests:

```bash
npm test
```

To run a specific test file:

```bash
npx mocha --import=tsx test/model.test.ts
```

## Adding Local Model Tests

To add tests that require specific local models:

1.  Ensure the model is available in `modelCacheDir`.
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
