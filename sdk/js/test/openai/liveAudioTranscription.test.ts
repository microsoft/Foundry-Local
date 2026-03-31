import { describe, it } from 'mocha';
import { expect } from 'chai';
import { parseTranscriptionResult, tryParseCoreError } from '../../src/openai/liveAudioTranscriptionTypes.js';
import { LiveAudioTranscriptionSettings } from '../../src/openai/liveAudioTranscriptionClient.js';
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

    describe('LiveAudioTranscriptionSettings', () => {
        it('should have correct default values', () => {
            const settings = new LiveAudioTranscriptionSettings();

            expect(settings.sampleRate).to.equal(16000);
            expect(settings.channels).to.equal(1);
            expect(settings.bitsPerSample).to.equal(16);
            expect(settings.language).to.be.undefined;
            expect(settings.pushQueueCapacity).to.equal(100);
        });

        it('should create a frozen snapshot', () => {
            const settings = new LiveAudioTranscriptionSettings();
            settings.sampleRate = 44100;
            settings.language = 'en';

            const snapshot = settings.snapshot();

            expect(snapshot.sampleRate).to.equal(44100);
            expect(snapshot.language).to.equal('en');
            expect(() => { (snapshot as any).sampleRate = 8000; }).to.throw();
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
                const client = model!.createLiveTranscriptionClient();
                client.settings.sampleRate = 16000;
                client.settings.channels = 1;
                client.settings.bitsPerSample = 16;
                client.settings.language = 'en';

                await client.start();

                // Collect results in background (must start before pushing audio)
                const results: any[] = [];
                const readPromise = (async () => {
                    for await (const result of client.getTranscriptionStream()) {
                        results.push(result);
                    }
                })();

                // Generate ~2 seconds of synthetic PCM audio (440Hz sine wave)
                const sampleRate = client.settings.sampleRate;
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
                    await client.append(pcmBytes.slice(offset, offset + len));
                }

                // Stop session to flush remaining audio and complete the stream
                await client.stop();
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
