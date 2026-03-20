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

using Microsoft.VisualStudio.TestPlatform.TestHost;

using Moq;

internal static class Utils
{
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

        ILogger logger = loggerFactory.CreateLogger<Program>();

        // Read configuration from appsettings.Test.json
        logger.LogDebug("Reading configuration from appsettings.Test.json");
        var configuration = new ConfigurationBuilder()
            .SetBasePath(Directory.GetCurrentDirectory())
            .AddJsonFile("appsettings.Test.json", optional: true, reloadOnChange: false)
            .Build();

        var testModelCacheDirName = "test-data-shared";
        string testDataSharedPath;
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

        logger.LogInformation("Using test model cache directory: {testDataSharedPath}", testDataSharedPath);

        if (!Directory.Exists(testDataSharedPath))
        {
            throw new DirectoryNotFoundException($"Test model cache directory does not exist: {testDataSharedPath}");

        }

        var config = new Configuration
        {
            AppName = "FoundryLocalSdkTest",
            LogLevel = Local.LogLevel.Debug,
            Web = new Configuration.WebService
            {
                Urls = "http://127.0.0.1:0"
            },
            ModelCacheDir = testDataSharedPath,
            LogsDir = Path.Combine(GetRepoRoot(), "sdk_v2", "cs", "logs")
        };

        // Initialize the singleton instance.
        FoundryLocalManager.CreateAsync(config, logger).GetAwaiter().GetResult();

        // standalone instance for testing individual components that skips the 'initialize' command
        CoreInterop = new CoreInterop(logger);        
    }

    internal static ICoreInterop CoreInterop { get; private set; } = default!;

    internal static Mock<ILogger> CreateCapturingLoggerMock(List<string> sink)
    {
        var mock = new Mock<ILogger>();
        mock.Setup(x => x.Log(
                It.IsAny<LogLevel>(),
                It.IsAny<EventId>(),
                It.IsAny<It.IsAnyType>(),
                It.IsAny<Exception?>(),
                (Func<It.IsAnyType, Exception?, string>)It.IsAny<object>()))
            .Callback((LogLevel level, EventId id, object state, Exception? ex, Delegate formatter) =>
            {
                var message = formatter.DynamicInvoke(state, ex) as string;
                sink.Add($"{level}: {message}");
            });

        return mock;
    }

    internal sealed record InteropCommandInterceptInfo
    {
        public string CommandName { get; init; } = default!;
        public string? CommandInput { get; init; }
        public string ResponseData { get; init; } = default!;
        public string? ResponseError { get; init; }
    }

    internal static Mock<ICoreInterop> CreateCoreInteropWithIntercept(ICoreInterop coreInterop,
                                                                      List<InteropCommandInterceptInfo> intercepts)
    {
        var mock = new Mock<ICoreInterop>();
        var interceptNames = new HashSet<string>(StringComparer.InvariantCulture);

        foreach (var intercept in intercepts)
        {
            if (!interceptNames.Add(intercept.CommandName))
            {
                throw new ArgumentException($"Duplicate intercept for command {intercept.CommandName}");
            }

            mock.Setup(x => x.ExecuteCommand(It.Is<string>(s => s == intercept.CommandName), It.IsAny<CoreInteropRequest?>()))
                .Returns(new ICoreInterop.Response
                {
                    Data = intercept.ResponseData,
                    Error = intercept.ResponseError
                });

            mock.Setup(x => x.ExecuteCommandAsync(It.Is<string>(s => s == intercept.CommandName),
                                                  It.IsAny<CoreInteropRequest?>(),
                                                  It.IsAny<CancellationToken?>()))
                .ReturnsAsync(new ICoreInterop.Response
                {
                    Data = intercept.ResponseData,
                    Error = intercept.ResponseError
                });
        }

        mock.Setup(x => x.ExecuteCommand(It.Is<string>(s => !interceptNames.Contains(s)),
                                         It.IsAny<CoreInteropRequest?>()))
            .Returns((string commandName, CoreInteropRequest? commandInput) =>
                        coreInterop.ExecuteCommand(commandName, commandInput));

        mock.Setup(x => x.ExecuteCommandAsync(It.Is<string>(s => !interceptNames.Contains(s)),
                                              It.IsAny<CoreInteropRequest?>(),
                                              It.IsAny<CancellationToken?>()))
            .Returns((string commandName, CoreInteropRequest? commandInput, CancellationToken? ct) =>
                coreInterop.ExecuteCommandAsync(commandName, commandInput, ct));

        return mock;
    }

    internal static bool IsRunningInCI()
    {
        var azureDevOps = Environment.GetEnvironmentVariable("TF_BUILD");
        var githubActions = Environment.GetEnvironmentVariable("GITHUB_ACTIONS");
        var isCI = string.Equals(azureDevOps, "True", StringComparison.OrdinalIgnoreCase) ||
                   string.Equals(githubActions, "true", StringComparison.OrdinalIgnoreCase);

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
