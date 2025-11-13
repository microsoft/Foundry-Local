# <a id="Microsoft_AI_Foundry_Local"></a> Namespace Microsoft.AI.Foundry.Local

### Classes

 [OpenAIChatClient.ChatSettings](Microsoft.AI.Foundry.Local.OpenAIChatClient.ChatSettings.md)

Settings controlling chat completion generation. Only the subset supported by Foundry Local.

 [Configuration](Microsoft.AI.Foundry.Local.Configuration.md)

Foundry Local configuration used to initialize the <xref href="Microsoft.AI.Foundry.Local.FoundryLocalManager" data-throw-if-not-resolved="false"></xref> singleton.

 [FoundryLocalException](Microsoft.AI.Foundry.Local.FoundryLocalException.md)

Exception type thrown by the Foundry Local SDK to represent operational or initialization errors.

 [FoundryLocalManager](Microsoft.AI.Foundry.Local.FoundryLocalManager.md)

Entry point for Foundry Local SDK providing initialization, catalog access, model management
and optional web service hosting.

 [Model](Microsoft.AI.Foundry.Local.Model.md)

Represents a family of related model variants (versions or configurations) that share a common alias.
Acts as a façade over its variants, letting you:
 - enumerate and select a specific variant
 - prefer a locally cached variant automatically
 - resolve the latest version of a given variant
 - download, load, unload, cache removal for the currently selected variant
 - create chat and audio clients for the currently selected variant.
Use <xref href="Microsoft.AI.Foundry.Local.ModelVariant" data-throw-if-not-resolved="false"></xref> when you need per‑variant metadata; use <xref href="Microsoft.AI.Foundry.Local.Model" data-throw-if-not-resolved="false"></xref> when you want alias‑level orchestration.

 [ModelInfo](Microsoft.AI.Foundry.Local.ModelInfo.md)

Full descriptive metadata for a model variant within the catalog.

 [ModelSettings](Microsoft.AI.Foundry.Local.ModelSettings.md)

Optional settings applied to a model instance (e.g. default parameters).

 [ModelVariant](Microsoft.AI.Foundry.Local.ModelVariant.md)

Represents a single, concrete downloadable model instance (a specific version + configuration) identified
by a unique model Id and grouped under a broader alias shared with other device-specific variants.
Provides:
 - Direct access to catalog metadata via <xref href="Microsoft.AI.Foundry.Local.ModelInfo" data-throw-if-not-resolved="false"></xref>
 - Lifecycle operations (download, load, unload, cache removal)
 - State queries (cached vs. loaded) independent of other variants
 - Resolution of the local cache path
 - Creation of OpenAI‑style chat and audio clients once loaded
Unlike <xref href="Microsoft.AI.Foundry.Local.Model" data-throw-if-not-resolved="false"></xref>, which orchestrates multiple variants under an alias, <xref href="Microsoft.AI.Foundry.Local.ModelVariant" data-throw-if-not-resolved="false"></xref> is the
the one specific model instance.
All public methods surface consistent error handling through <xref href="Microsoft.AI.Foundry.Local.FoundryLocalException" data-throw-if-not-resolved="false"></xref>.

 [OpenAIAudioClient](Microsoft.AI.Foundry.Local.OpenAIAudioClient.md)

Audio transcription client using an OpenAI compatible API surface implemented using Betalgo.Ranul.OpenAI SDK types.
Supports transcription of audio files.

 [OpenAIChatClient](Microsoft.AI.Foundry.Local.OpenAIChatClient.md)

Chat client using an OpenAI compatible API surface implemented using Betalgo.Ranul.OpenAI SDK types.
Provides convenience methods for standard and streaming chat completions.

 [Parameter](Microsoft.AI.Foundry.Local.Parameter.md)

A single configurable parameter that can influence model behavior.

 [PromptTemplate](Microsoft.AI.Foundry.Local.PromptTemplate.md)

Template segments used to build a prompt for a model.
For AzureFoundry model types you do NOT need to populate this; Foundry Local will handle prompt construction automatically.

 [Runtime](Microsoft.AI.Foundry.Local.Runtime.md)

Runtime configuration details describing how the model will execute.

 [Configuration.WebService](Microsoft.AI.Foundry.Local.Configuration.WebService.md)

Configuration settings if the optional web service is used.

### Interfaces

 [ICatalog](Microsoft.AI.Foundry.Local.ICatalog.md)

 [IModel](Microsoft.AI.Foundry.Local.IModel.md)

Common operations for a model variant or model abstraction including caching, loading
and client creation helpers.

### Enums

 [DeviceType](Microsoft.AI.Foundry.Local.DeviceType.md)

Device types supported by the runtime for model execution.

 [LogLevel](Microsoft.AI.Foundry.Local.LogLevel.md)

Logging verbosity levels used by the Foundry Local SDK. These levels align with Serilog (Verbose, Debug, Information, Warning, Error, Fatal)
and differ from Microsoft.Extensions.Logging.LogLevel, which includes Trace, Critical, and None.

