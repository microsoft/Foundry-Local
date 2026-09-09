// Copyright (c) Microsoft Corporation. Licensed under the MIT License.
package com.microsoft.foundry.local.cli;

import com.microsoft.foundry.local.*;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/** Generic command-line example. JSON Lines on stdout; errors have a nonzero exit status. */
public final class AsrCli {
    public static final String DEFAULT_MODEL = "nemotron-3.5-asr-streaming-0.6b-generic-cpu:3";
    private AsrCli() {}

    public static void main(String[] args) {
        try { run(args); }
        catch (Exception | LinkageError e) {
            emit(Map.of("event", "error", "errorType", e.getClass().getSimpleName(),
                    "code", e instanceof FoundryLocalException nativeError ? nativeError.code() : -1,
                    "message", String.valueOf(e.getMessage())));
            System.exit(1);
        }
    }

    static void run(String[] args) throws Exception {
        if (args.length == 0 || args[0].equals("--help")) {
            System.out.println("""
                    Foundry Local Java ASR POC (Java 17+; no Node or HTTP service)
                    identify | catalog | prepare | transcribe | stream
                      --runtime-dir DIR --cache-dir DIR [--app-data-dir DIR]
                      [--model EXACT_ID] [--json]
                    prepare --explicit-download --accept-model-license [--cancel-after-ms N]
                    transcribe --wav FILE [--cancel-after-ms N]
                    stream (--wav FILE | --pcm FILE) [--chunk-ms 100] [--cancel-after-ms N]
                    stream always paces signed PCM16LE 16000 Hz mono at real-time speed.
                    Runtime preparation is a separate explicit scripts/prepare_runtime.py command.
                    Set ORT_TELEMETRY_DISABLED=1 before launching to disable native telemetry.
                    """);
            return;
        }
        String command = args[0];
        if (!Set.of("identify", "catalog", "prepare", "transcribe", "stream").contains(command)) {
            throw new IllegalArgumentException("Unknown command: " + command);
        }
        Map<String, String> options = options(args);
        Path runtime = Path.of(required(options, "runtime-dir"));
        Path cache = Path.of(required(options, "cache-dir"));
        Path app = Path.of(options.getOrDefault("app-data-dir", cache.resolveSibling("app-data").toString()));
        String modelId = options.getOrDefault("model", DEFAULT_MODEL);
        try (FoundryLocalManager manager = new FoundryLocalManager(
                new Configuration("foundry-java-asr", runtime, cache, app))) {
            emit(Map.of("event", "runtime", "version", manager.runtimeVersion(), "apiVersion", 1,
                    "nativeTarget", manager.nativeTarget(), "javaVersion", System.getProperty("java.runtime.version"),
                    "javaVendor", System.getProperty("java.vendor"), "pid", ProcessHandle.current().pid()));
            if (command.equals("catalog")) {
                for (ModelInfo info : manager.catalog().models()) {
                    if (info.task().equals("automatic-speech-recognition")) emit(Map.of("event", "model", "model", info));
                }
                return;
            }
            Model model = manager.catalog().getModel(modelId);
            emit(Map.of("event", "model", "model", model.info(), "cached", model.isCached(), "loaded", model.isLoaded()));
            if (command.equals("identify")) return;
            long cancelAfter = number(options, "cancel-after-ms", -1);
            if (cancelAfter < -1) throw new IllegalArgumentException("cancel-after-ms must be nonnegative");
            if (command.equals("prepare")) {
                if (!options.containsKey("explicit-download") || !options.containsKey("accept-model-license")) {
                    throw new IllegalArgumentException("Review model license metadata, then provide "
                            + "--explicit-download --accept-model-license");
                }
                CancellationToken cancellation = new CancellationToken();
                var timer = Executors.newSingleThreadScheduledExecutor();
                long start = System.nanoTime();
                try {
                    if (cancelAfter >= 0) timer.schedule(cancellation::cancel, cancelAfter, TimeUnit.MILLISECONDS);
                    model.download(cancellation, percent -> emit(Map.of("event", "download", "percent", percent)));
                    emit(Map.of("event", "prepared", "modelId", modelId, "cached", model.isCached(),
                            "elapsedMillis", TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - start)));
                } finally {
                    timer.shutdownNow();
                    if (!timer.awaitTermination(5, TimeUnit.SECONDS)) throw new IllegalStateException("Timer did not stop");
                }
                return;
            }
            Path wav = options.containsKey("wav") ? Path.of(options.get("wav")) : null;
            Path pcmFile = options.containsKey("pcm") ? Path.of(options.get("pcm")) : null;
            if ((wav == null) == (pcmFile == null) || (command.equals("transcribe") && wav == null)) {
                throw new IllegalArgumentException("transcribe requires --wav; stream requires exactly one of --wav/--pcm");
            }
            byte[] pcm = wav != null ? WavAudio.read(wav).pcm() : readPcm(pcmFile);
            int chunkMs = Math.toIntExact(number(options, "chunk-ms", 100));
            if (chunkMs < 1 || chunkMs > 1000) throw new IllegalArgumentException("chunk-ms must be 1..1000");
            long loadStart = System.nanoTime();
            model.load();
            emit(Map.of("event", "loaded", "elapsedMillis",
                    TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - loadStart)));
            try (AudioSession session = model.createAudioSession()) {
                var timer = Executors.newSingleThreadScheduledExecutor();
                long start = System.nanoTime();
                long closeStart;
                try (Transcription transcription = command.equals("stream")
                        ? session.streamPcm(PcmFormat.SPEECH, event -> speech(event, start))
                        : session.transcribeWav(wav, event -> speech(event, start))) {
                    if (cancelAfter >= 0) timer.schedule(transcription::cancel, cancelAfter, TimeUnit.MILLISECONDS);
                    if (command.equals("stream")) {
                        int chunk = chunkMs * 32;
                        long feedStart = System.nanoTime();
                        for (int offset = 0; offset < pcm.length && !transcription.isDone(); offset += chunk) {
                            long deadline = feedStart + TimeUnit.MILLISECONDS.toNanos(offset / 32);
                            long delay = deadline - System.nanoTime();
                            if (delay > 0) TimeUnit.NANOSECONDS.sleep(delay);
                            if (transcription.isDone()) break;
                            try {
                                transcription.writePcm(Arrays.copyOfRange(pcm, offset, Math.min(pcm.length, offset + chunk)));
                            } catch (IllegalStateException e) {
                                if (!transcription.isCancelled()) throw e;
                                break;
                            }
                        }
                        if (!transcription.isDone() && !transcription.isCancelled()) {
                            long remaining = feedStart + pcm.length * 1_000_000_000L / 32000 - System.nanoTime();
                            if (remaining > 0) TimeUnit.NANOSECONDS.sleep(remaining);
                            transcription.finishInput();
                        }
                    }
                    TranscriptionResult result = transcription.await(Duration.ofMinutes(5));
                    emit(Map.of("event", "result", "modelId", modelId, "result", result,
                            "audioSeconds", pcm.length / 32000.0, "mode", command,
                            "timing", transcription.timing(), "chunkMillis", command.equals("stream") ? chunkMs : 100,
                            "processChildren", ProcessHandle.current().descendants().count()));
                    closeStart = System.nanoTime();
                } finally {
                    timer.shutdownNow();
                    if (!timer.awaitTermination(5, TimeUnit.SECONDS)) throw new IllegalStateException("Timer did not stop");
                }
                emit(Map.of("event", "requestClosed", "elapsedMillis",
                        TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - closeStart)));
            } finally { model.unload(); }
            emit(Map.of("event", "modelUnloaded"));
        }
        emit(Map.of("event", "managerClosed"));
    }

    private static byte[] readPcm(Path file) throws IOException {
        long size = Files.size(file);
        if (size == 0 || size % 2 != 0 || size > 64L * 1024 * 1024) {
            throw new IOException("PCM file must contain 2 bytes..64 MiB of complete 16-bit samples");
        }
        try (var input = Files.newInputStream(file)) {
            byte[] bytes = input.readNBytes(64 * 1024 * 1024 + 1);
            if (bytes.length == 0 || bytes.length % 2 != 0 || bytes.length > 64 * 1024 * 1024) {
                throw new IOException("PCM changed length while reading or exceeds 64 MiB");
            }
            return bytes;
        }
    }

    private static void speech(SpeechEvent event, long start) {
        emit(Map.of("event", "speech", "speech", event,
                "elapsedMillis", TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - start)));
    }

    private static Map<String, String> options(String[] args) {
        Set<String> flags = Set.of("explicit-download", "accept-model-license", "json");
        Set<String> values = Set.of("runtime-dir", "cache-dir", "app-data-dir", "model", "wav", "pcm",
                "chunk-ms", "cancel-after-ms");
        Map<String, String> options = new LinkedHashMap<>();
        for (int i = 1; i < args.length; i++) {
            if (!args[i].startsWith("--")) throw new IllegalArgumentException("Expected an option: " + args[i]);
            String key = args[i].substring(2);
            if (options.containsKey(key)) throw new IllegalArgumentException("Duplicate option: " + key);
            if (flags.contains(key)) options.put(key, "true");
            else if (values.contains(key) && ++i < args.length) options.put(key, args[i]);
            else throw new IllegalArgumentException("Unknown option or missing value: " + key);
        }
        return options;
    }

    private static String required(Map<String, String> options, String key) {
        if (!options.containsKey(key)) throw new IllegalArgumentException("Missing --" + key);
        return options.get(key);
    }
    private static long number(Map<String, String> options, String key, long fallback) {
        return options.containsKey(key) ? Long.parseLong(options.get(key)) : fallback;
    }

    private static synchronized void emit(Object value) { System.out.println(json(value)); }

    static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof Boolean || value instanceof Number) return value.toString();
        if (value instanceof Map<?, ?> map) {
            return map.entrySet().stream().map(e -> json(e.getKey().toString()) + ":" + json(e.getValue()))
                    .collect(java.util.stream.Collectors.joining(",", "{", "}"));
        }
        if (value.getClass().isRecord()) {
            Map<String, Object> fields = new LinkedHashMap<>();
            for (var field : value.getClass().getRecordComponents()) {
                try { fields.put(field.getName(), field.getAccessor().invoke(value)); }
                catch (IllegalAccessException | InvocationTargetException e) { throw new IllegalStateException(e); }
            }
            return json(fields);
        }
        StringBuilder out = new StringBuilder("\"");
        for (char c : value.toString().toCharArray()) {
            switch (c) {
                case '"' -> out.append("\\\"");
                case '\\' -> out.append("\\\\");
                case '\n' -> out.append("\\n");
                case '\r' -> out.append("\\r");
                case '\t' -> out.append("\\t");
                default -> {
                    if (c < 32) out.append(String.format("\\u%04x", (int) c));
                    else out.append(c);
                }
            }
        }
        return out.append('"').toString();
    }
}
