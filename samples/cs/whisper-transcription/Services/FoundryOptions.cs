namespace WhisperTranscription;

public class FoundryOptions
{
    public const string SectionName = "Foundry";
    public string ModelAlias { get; set; } = "whisper-tiny";
    public string LogLevel { get; set; } = "Information";
}
