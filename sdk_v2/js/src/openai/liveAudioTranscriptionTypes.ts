/**
 * Types for real-time audio streaming transcription results and structured errors.
 * Mirrors the C# LiveAudioTranscriptionResult and CoreErrorResponse.
 */

/**
 * A transcription result from a real-time audio streaming session.
 */
export interface LiveAudioTranscriptionResult {
    /** Whether this is a partial (interim) or final result for this segment. */
    is_final: boolean;
    /** The transcribed text. */
    text: string;
    /** Start time offset of this segment in the audio stream (seconds). */
    start_time?: number | null;
    /** End time offset of this segment in the audio stream (seconds). */
    end_time?: number | null;
    /** Confidence score (0.0 - 1.0) if available. */
    confidence?: number | null;
}

/**
 * Structured error response from native core audio streaming commands.
 * Used by the push loop to distinguish transient vs permanent failures.
 * @internal
 */
export interface CoreErrorResponse {
    /** Machine-readable error code. */
    code: string;
    /** Human-readable error message. */
    message: string;
    /** Whether this error is transient and may succeed on retry. */
    isTransient: boolean;
}

/**
 * Attempt to parse a native error string as a structured CoreErrorResponse.
 * Returns null if the error is not valid JSON or doesn't match the schema,
 * which should be treated as a permanent/unknown error.
 * @internal
 */
export function tryParseCoreError(errorString: string): CoreErrorResponse | null {
    try {
        const parsed = JSON.parse(errorString);
        if (typeof parsed.code === 'string' && typeof parsed.isTransient === 'boolean') {
            return parsed as CoreErrorResponse;
        }
        return null;
    } catch {
        return null; // unstructured error — treat as permanent
    }
}
