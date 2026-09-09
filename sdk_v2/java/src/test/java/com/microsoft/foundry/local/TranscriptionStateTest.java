// Copyright (c) Microsoft Corporation. Licensed under the MIT License.
package com.microsoft.foundry.local;

import static org.junit.jupiter.api.Assertions.*;
import org.junit.jupiter.api.Test;

class TranscriptionStateTest {
    @Test void absentOrUnfinishedResponseIsNotCancellation() {
        assertThrows(IllegalStateException.class, () -> Transcription.cancelledResult(false, false, 0));
        assertThrows(IllegalStateException.class, () -> Transcription.cancelledResult(false, true, 0));
        assertThrows(IllegalStateException.class, () -> Transcription.cancelledResult(false, true, 1));
    }

    @Test void onlyAnObservedCancellationMakesACancelledResult() {
        assertTrue(Transcription.cancelledResult(true, false, 0));
        assertTrue(Transcription.cancelledResult(true, true, 0));
        assertFalse(Transcription.cancelledResult(false, true, 2));
    }
}
