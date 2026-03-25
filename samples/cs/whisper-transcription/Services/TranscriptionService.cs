using Microsoft.AI.Foundry.Local;

namespace WhisperTranscription;

public class TranscriptionService
{
    private readonly FoundryModelService _modelService;
    private readonly ILogger<TranscriptionService> _logger;

    public TranscriptionService(
        FoundryModelService modelService,
        ILogger<TranscriptionService> logger)
    {
        _modelService = modelService;
        _logger = logger;
    }

    public async Task<TranscriptionResult> TranscribeAsync(string filePath, string? modelAlias = null,
        CancellationToken ct = default)
    {
        var model = await _modelService.GetModelAsync(modelAlias);
        await _modelService.EnsureModelReadyAsync(model, ct);

        var audioClient = await model.GetAudioClientAsync(ct)
            ?? throw new InvalidOperationException("Failed to get audio client");

        _logger.LogInformation("Transcribing \"{FilePath}\" with model {ModelId}", filePath, model.Id);

        // Use streaming transcription for real-time output
        var textParts = new List<string>();
        var response = audioClient.TranscribeAudioStreamingAsync(filePath, ct);
        await foreach (var chunk in response.WithCancellation(ct))
        {
            if (!string.IsNullOrEmpty(chunk.Text))
            {
                textParts.Add(chunk.Text);
            }
        }

        var fullText = string.Join("", textParts);
        _logger.LogInformation("Transcription complete: {Length} characters", fullText.Length);

        return new TranscriptionResult
        {
            Text = fullText,
            ModelId = model.Id,
        };
    }
}

public class TranscriptionResult
{
    public string Text { get; set; } = "";
    public string ModelId { get; set; } = "";
}
