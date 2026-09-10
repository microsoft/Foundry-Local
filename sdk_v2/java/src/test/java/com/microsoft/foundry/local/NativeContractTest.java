// Copyright (c) Microsoft Corporation. Licensed under the MIT License.
package com.microsoft.foundry.local;

import static org.junit.jupiter.api.Assertions.*;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Properties;
import java.util.Set;
import java.util.stream.Collectors;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class NativeContractTest {
    @TempDir Path temporary;

    @Test void matchesPackaged64BitStructSizes() {
        assertEquals(16, new NativeApi.CallbackData().size());
        assertEquals(72, new NativeApi.AudioData().size());
        assertEquals(48, new NativeApi.BytesData().size());
        assertEquals(64, new NativeApi.SegmentData().size());
        assertEquals(48, new NativeApi.ResultData().size());
    }

    @Test void missingNativeFilesFailBeforeLoading() {
        IOException error = assertThrows(IOException.class, () -> NativeApi.verify(temporary));
        assertTrue(error.getMessage().contains("Missing pinned native file"));
    }

    @Test void incorrectNativeHashesFailBeforeCallingAnyAbi() throws Exception {
        Properties pins = new Properties();
        try (var input = NativeApi.class.getResourceAsStream("native-lock.properties")) { pins.load(input); }
        String prefix = NativeApi.target() + ".";
        for (String key : pins.stringPropertyNames()) {
            if (key.startsWith(prefix)) Files.writeString(temporary.resolve(key.substring(prefix.length())), "not a native");
        }
        IOException error = assertThrows(IOException.class, () -> NativeApi.verify(temporary));
        assertTrue(error.getMessage().contains("SHA-256 mismatch"));
    }

    @Test void callbackLifecycleReentrancyIsRejected() {
        NativeApi.IN_CALLBACK.set(true);
        try { assertThrows(IllegalStateException.class, NativeApi::outsideCallback); }
        finally { NativeApi.IN_CALLBACK.remove(); }
    }

    @Test void everyPublishedTargetHasCompleteWellFormedPins() throws Exception {
        Properties pins = new Properties();
        try (var input = NativeApi.class.getResourceAsStream("native-lock.properties")) {
            assertNotNull(input);
            pins.load(input);
        }
        Set<String> targets = Set.of("win-x64", "win-arm64", "linux-x64", "linux-arm64", "osx-arm64");
        assertEquals(targets, pins.stringPropertyNames().stream()
                .map(key -> key.substring(0, key.indexOf('.'))).collect(Collectors.toSet()));
        for (String key : pins.stringPropertyNames()) {
            assertTrue(pins.getProperty(key).matches("[0-9a-f]{64}"), key);
        }
        for (String target : targets) {
            assertTrue(pins.stringPropertyNames().stream().filter(key -> key.startsWith(target + ".")).count() >= 4);
        }
        for (String target : Set.of("linux-x64", "linux-arm64")) {
            assertEquals(pins.getProperty(target + ".libonnxruntime.so"),
                    pins.getProperty(target + ".libonnxruntime.so.1"));
        }
        assertEquals(pins.getProperty("osx-arm64.libonnxruntime.dylib"),
                pins.getProperty("osx-arm64.libonnxruntime.1.dylib"));
        try (var input = NativeApi.class.getResourceAsStream("runtime-lock.json")) {
            assertNotNull(input);
        }
    }
}
