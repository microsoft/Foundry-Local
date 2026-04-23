# Live Audio Transcription Example (C++)

This sample demonstrates the Nemotron live-audio API surface introduced in PR #655:

- `OpenAIAudioClient::CreateLiveTranscriptionSession()`
- `LiveAudioTranscriptionSession::Start()`
- `LiveAudioTranscriptionSession::Append(...)`
- `LiveAudioTranscriptionSession::TryGetNext(...)`
- `LiveAudioTranscriptionSession::Stop()`

The sample pushes synthetic PCM audio (440Hz sine wave) and prints streaming/final transcript text.

> This example assumes your branch includes the C++ SDK live-audio APIs from PR #655.
