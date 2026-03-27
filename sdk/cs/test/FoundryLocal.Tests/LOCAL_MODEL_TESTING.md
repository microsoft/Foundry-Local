# Running Local Model Tests

## Configuration

The test model cache directory name is configured in `sdk/cs/test/FoundryLocal.Tests/appsettings.Test.json`:

```json
{
  "TestModelCacheDirName": "test-data-shared"
}
```

If the name is a relative path it will be resolved as <repository-root>/../{TestModelCacheDirName}.
If the name is an absolute path it will be used as-is.

## Run the tests

The tests will automatically find the models in the configured test model cache directory.

```bash
cd /path/to/parent-dir/foundry-local-sdk/sdk/cs/test/FoundryLocal.Tests
dotnet test Microsoft.AI.Foundry.Local.Tests.csproj --configuration Release# Running Local Model Tests

## Configuration

The test model cache directory name is configured in `sdk/cs/test/FoundryLocal.Tests/appsettings.Test.json`:

```json
{
  "TestModelCacheDirName": "/path/to/model/cache"
}
```

## Run the tests

The tests will automatically find the models in the configured test model cache directory.

```bash
cd /path/to/parent-dir/foundry-local-sdk/sdk/cs/test/FoundryLocal.Tests
dotnet test Microsoft.AI.Foundry.Local.Tests.csproj --configuration Release
