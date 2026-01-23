// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

namespace Microsoft.AI.Foundry.Local.Tests;

using System.Text;
using System.Threading.Tasks;

internal sealed class AudioClientTests
{
    private static Model? model;

    [Before(Class)]
    public static async Task Setup()
    {
        var manager = FoundryLocalManager.Instance; // initialized by Utils
        var catalog = await manager.GetCatalogAsync();
        var model = await catalog.GetModelAsync("whisper-tiny").ConfigureAwait(false);
        await Assert.That(model).IsNotNull();

        await model.LoadAsync().ConfigureAwait(false);
        await Assert.That(await model.IsLoadedAsync()).IsTrue();

        AudioClientTests.model = model;
    }

    [Test]
    public async Task AudioTranscription_NoStreaming_Succeeds()
    {
        var audioClient = await model!.GetAudioClientAsync();
        await Assert.That(audioClient).IsNotNull();


        var audioFilePath = Path.Combine(AppContext.BaseDirectory, "testdata/Recording.mp3");

        var response = await audioClient.TranscribeAudioAsync(audioFilePath).ConfigureAwait(false);

        await Assert.That(response).IsNotNull();
        await Assert.That(response.Text).IsNotNull().And.IsNotEmpty();
        var content = response.Text;
        await Assert.That(content).IsEqualTo(" And lots of times you need to give people more than one link at a time. You a band could give their fans a couple new videos from the live concert behind the scenes photo gallery and album to purchase like these next few links.");
        Console.WriteLine($"Response: {content}");
    }

    [Test]
    public async Task AudioTranscription_Streaming_Succeeds()
    {
        var audioClient = await model!.GetAudioClientAsync();
        await Assert.That(audioClient).IsNotNull();


        var audioFilePath = Path.Combine(AppContext.BaseDirectory, "testdata/Recording.mp3");

        var updates = audioClient.TranscribeAudioStreamingAsync(audioFilePath, CancellationToken.None).ConfigureAwait(false);

        StringBuilder responseMessage = new();
        await foreach (var response in updates)
        {
            await Assert.That(response).IsNotNull();
            await Assert.That(response.Text).IsNotNull().And.IsNotEmpty();
            var content = response.Text;
            responseMessage.Append(content);
        }

        var fullResponse = responseMessage.ToString();
        Console.WriteLine(fullResponse);
        await Assert.That(fullResponse).IsEqualTo(" And lots of times you need to give people more than one link at a time. You a band could give their fans a couple new videos from the live concert behind the scenes photo gallery and album to purchase like these next few links.");


    }
}
