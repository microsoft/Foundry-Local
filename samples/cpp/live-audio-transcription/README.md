# Live Audio Transcription Example (C++)

Demonstrates real-time microphone-to-text using the Foundry Local C++ SDK.

Uses [PortAudio](http://www.portaudio.com/) for cross-platform microphone capture
(the C/C++ equivalent of `naudiodon2` used by the JS sample). If PortAudio is not
available, falls back to synthetic PCM audio.


## Build

```bash
# With PortAudio (live microphone)
g++ -std=c++20 -DHAS_PORTAUDIO main.cpp -lfoundry_local -lportaudio -o live-audio-transcription-example

# Without PortAudio (synthetic audio only)
g++ -std=c++20 main.cpp -lfoundry_local -o live-audio-transcription-example
```

## Run

```bash
# Live microphone (requires PortAudio)
./live-audio-transcription-example

# Synthetic 440Hz sine wave (no microphone needed)
./live-audio-transcription-example --synth
```
