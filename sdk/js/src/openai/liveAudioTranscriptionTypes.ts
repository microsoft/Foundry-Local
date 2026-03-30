/**
 * Types for real-time audio streaming transcription results and structured errors.
 * Mirrors the C# LiveAudioTranscriptionResponse (extends ConversationItem) and CoreErrorResponse.
 */

/**
 * A content part within a transcription result.
 * Follows the OpenAI Realtime API's ContentPart pattern.
 */
export interface TranscriptionContentPart {
    /** The transcribed text. */
    text?: string | null;
    /** Alias for text, matching the OpenAI Realtime API's ContentPart.transcript field. */
    transcript?: string | null;
}

/**
 * A transcription result from a real-time audio streaming session.
 * Shaped like the OpenAI Realtime API's ConversationItem so that
 * customers access text via result.content[0].text or result.content[0].transcript.
 */
export interface LiveAudioTranscriptionResult {
    /** Unique identifier for this result (if available). */
    id?: string | null;
    /** Whether this is a partial (interim) or final result for this segment. */
    is_final: boolean;
    /** The transcription content parts. Access text via content[0].text or content[0].transcript. */
    content: TranscriptionContentPart[];
    /** Start time offset of this segment in the audio stream (seconds). */
    start_time?: number | null;
    /** End time offset of this segment in the audio stream (seconds). */
    end_time?: number | null;
}

/**
 * Parse raw Core JSON response into a LiveAudioTranscriptionResult.
 * Maps the flat Core format (text, is_final, start_time, end_time) into
 * the ConversationItem-shaped result with content[0].text and content[0].transcript.
 * @internal
 */
export function parseTranscriptionResult(json: string): LiveAudioTranscriptionResult {
    const raw = JSON.parse(json);
    return {
        is_final: raw.is_final ?? false,
        start_time: raw.start_time ?? null,
        end_time: raw.end_time ?? null,
        content: [
            {
                text: raw.text ?? '',
                transcript: raw.text ?? ''
            }
        ]
    };
}

/**
 * Structured error response from native core audio streaming commands.
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
 * Returns null if the error is not valid JSON or doesn't match the schema.
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
        return null;
    }
}
