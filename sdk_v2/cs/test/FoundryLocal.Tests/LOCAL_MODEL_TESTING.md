# Running Local Model Tests

## Configuration

The test model cache directory name is configured in `sdk_v2/cs/test/FoundryLocal.Tests/appsettings.Test.json`:

```json
{
  "TestModelCacheDirName": "/path/to/model/cache"
}
```

## Run the tests

The tests will automatically find the models in the configured test model cache directory.

```bash
cd /path/to/parent-dir/foundry-local-sdk/sdk_v2/cs/test/FoundryLocal.Tests
dotnet test Microsoft.AI.Foundry.Local.Tests.csproj --configuration Release# Running Local Model Tests

## Configuration

The test model cache directory name is configured in `sdk_v2/cs/test/FoundryLocal.Tests/appsettings.Test.json`:

```json
{
  "TestModelCacheDirName": "/path/to/model/cache"
}
```

## Run the tests

The tests will automatically find the models in the configured test model cache directory.

```bash
cd /path/to/parent-dir/foundry-local-sdk/sdk_v2/cs/test/FoundryLocal.Tests
dotnet test Microsoft.AI.Foundry.Local.Tests.csproj --configuration Release