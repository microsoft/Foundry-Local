export function isAbortSignal(value: unknown): value is AbortSignal {
    return typeof value === 'object'
        && value !== null
        && 'aborted' in value
        && typeof (value as AbortSignal).aborted === 'boolean';
}
