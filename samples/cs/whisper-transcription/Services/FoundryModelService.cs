using Microsoft.AI.Foundry.Local;
using Microsoft.Extensions.Options;

namespace WhisperTranscription;

public class FoundryModelService
{
    private readonly ILogger<FoundryModelService> _logger;
    private readonly ILoggerFactory _loggerFactory;
    private readonly FoundryOptions _options;
    private readonly SemaphoreSlim _initLock = new(1, 1);
    private bool _initialized;

    public FoundryModelService(
        IOptions<FoundryOptions> options,
        ILogger<FoundryModelService> logger,
        ILoggerFactory loggerFactory)
    {
        _logger = logger;
        _loggerFactory = loggerFactory;
        _options = options.Value;
    }

    public async Task InitializeAsync()
    {
        if (_initialized) return;

        await _initLock.WaitAsync();
        try
        {
            if (_initialized) return;

            _logger.LogInformation("Initializing Foundry Local Manager");
            var config = new Configuration
            {
                AppName = "WhisperTranscription",
                LogLevel = Enum.TryParse<Microsoft.AI.Foundry.Local.LogLevel>(
                    _options.LogLevel, true, out var lvl)
                    ? lvl
                    : Microsoft.AI.Foundry.Local.LogLevel.Information,
            };

            await FoundryLocalManager.CreateAsync(config, _loggerFactory.CreateLogger("FoundryLocal"));
            var mgr = FoundryLocalManager.Instance;
            await mgr.EnsureEpsDownloadedAsync();
            _initialized = true;
        }
        finally
        {
            _initLock.Release();
        }
    }

    public async Task<Model> GetModelAsync(string? aliasOrId = null)
    {
        await InitializeAsync();
        var mgr = FoundryLocalManager.Instance;
        var catalog = await mgr.GetCatalogAsync()
            ?? throw new InvalidOperationException("Failed to get model catalog");

        var alias = string.IsNullOrWhiteSpace(aliasOrId) ? _options.ModelAlias : aliasOrId;
        var model = await catalog.GetModelAsync(alias)
            ?? throw new InvalidOperationException($"Model '{alias}' not found in catalog");

        return model;
    }

    public async Task EnsureModelReadyAsync(Model model)
    {
        // Prefer CPU variant
        var cpuVariant = model.Variants.FirstOrDefault(
            v => v.Info.Runtime?.DeviceType == DeviceType.CPU);
        if (cpuVariant != null)
        {
            model.SelectVariant(cpuVariant);
        }

        // Check cache and download if needed
        if (!await model.IsCachedAsync())
        {
            _logger.LogInformation("Model \"{ModelId}\" not cached — downloading...", model.Id);
            var lastLoggedBucket = -1;
            await model.DownloadAsync(progress =>
            {
                var bucket = (int)Math.Floor(progress / 10);
                if (bucket > lastLoggedBucket)
                {
                    lastLoggedBucket = bucket;
                    _logger.LogInformation("Download progress: {Progress:F0}%", progress);
                }
            });
            _logger.LogInformation("Model downloaded");
        }
        else
        {
            _logger.LogInformation("Model \"{ModelId}\" already cached", model.Id);
        }

        _logger.LogInformation("Loading model \"{ModelId}\"...", model.Id);
        await model.LoadAsync();
        _logger.LogInformation("Model loaded and ready");
    }
}
