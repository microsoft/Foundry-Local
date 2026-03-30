import { describe, it } from 'mocha';
import { expect } from 'chai';
import { parseTranscriptionResult, tryParseCoreError } from '../../src/openai/liveAudioTranscriptionTypes.js';
import { LiveAudioTranscriptionSettings } from '../../src/openai/liveAudioTranscriptionClient.js';

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
});
