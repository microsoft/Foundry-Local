// Copyright (c) Microsoft Corporation. Licensed under the MIT License.
package com.microsoft.foundry.local;

/** This POC deliberately accepts only signed PCM16LE, 16 kHz, mono. No implicit resampling. */
public record PcmFormat(int sampleRate, int channels, int bitsPerSample) {
    public static final PcmFormat SPEECH = new PcmFormat(16000, 1, 16);
    public PcmFormat {
        if (sampleRate != 16000 || channels != 1 || bitsPerSample != 16) {
            throw new IllegalArgumentException("Expected signed PCM16LE at 16000 Hz, mono");
        }
    }
    public void validateChunk(byte[] bytes) {
        if (bytes == null || bytes.length == 0 || bytes.length % 2 != 0 || bytes.length > 32000) {
            throw new IllegalArgumentException("PCM chunks must contain 2..32000 bytes and complete 16-bit samples");
        }
    }
}
