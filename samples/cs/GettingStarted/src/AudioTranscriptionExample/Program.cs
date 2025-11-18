using Microsoft.AI.Foundry.Local;

var config = new Configuration
{
    AppName = "foundry_local_samples",
    LogLevel = Microsoft.AI.Foundry.Local.LogLevel.Information
};


// Initialize the singleton instance.
await FoundryLocalManager.CreateAsync(config, Utils.GetAppLogger());
var mgr = FoundryLocalManager.Instance;


// Ensure that any Execution Provider (EP) downloads run and are completed.
// EP packages include dependencies and may be large.
// Download is only required again if a new version of the EP is released.
// For cross platform builds there is no dynamic EP download and this will return immediately.
await Utils.RunWithSpinner("Registering execution providers", mgr.EnsureEpsDownloadedAsync());


// Get the model catalog
var catalog = await mgr.GetCatalogAsync();


// Get a model using an alias and select the CPU model variant
var model = await catalog.GetModelAsync("whisper-tiny") ?? throw new System.Exception("Model not found");
var modelVariant = model.Variants.First(v => v.Info.Runtime?.DeviceType == DeviceType.CPU);
model.SelectVariant(modelVariant);


// Download the model (the method skips download if already cached)
await model.DownloadAsync(progress =>
{
    Console.Write($"\rDownloading model: {progress:F2}%");
    if (progress >= 100f)
    {
        Console.WriteLine();
    }
});


// Load the model
Console.Write($"Loading model {model.Id}...");
await model.LoadAsync();
Console.WriteLine("done.");


// Get a chat client
var audioClient = await model.GetAudioClientAsync();


// Get a transcription with streaming outputs
Console.WriteLine("Transcribing audio with streaming output:");
var response = audioClient.TranscribeAudioStreamingAsync("Recording.mp3", CancellationToken.None);
await foreach (var chunk in response)
{
    Console.Write(chunk.Text);
    Console.Out.Flush();
}

Console.WriteLine();


// Tidy up - unload the model
await model.UnloadAsync();