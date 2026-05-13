// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Text.Json;

using Microsoft.AI.Foundry.Local.Detail;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging;

#pragma warning disable CS0618 // Test helpers exercise PromptTemplate/ModelSettings which are obsolete but still supported.

internal static class Utils
{
    /// <summary>
    /// Resolves a path under the test project's <c>testdata/</c> output directory.
    /// </summary>
    internal static string TestDataPath(string filename) =>
        Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "testdata", filename));

    internal struct TestCatalogInfo
    {
        internal readonly List<ModelInfo> TestCatalog { get; }
        internal readonly string ModelListJson { get; }

        internal TestCatalogInfo(bool includeCuda)
        {

            TestCatalog = Utils.BuildTestCatalog(includeCuda);
            ModelListJson = JsonSerializer.Serialize(TestCatalog, JsonSerializationContext.Default.ListModelInfo);
        }
    }

    internal static readonly TestCatalogInfo TestCatalog = new(true);

    [Before(Assembly)]
    public static void AssemblyInit(AssemblyHookContext _)
    {
        using var loggerFactory = LoggerFactory.Create(builder =>
        {
            builder
                .AddConsole()
                .SetMinimumLevel(LogLevel.Debug);
        });

        ILogger logger = loggerFactory.CreateLogger("FoundryLocalSdkTest");

        // Read configuration from appsettings.Test.json
        logger.LogDebug("Reading configuration from appsettings.Test.json");
        var configuration = new ConfigurationBuilder()
            .SetBasePath(Directory.GetCurrentDirectory())
            .AddJsonFile("appsettings.Test.json", optional: true, reloadOnChange: false)
            .Build();

        // Prefer the TEST_MODEL_CACHE_DIR env var when set (used by CI). If it points at
        // an existing directory we treat it as an absolute path. Otherwise fall back to
        // the appsettings.Test.json TestModelCacheDirName logic for inner-loop / VS use.
        var envCacheDir = Environment.GetEnvironmentVariable("TEST_MODEL_CACHE_DIR");
        string testDataSharedPath;
        if (!string.IsNullOrWhiteSpace(envCacheDir) && Directory.Exists(envCacheDir))
        {
            testDataSharedPath = Path.GetFullPath(envCacheDir);
            logger.LogInformation(
                "Using test model cache directory from TEST_MODEL_CACHE_DIR env var: {TestDataSharedPath}",
                testDataSharedPath);
        }
        else
        {
            if (!string.IsNullOrWhiteSpace(envCacheDir))
            {
                logger.LogWarning(
                    "TEST_MODEL_CACHE_DIR is set to '{EnvCacheDir}' but the directory does not exist; falling back to appsettings.Test.json.",
                    envCacheDir);
            }

            var testModelCacheDirName = configuration["TestModelCacheDirName"] ?? "test-data-shared";
            if (Path.IsPathRooted(testModelCacheDirName) ||
                testModelCacheDirName.Contains(Path.DirectorySeparatorChar) ||
                testModelCacheDirName.Contains(Path.AltDirectorySeparatorChar))
            {
                // It's a relative or complete filepath, resolve from current directory
                testDataSharedPath = Path.GetFullPath(testModelCacheDirName);
            }
            else
            {
                // It's just a directory name, combine with repo root parent
                testDataSharedPath = Path.GetFullPath(Path.Combine(GetRepoRoot(), "..", testModelCacheDirName));
            }

            logger.LogInformation(
                "Using test model cache directory from appsettings.Test.json: {TestDataSharedPath}",
                testDataSharedPath);
        }

        if (!Directory.Exists(testDataSharedPath))
        {
            // need to ensure there's a user visible error when running in VS.
            logger.LogCritical($"Test model cache directory does not exist: {testDataSharedPath}");
            Console.Error.WriteLine($"[AssemblyInit] Test model cache directory does not exist: {testDataSharedPath}");
            throw new DirectoryNotFoundException($"Test model cache directory does not exist: {testDataSharedPath}");

        }

        // Echo to stdout as well so CI test-output capture (which doesn't always
        // forward the ILogger console sink from an assembly-hook context) records
        // exactly which path we resolved. Critical when diagnosing initialization
        // failures from CI logs only.
        Console.WriteLine($"[AssemblyInit] TEST_MODEL_CACHE_DIR env: '{envCacheDir}'");
        Console.WriteLine($"[AssemblyInit] Resolved test model cache: '{testDataSharedPath}'");

        var config = new Configuration
        {
            AppName = "FoundryLocalSdkTest",
            LogLevel = Local.LogLevel.Debug,
            Web = new Configuration.WebService
            {
                Urls = "http://127.0.0.1:0"
            },
            ModelCacheDir = testDataSharedPath,
            LogsDir = Path.Combine(GetRepoRoot(), "sdk_v2", "cs", "logs"),
            // Leave as default. Currently that's a static catalog.
            //CatalogUrls = new List<(string Url, string? Filter)>
            //{
            //    ("https://ai.azure.com/api/eastus/ux/v1.0", null)
            //}
        };

        // Initialize the singleton instance.
        // Wrapped in Task.Run so the async work runs on a thread-pool thread with no captured
        // SynchronizationContext. Calling GetAwaiter().GetResult() directly here would deadlock
        // if any test runner or hosting context installs a single-threaded sync context (the
        // continuation would wait for this thread, which is blocked waiting for the result).
        try
        {
            Task.Run(() => FoundryLocalManager.CreateAsync(config, logger)).GetAwaiter().GetResult();
        }
        catch (Exception ex)
        {
            // Surface the full exception detail to stdout/stderr so CI captures it. The
            // BeforeAssemblyException wrapper only echoes the top-level message, which on
            // FoundryLocalException is just "Error during initialization" — useless without
            // the inner native error / stack.
            Console.Error.WriteLine($"[AssemblyInit] FoundryLocalManager.CreateAsync failed: {ex}");
            throw;
        }
    }

    internal static bool IsRunningInCI()
    {
        var azureDevOps = Environment.GetEnvironmentVariable("TF_BUILD");
        var githubActions = Environment.GetEnvironmentVariable("GITHUB_ACTIONS");
        var ci = Environment.GetEnvironmentVariable("CI");
        var isCI = string.Equals(azureDevOps, "True", StringComparison.OrdinalIgnoreCase) ||
                   string.Equals(githubActions, "true", StringComparison.OrdinalIgnoreCase) ||
                   string.Equals(ci, "true", StringComparison.OrdinalIgnoreCase) ||
                   string.Equals(ci, "1", StringComparison.OrdinalIgnoreCase);

        return isCI;
    }

    private static List<ModelInfo> BuildTestCatalog(bool includeCuda = true)
    {
        // Mirrors MOCK_CATALOG_DATA ordering and fields (Python tests)
        var common = new
        {
            ProviderType = "AzureFoundry",
            Version = 1,
            ModelType = "ONNX",
            PromptTemplate = (PromptTemplate?)null,
            Publisher = "Microsoft",
            Task = "chat-completion",
            FileSizeMb = 10403,
            ModelSettings = new ModelSettings { Parameters = [] },
            SupportsToolCalling = false,
            License = "MIT",
            LicenseDescription = "License…",
            MaxOutputTokens = 1024L,
            MinFLVersion = "1.0.0",
        };

        var list = new List<ModelInfo>
            {
                // model-1 generic-gpu, generic-cpu:2, generic-cpu:1
                new()
                {
                    Id = "model-1-generic-gpu:1",
                    Name = "model-1-generic-gpu",
                    DisplayName = "model-1-generic-gpu",
                    Uri = "azureml://registries/azureml/models/model-1-generic-gpu/versions/1",
                    Runtime = new Runtime { DeviceType = DeviceType.GPU, ExecutionProvider = "WebGpuExecutionProvider" },
                    Alias = "model-1",
                    // ParentModelUri = "azureml://registries/azureml/models/model-1/versions/1",
                    ProviderType = common.ProviderType, Version = common.Version, ModelType = common.ModelType,
                    PromptTemplate = common.PromptTemplate, Publisher = common.Publisher, Task = common.Task,
                    FileSizeMb = common.FileSizeMb, ModelSettings = common.ModelSettings,
                    SupportsToolCalling = common.SupportsToolCalling, License = common.License,
                    LicenseDescription = common.LicenseDescription, MaxOutputTokens = common.MaxOutputTokens,
                    MinFLVersion = common.MinFLVersion
                },
                new()
                {
                    Id = "model-1-generic-cpu:2",
                    Name = "model-1-generic-cpu",
                    DisplayName = "model-1-generic-cpu",
                    Uri = "azureml://registries/azureml/models/model-1-generic-cpu/versions/2",
                    Runtime = new Runtime { DeviceType = DeviceType.CPU, ExecutionProvider = "CPUExecutionProvider" },
                    Alias = "model-1",
                    // ParentModelUri = "azureml://registries/azureml/models/model-1/versions/2",
                    ProviderType = common.ProviderType,
                    Version = common.Version, ModelType = common.ModelType,
                    PromptTemplate = common.PromptTemplate,
                    Publisher = common.Publisher, Task = common.Task,
                    FileSizeMb = common.FileSizeMb - 10,  // smaller so default chosen in test that sorts on this
                    ModelSettings = common.ModelSettings,
                    SupportsToolCalling = common.SupportsToolCalling,
                    License = common.License,
                    LicenseDescription = common.LicenseDescription,
                    MaxOutputTokens = common.MaxOutputTokens,
                    MinFLVersion = common.MinFLVersion
                },
                new()
                {
                    Id = "model-1-generic-cpu:1",
                    Name = "model-1-generic-cpu",
                    DisplayName = "model-1-generic-cpu",
                    Uri = "azureml://registries/azureml/models/model-1-generic-cpu/versions/1",
                    Runtime = new Runtime { DeviceType = DeviceType.CPU, ExecutionProvider = "CPUExecutionProvider" },
                    Alias = "model-1",
                    //ParentModelUri = "azureml://registries/azureml/models/model-1/versions/1",
                    ProviderType = common.ProviderType,
                    Version = common.Version,
                    ModelType = common.ModelType,
                    PromptTemplate = common.PromptTemplate,
                    Publisher = common.Publisher, Task = common.Task,
                    FileSizeMb = common.FileSizeMb,
                    ModelSettings = common.ModelSettings,
                    SupportsToolCalling = common.SupportsToolCalling,
                    License = common.License,
                    LicenseDescription = common.LicenseDescription,
                    MaxOutputTokens = common.MaxOutputTokens,
                    MinFLVersion = common.MinFLVersion
                },

                // model-2 npu:2, npu:1, generic-cpu:1
                new()
                {
                    Id = "model-2-npu:2",
                    Name = "model-2-npu",
                    DisplayName = "model-2-npu",
                    Uri = "azureml://registries/azureml/models/model-2-npu/versions/2",
                    Runtime = new Runtime { DeviceType = DeviceType.NPU, ExecutionProvider = "QNNExecutionProvider" },
                    Alias = "model-2",
                    //ParentModelUri = "azureml://registries/azureml/models/model-2/versions/2",
                    ProviderType = common.ProviderType,
                    Version = common.Version, ModelType = common.ModelType,
                    PromptTemplate = common.PromptTemplate,
                    Publisher = common.Publisher, Task = common.Task,
                    FileSizeMb = common.FileSizeMb,
                    ModelSettings = common.ModelSettings,
                    SupportsToolCalling = common.SupportsToolCalling,
                    License = common.License,
                    LicenseDescription = common.LicenseDescription,
                    MaxOutputTokens = common.MaxOutputTokens,
                    MinFLVersion = common.MinFLVersion
                },
                new()
                {
                    Id = "model-2-npu:1",
                    Name = "model-2-npu",
                    DisplayName = "model-2-npu",
                    Uri = "azureml://registries/azureml/models/model-2-npu/versions/1",
                    Runtime = new Runtime { DeviceType = DeviceType.NPU, ExecutionProvider = "QNNExecutionProvider" },
                    Alias = "model-2",
                    //ParentModelUri = "azureml://registries/azureml/models/model-2/versions/1",
                    ProviderType = common.ProviderType,
                    Version = common.Version, ModelType = common.ModelType,
                    PromptTemplate = common.PromptTemplate,
                    Publisher = common.Publisher, Task = common.Task,
                    FileSizeMb = common.FileSizeMb,
                    ModelSettings = common.ModelSettings,
                    SupportsToolCalling = common.SupportsToolCalling,
                    License = common.License,
                    LicenseDescription = common.LicenseDescription,
                    MaxOutputTokens = common.MaxOutputTokens,
                    MinFLVersion = common.MinFLVersion
                },
                new()
                {
                    Id = "model-2-generic-cpu:1",
                    Name = "model-2-generic-cpu",
                    DisplayName = "model-2-generic-cpu",
                    Uri = "azureml://registries/azureml/models/model-2-generic-cpu/versions/1",
                    Runtime = new Runtime { DeviceType = DeviceType.CPU, ExecutionProvider = "CPUExecutionProvider" },
                    Alias = "model-2",
                    //ParentModelUri = "azureml://registries/azureml/models/model-2/versions/1",
                    ProviderType = common.ProviderType,
                    Version = common.Version, ModelType = common.ModelType,
                    PromptTemplate = common.PromptTemplate,
                    Publisher = common.Publisher, Task = common.Task,
                    FileSizeMb = common.FileSizeMb,
                    ModelSettings = common.ModelSettings,
                    SupportsToolCalling = common.SupportsToolCalling,
                    License = common.License,
                    LicenseDescription = common.LicenseDescription,
                    MaxOutputTokens = common.MaxOutputTokens,
                    MinFLVersion = common.MinFLVersion
                },
            };

        // model-3 cuda-gpu (optional), generic-gpu, generic-cpu
        if (includeCuda)
        {
            list.Add(new ModelInfo
            {
                Id = "model-3-cuda-gpu:1",
                Name = "model-3-cuda-gpu",
                DisplayName = "model-3-cuda-gpu",
                Uri = "azureml://registries/azureml/models/model-3-cuda-gpu/versions/1",
                Runtime = new Runtime { DeviceType = DeviceType.GPU, ExecutionProvider = "CUDAExecutionProvider" },
                Alias = "model-3",
                //ParentModelUri = "azureml://registries/azureml/models/model-3/versions/1",
                ProviderType = common.ProviderType,
                Version = common.Version,
                ModelType = common.ModelType,
                PromptTemplate = common.PromptTemplate,
                Publisher = common.Publisher,
                Task = common.Task,
                FileSizeMb = common.FileSizeMb,
                ModelSettings = common.ModelSettings,
                SupportsToolCalling = common.SupportsToolCalling,
                License = common.License,
                LicenseDescription = common.LicenseDescription,
                MaxOutputTokens = common.MaxOutputTokens,
                MinFLVersion = common.MinFLVersion
            });
        }

        list.AddRange(new[]
        {
                new ModelInfo
                {
                    Id = "model-3-generic-gpu:1",
                    Name = "model-3-generic-gpu",
                    DisplayName = "model-3-generic-gpu",
                    Uri = "azureml://registries/azureml/models/model-3-generic-gpu/versions/1",
                    Runtime = new Runtime { DeviceType = DeviceType.GPU, ExecutionProvider = "WebGpuExecutionProvider" },
                    Alias = "model-3",
                    //ParentModelUri = "azureml://registries/azureml/models/model-3/versions/1",
                    ProviderType = common.ProviderType,
                    Version = common.Version, ModelType = common.ModelType,
                    PromptTemplate = common.PromptTemplate,
                    Publisher = common.Publisher, Task = common.Task,
                    FileSizeMb = common.FileSizeMb,
                    ModelSettings = common.ModelSettings,
                    SupportsToolCalling = common.SupportsToolCalling,
                    License = common.License,
                    LicenseDescription = common.LicenseDescription,
                    MaxOutputTokens = common.MaxOutputTokens,
                    MinFLVersion = common.MinFLVersion
                },
                new ModelInfo
                {
                    Id = "model-3-generic-cpu:1",
                    Name = "model-3-generic-cpu",
                    DisplayName = "model-3-generic-cpu",
                    Uri = "azureml://registries/azureml/models/model-3-generic-cpu/versions/1",
                    Runtime = new Runtime { DeviceType = DeviceType.CPU, ExecutionProvider = "CPUExecutionProvider" },
                    Alias = "model-3",
                    //ParentModelUri = "azureml://registries/azureml/models/model-3/versions/1",
                    ProviderType = common.ProviderType,
                    Version = common.Version,
                    ModelType = common.ModelType,
                    PromptTemplate = common.PromptTemplate,
                    Publisher = common.Publisher, Task = common.Task,
                    FileSizeMb = common.FileSizeMb,
                    ModelSettings = common.ModelSettings,
                    SupportsToolCalling = common.SupportsToolCalling,
                    License = common.License,
                    LicenseDescription = common.LicenseDescription,
                    MaxOutputTokens = common.MaxOutputTokens,
                    MinFLVersion = common.MinFLVersion
                }
            });

        // model-4 generic-gpu (nullable prompt)
        list.Add(new ModelInfo
        {
            Id = "model-4-generic-gpu:1",
            Name = "model-4-generic-gpu",
            DisplayName = "model-4-generic-gpu",
            Uri = "azureml://registries/azureml/models/model-4-generic-gpu/versions/1",
            Runtime = new Runtime { DeviceType = DeviceType.GPU, ExecutionProvider = "WebGpuExecutionProvider" },
            Alias = "model-4",
            //ParentModelUri = "azureml://registries/azureml/models/model-4/versions/1",
            ProviderType = common.ProviderType,
            Version = common.Version,
            ModelType = common.ModelType,
            PromptTemplate = null,
            Publisher = common.Publisher,
            Task = common.Task,
            FileSizeMb = common.FileSizeMb,
            ModelSettings = common.ModelSettings,
            SupportsToolCalling = common.SupportsToolCalling,
            License = common.License,
            LicenseDescription = common.LicenseDescription,
            MaxOutputTokens = common.MaxOutputTokens,
            MinFLVersion = common.MinFLVersion
        });

        return list;
    }

    private static string GetSourceFilePath([CallerFilePath] string path = "") => path;

    // Gets the root directory of the foundry-local-sdk repository by finding the .git directory.
    private static string GetRepoRoot()
    {
        var sourceFile = GetSourceFilePath();
        var dir = new DirectoryInfo(Path.GetDirectoryName(sourceFile)!);

        while (dir != null)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, ".git")))
                return dir.FullName;

            dir = dir.Parent;
        }

        throw new InvalidOperationException("Could not find git repository root from test file location");
    }
}
