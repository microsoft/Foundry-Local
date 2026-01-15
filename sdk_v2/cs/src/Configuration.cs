// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local;

public class Configuration
{
    /// <summary>
    /// Your application name. MUST be set to a valid name.
    /// </summary>
    public required string AppName { get; set; }

    /// <summary>
    /// Application data directory.
    /// Default: {home}/.{appname}, where {home} is the user's home directory and {appname} is the AppName value.
    /// </summary>
    public string? AppDataDir { get; init; }

    /// <summary>
    /// Model cache directory.
    /// Default: {appdata}/cache/models, where {appdata} is the AppDataDir value.
    /// </summary>
    public string? ModelCacheDir { get; init; }

    /// <summary>
    /// Log directory.
    /// Default: {appdata}/logs
    /// </summary>
    public string? LogsDir { get; init; }

    /// <summary>
    /// Logging level.
    /// Valid values are: Verbose, Debug, Information, Warning, Error, Fatal.
    /// Default: LogLevel.Warning
    /// </summary>
    public LogLevel LogLevel { get; init; } = LogLevel.Warning;

    /// <summary>
    /// Enable manual execution provider download mode. Only meaningful if using WinML.
    /// 
    /// Default: false
    /// 
    /// When false, EPs are downloaded automatically in the background when FoundryLocalManager is created.
    /// When true, EPs are downloaded when FoundryLocalManager.EnsureEpsDownloadedAsync or GetCatalogAsync are called.
    ///
    /// Once an EP is downloaded it will not be re-downloaded unless a new version is available.
    /// </summary>
    // DISABLED: We want to make sure this is required before making it public as supporting this complicates the
    //           Core implementation. Can be specified via AdditionalSettings if needed for testing.
    // public bool ManualEpDownload { get; init; } 

    /// <summary>
    /// Optional configuration for the built-in web service.
    /// NOTE: This is not included in all builds.
    /// </summary>
    public WebService? Web { get; init; }

    /// <summary>
    /// Additional settings that Foundry Local Core can consume.
    /// Keys and values are strings.
    /// </summary>
    public IDictionary<string, string>? AdditionalSettings { get; init; }

    /// <summary>
    /// Configuration settings if the optional web service is used.
    /// </summary>
    public class WebService
    {
        /// <summary>
        /// Url/s to bind to the web service when <see cref="FoundryLocalManager.StartWebServiceAsync"/> is called.
        /// After startup, <see cref="FoundryLocalManager.Urls"/> will contain the actual URL/s the service is listening on.
        /// 
        /// Default: 127.0.0.1:0, which binds to a random ephemeral port.
        /// Multiple URLs can be specified as a semi-colon separated list.
        /// </summary>
        public string? Urls { get; init; }

        /// <summary>
        /// If the web service is running in a separate process, it will be accessed using this URI.
        /// </summary>
        /// <remarks>
        /// Both processes should be using the same version of the SDK. If a random port is assigned when creating
        /// the web service in the external process the actual port must be provided here.
        /// </remarks> 
        public Uri? ExternalUrl { get; init; }
    }

    internal void Validate()
    {
        if (string.IsNullOrEmpty(AppName))
        {
            throw new ArgumentException("Configuration AppName must be set to a valid application name.");
        }

        if (AppName.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
        {
            throw new ArgumentException("Configuration AppName value contains invalid characters.");
        }


        if (Web?.ExternalUrl?.Port == 0)
        {
            throw new ArgumentException("Configuration Web.ExternalUrl has invalid port of 0.");
        }
    }

    internal Dictionary<string, string> AsDictionary()
    {
        if (string.IsNullOrEmpty(AppName))
        {
            throw new FoundryLocalException(
                "Configuration AppName must be set to a valid application name.");
        }

        var configValues = new Dictionary<string, string>
        {
            { "AppName", AppName },
            { "LogLevel", LogLevel.ToString() }
        };

        if (!string.IsNullOrEmpty(AppDataDir))
        {
            configValues.Add("AppDataDir", AppDataDir);
        }

        if (!string.IsNullOrEmpty(ModelCacheDir))
        {
            configValues.Add("ModelCacheDir", ModelCacheDir);
        }

        if (!string.IsNullOrEmpty(LogsDir))
        {
            configValues.Add("LogsDir", LogsDir);
        }

        //configValues.Add("ManualEpDownload", ManualEpDownload.ToString());

        if (Web != null)
        {
            if (Web.Urls != null)
            {
                configValues["WebServiceUrls"] = Web.Urls;
            }
        }

        // Emit any additional settings.
        if (AdditionalSettings != null)
        {
            foreach (var kvp in AdditionalSettings)
            {
                if (string.IsNullOrEmpty(kvp.Key))
                {
                    continue; // skip empty keys
                }
                configValues[kvp.Key] = kvp.Value ?? string.Empty;
            }
        }

        return configValues;
    }
}
