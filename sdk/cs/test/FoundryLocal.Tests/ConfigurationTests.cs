// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Collections.Generic;

internal sealed class ConfigurationTests
{
    [Test]
    public async Task AsDictionary_RequiredAppName_IsIncluded()
    {
        var config = new Configuration { AppName = "TestApp" };
        var dict = config.AsDictionary();

        await Assert.That(dict["AppName"]).IsEqualTo("TestApp");
    }

    [Test]
    public async Task AsDictionary_DefaultLogLevel_IsWarning()
    {
        var config = new Configuration { AppName = "TestApp" };
        var dict = config.AsDictionary();

        await Assert.That(dict["LogLevel"]).IsEqualTo("Warning");
    }

    [Test]
    public async Task AsDictionary_CustomLogLevel_IsMapped()
    {
        var config = new Configuration { AppName = "TestApp", LogLevel = LogLevel.Debug };
        var dict = config.AsDictionary();

        await Assert.That(dict["LogLevel"]).IsEqualTo("Debug");
    }

    [Test]
    public async Task AsDictionary_NullAppName_Throws()
    {
        var config = new Configuration { AppName = null! };

        await Assert.That(() => config.AsDictionary()).Throws<FoundryLocalException>();
    }

    [Test]
    public async Task AsDictionary_EmptyAppName_Throws()
    {
        var config = new Configuration { AppName = string.Empty };

        await Assert.That(() => config.AsDictionary()).Throws<FoundryLocalException>();
    }

    [Test]
    public async Task AsDictionary_OptionalModelCacheDir_IncludedWhenSet()
    {
        var config = new Configuration { AppName = "TestApp", ModelCacheDir = "/tmp/models" };
        var dict = config.AsDictionary();

        await Assert.That(dict).ContainsKey("ModelCacheDir");
        await Assert.That(dict["ModelCacheDir"]).IsEqualTo("/tmp/models");
    }

    [Test]
    public async Task AsDictionary_OptionalModelCacheDir_OmittedWhenNull()
    {
        var config = new Configuration { AppName = "TestApp" };
        var dict = config.AsDictionary();

        await Assert.That(dict.ContainsKey("ModelCacheDir")).IsFalse();
    }

    [Test]
    public async Task AsDictionary_OptionalLogsDir_IncludedWhenSet()
    {
        var config = new Configuration { AppName = "TestApp", LogsDir = "/tmp/logs" };
        var dict = config.AsDictionary();

        await Assert.That(dict["LogsDir"]).IsEqualTo("/tmp/logs");
    }

    [Test]
    public async Task AsDictionary_OptionalAppDataDir_IncludedWhenSet()
    {
        var config = new Configuration { AppName = "TestApp", AppDataDir = "/tmp/appdata" };
        var dict = config.AsDictionary();

        await Assert.That(dict["AppDataDir"]).IsEqualTo("/tmp/appdata");
    }

    [Test]
    public async Task AsDictionary_WebServiceUrls_IncludedWhenSet()
    {
        var config = new Configuration
        {
            AppName = "TestApp",
            Web = new Configuration.WebService { Urls = "http://localhost:5000" }
        };
        var dict = config.AsDictionary();

        await Assert.That(dict["WebServiceUrls"]).IsEqualTo("http://localhost:5000");
    }

    [Test]
    public async Task AsDictionary_WebServiceUrls_OmittedWhenNull()
    {
        var config = new Configuration
        {
            AppName = "TestApp",
            Web = new Configuration.WebService()
        };
        var dict = config.AsDictionary();

        await Assert.That(dict.ContainsKey("WebServiceUrls")).IsFalse();
    }

    [Test]
    public async Task AsDictionary_AdditionalSettings_AreMerged()
    {
        var config = new Configuration
        {
            AppName = "TestApp",
            AdditionalSettings = new Dictionary<string, string>
            {
                { "CustomKey", "CustomValue" },
                { "AnotherKey", "AnotherValue" }
            }
        };
        var dict = config.AsDictionary();

        await Assert.That(dict["CustomKey"]).IsEqualTo("CustomValue");
        await Assert.That(dict["AnotherKey"]).IsEqualTo("AnotherValue");
    }

    [Test]
    public async Task AsDictionary_AdditionalSettings_EmptyKeysSkipped()
    {
        var config = new Configuration
        {
            AppName = "TestApp",
            AdditionalSettings = new Dictionary<string, string>
            {
                { "ValidKey", "Value" }
            }
        };
        var dict = config.AsDictionary();

        // Should have AppName, LogLevel, ValidKey — no empty keys
        await Assert.That(dict).HasCount().EqualTo(3);
    }

    [Test]
    public async Task AsDictionary_AdditionalSettings_CanOverrideBuiltInKeys()
    {
        var config = new Configuration
        {
            AppName = "TestApp",
            AdditionalSettings = new Dictionary<string, string>
            {
                { "LogLevel", "OverriddenValue" }
            }
        };
        var dict = config.AsDictionary();

        // AdditionalSettings uses indexer so it overwrites
        await Assert.That(dict["LogLevel"]).IsEqualTo("OverriddenValue");
    }

    [Test]
    public async Task Validate_ValidConfig_DoesNotThrow()
    {
        var config = new Configuration { AppName = "TestApp" };

        // Should complete without throwing
        await Assert.That(() => config.Validate()).ThrowsNothing();
    }

    [Test]
    public async Task Validate_EmptyAppName_Throws()
    {
        var config = new Configuration { AppName = string.Empty };

        await Assert.That(() => config.Validate()).Throws<ArgumentException>();
    }

    [Test]
    public async Task Validate_InvalidCharsInAppName_Throws()
    {
        var config = new Configuration { AppName = "invalid/name" };

        await Assert.That(() => config.Validate()).Throws<ArgumentException>();
    }
}
