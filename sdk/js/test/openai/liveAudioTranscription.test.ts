import { describe, it } from 'mocha';
import { expect } from 'chai';
import { parseTranscriptionResult, tryParseCoreError, CoreError, wrapCoreError } from '../../src/openai/liveAudioTranscriptionTypes.js';
import { LiveAudioTranscriptionOptions } from '../../src/openai/liveAudioTranscriptionClient.js';
import { getTestManager } from '../testUtils.js';

describe('Live Audio Transcription Types', () => {

    describe('parseTranscriptionResult', () => {
        it('should parse text and is_final', () => {
            const json = '{"is_final":true,"text":"hello world","start_time":null,"end_time":null}';
            const result = parseTranscriptionResult(json);

            expect(result.content).to.be.an('array').with.length(1);
            expect(result.content[0].text).to.equal('hello world');
            expect(result.content[0].transcript).to.equal('hello world');
            expect(result.is_final).to.be.true;
        });

        it('should map timing fields', () => {
            const json = '{"is_final":false,"text":"partial","start_time":1.5,"end_time":3.0}';
            const result = parseTranscriptionResult(json);

            expect(result.content[0].text).to.equal('partial');
            expect(result.is_final).to.be.false;
            expect(result.start_time).to.equal(1.5);
            expect(result.end_time).to.equal(3.0);
        });

        it('should parse empty text successfully', () => {
            const json = '{"is_final":true,"text":"","start_time":null,"end_time":null}';
            const result = parseTranscriptionResult(json);

            expect(result.content[0].text).to.equal('');
            expect(result.is_final).to.be.true;
        });

        it('should set both text and transcript to the same value', () => {
            const json = '{"is_final":true,"text":"test","start_time":null,"end_time":null}';
            const result = parseTranscriptionResult(json);

            expect(result.content[0].text).to.equal('test');
            expect(result.content[0].transcript).to.equal('test');
        });

        it('should handle only start_time', () => {
            const json = '{"is_final":true,"text":"word","start_time":2.0,"end_time":null}';
            const result = parseTranscriptionResult(json);

            expect(result.start_time).to.equal(2.0);
            expect(result.end_time).to.be.null;
            expect(result.content[0].text).to.equal('word');
        });

        it('should throw on invalid JSON', () => {
            expect(() => parseTranscriptionResult('not valid json')).to.throw();
        });
    });

    describe('tryParseCoreError', () => {
        it('should parse valid error JSON', () => {
            const json = '{"code":"ASR_SESSION_NOT_FOUND","message":"Session not found","isTransient":false}';
            const error = tryParseCoreError(json);

            expect(error).to.not.be.null;
            expect(error!.code).to.equal('ASR_SESSION_NOT_FOUND');
            expect(error!.message).to.equal('Session not found');
            expect(error!.isTransient).to.be.false;
        });

        it('should return null for invalid JSON', () => {
            const result = tryParseCoreError('not json');
            expect(result).to.be.null;
        });

        it('should parse transient error', () => {
            const json = '{"code":"BUSY","message":"Model busy","isTransient":true}';
            const error = tryParseCoreError(json);

            expect(error).to.not.be.null;
            expect(error!.isTransient).to.be.true;
        });

        it('should extract error JSON from CoreInterop-prefixed message', () => {
            const prefixed = 'Command \'audio_stream_push\' failed: {"code":"ASR_SESSION_NOT_FOUND","message":"Session not found","isTransient":false}';
            const error = tryParseCoreError(prefixed);

            expect(error).to.not.be.null;
            expect(error!.code).to.equal('ASR_SESSION_NOT_FOUND');
            expect(error!.message).to.equal('Session not found');
            expect(error!.isTransient).to.be.false;
        });
    });

    describe('LiveAudioTranscriptionOptions', () => {
        it('should have correct default values', () => {
            const settings = new LiveAudioTranscriptionOptions();

            expect(settings.sampleRate).to.equal(16000);
            expect(settings.channels).to.equal(1);
            expect(settings.bitsPerSample).to.equal(16);
            expect(settings.language).to.be.undefined;
            expect(settings.pushQueueCapacity).to.equal(100);
        });

        it('should create a frozen snapshot', () => {
            const settings = new LiveAudioTranscriptionOptions();
            settings.sampleRate = 44100;
            settings.language = 'en';

            const snapshot = settings.snapshot();

            expect(snapshot.sampleRate).to.equal(44100);
            expect(snapshot.language).to.equal('en');
            expect(() => { (snapshot as any).sampleRate = 8000; }).to.throw();
        });
    });

    describe('CoreError', () => {
        it('should expose code and isTransient when wrapping a structured error', () => {
            const cause = new Error('Command \'audio_stream_push\' failed: {"code":"BUSY","message":"Model busy","isTransient":true}');
            const err = wrapCoreError('Push failed: ', cause);

            expect(err).to.be.instanceOf(CoreError);
            expect(err.name).to.equal('CoreError');
            expect(err.code).to.equal('BUSY');
            expect(err.isTransient).to.be.true;
            expect(err.cause).to.equal(cause);
            expect(err.message).to.contain('Push failed: ');
        });

        it('should default code to UNKNOWN and isTransient to false for unstructured errors', () => {
            const cause = new Error('something exploded');
            const err = wrapCoreError('Op failed: ', cause);

            expect(err).to.be.instanceOf(CoreError);
            expect(err.code).to.equal('UNKNOWN');
            expect(err.isTransient).to.be.false;
        });

        it('should accept non-Error causes', () => {
            const err = wrapCoreError('Op failed: ', 'string cause');
            expect(err.code).to.equal('UNKNOWN');
            expect(err.message).to.contain('string cause');
        });
    });

    describe('AbortSignal helpers', () => {
        // These tests exercise the behavior locked in by the abort-listener leak fix.
        // We can't construct a real LiveAudioTranscriptionSession without the native
        // core DLL, but we can verify that AbortSignal listeners are properly added
        // and removed using the same pattern the client uses internally.

        it('should not leak listeners when racing a resolving promise against AbortSignal', async () => {
            const controller = new AbortController();
            const signal = controller.signal;
            const initialCount = (signal as any).listenerCount?.('abort') ?? 0;

            // Mimic the append() race pattern.
            for (let i = 0; i < 20; i++) {
                let onAbort: (() => void) | null = null;
                const abortPromise = new Promise<never>((_, reject) => {
                    onAbort = () => reject(new Error('aborted'));
                    signal.addEventListener('abort', onAbort, { once: true });
                });
                try {
                    await Promise.race([Promise.resolve(), abortPromise]);
                } finally {
                    if (onAbort) signal.removeEventListener('abort', onAbort);
                }
            }

            const finalCount = (signal as any).listenerCount?.('abort') ?? 0;
            expect(finalCount).to.equal(initialCount);
        });

        it('should propagate AbortError when signal is fired during race', async () => {
            const controller = new AbortController();
            const signal = controller.signal;

            let onAbort: (() => void) | null = null;
            const abortPromise = new Promise<never>((_, reject) => {
                onAbort = () => {
                    const err = new Error('The operation was aborted.');
                    err.name = 'AbortError';
                    reject(err);
                };
                signal.addEventListener('abort', onAbort, { once: true });
            });

            // Never-resolving "work" promise.
            const work = new Promise<void>(() => { /* never */ });
            const racePromise = Promise.race([work, abortPromise]);

            controller.abort();

            try {
                await racePromise;
                expect.fail('expected AbortError');
            } catch (err) {
                expect((err as Error).name).to.equal('AbortError');
            } finally {
                if (onAbort) signal.removeEventListener('abort', onAbort);
            }
        });

        it('should preserve non-Error abort reason in error message', () => {
            // Mirrors the abortMessage() helper used internally by the session client.
            // Non-Error reasons (e.g., controller.abort('timeout')) must be stringified
            // rather than dropped.
            const ctrl1 = new AbortController();
            ctrl1.abort('timeout');
            expect(typeof ctrl1.signal.reason).to.equal('string');

            const ctrl2 = new AbortController();
            ctrl2.abort(new Error('boom'));
            expect(ctrl2.signal.reason).to.be.instanceOf(Error);

            const ctrl3 = new AbortController();
            ctrl3.abort();
            expect(ctrl3.signal.reason).to.exist; // DOMException, not undefined

            // Verify the conversion logic produces a non-empty message in all cases.
            const toMessage = (signal: AbortSignal): string => {
                const r = signal.reason;
                if (r instanceof Error) return r.message;
                if (r !== undefined) return String(r);
                return 'The operation was aborted.';
            };
            expect(toMessage(ctrl1.signal)).to.equal('timeout');
            expect(toMessage(ctrl2.signal)).to.equal('boom');
            expect(toMessage(ctrl3.signal)).to.be.a('string').and.not.empty;
        });
    });

    // --- E2E streaming test with synthetic PCM audio ---

    describe('E2E with synthetic PCM audio', () => {
        const NEMOTRON_MODEL_ALIAS = 'nemotron';

        it('should complete a full streaming session with synthetic audio', async function() {
            this.timeout(60000);

            let manager;
            try {
                manager = getTestManager();
            } catch {
                console.log('  (skipped: Core DLL not available)');
                return;
            }

            const catalog = manager.catalog;

            // Skip if nemotron model is not cached
            const cachedModels = await catalog.getCachedModels();
            const cachedVariant = cachedModels.find(m => m.alias === NEMOTRON_MODEL_ALIAS);
            if (!cachedVariant) {
                console.log('  (skipped: nemotron model not cached)');
                return;
            }

            const model = await catalog.getModel(NEMOTRON_MODEL_ALIAS);
            expect(model).to.not.be.undefined;
            model!.selectVariant(cachedVariant);
            await model!.load();

            try {
                const audioClient = model!.createAudioClient();
                const session = audioClient.createLiveTranscriptionSession();
                session.settings.sampleRate = 16000;
                session.settings.channels = 1;
                session.settings.bitsPerSample = 16;
                session.settings.language = 'en';

                await session.start();

                // Collect results in background (must start before pushing audio)
                const results: any[] = [];
                const readPromise = (async () => {
                    for await (const result of session.getTranscriptionStream()) {
                        results.push(result);
                    }
                })();

                // Generate ~2 seconds of synthetic PCM audio (440Hz sine wave)
                const sampleRate = session.settings.sampleRate;
                const duration = 2;
                const totalSamples = sampleRate * duration;
                const pcmBytes = new Uint8Array(totalSamples * 2);
                for (let i = 0; i < totalSamples; i++) {
                    const t = i / sampleRate;
                    const sample = Math.round(32767 * 0.5 * Math.sin(2 * Math.PI * 440 * t));
                    pcmBytes[i * 2] = sample & 0xFF;
                    pcmBytes[i * 2 + 1] = (sample >> 8) & 0xFF;
                }

                // Push audio in 100ms chunks
                const chunkSize = (sampleRate / 10) * 2;
                for (let offset = 0; offset < pcmBytes.length; offset += chunkSize) {
                    const len = Math.min(chunkSize, pcmBytes.length - offset);
                    await session.append(pcmBytes.slice(offset, offset + len));
                }

                // Stop session to flush remaining audio and complete the stream
                await session.stop();
                await readPromise;

                // Verify response structure — synthetic audio may not produce text,
                // but response objects should be properly shaped
                for (const result of results) {
                    expect(result.content).to.be.an('array').with.length.greaterThan(0);
                    expect(result.content[0].text).to.be.a('string');
                    expect(result.content[0].transcript).to.equal(result.content[0].text);
                }
            } finally {
                await model!.unload();
            }
        });
    });
});
