// Copyright (c) Microsoft Corporation. Licensed under the MIT License.
package com.microsoft.foundry.local.cli;

import static org.junit.jupiter.api.Assertions.*;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.concurrent.TimeUnit;
import org.junit.jupiter.api.Test;

class AsrCliEncodingTest {
    private static final String UNKNOWN = "caf\u00e9\u4e2d\u6587\ud83d\ude00";

    @Test void subprocessJsonlIsUtf8EvenWithWindows1252Stdout() throws Exception {
        Path directory = Files.createTempDirectory(Path.of("target"), "cli-encoding-");
        Path stdout = directory.resolve("stdout.bin");
        Path stderr = directory.resolve("stderr.txt");
        String executable = System.getProperty("os.name").startsWith("Windows") ? "java.exe" : "java";
        Process child = new ProcessBuilder(
                Path.of(System.getProperty("java.home"), "bin", executable).toString(),
                "-Dfile.encoding=windows-1252", "-Dsun.stdout.encoding=windows-1252",
                "-Dstdout.encoding=windows-1252", "-cp",
                System.getProperty("surefire.test.class.path", System.getProperty("java.class.path")),
                Probe.class.getName())
                .redirectOutput(stdout.toFile()).redirectError(stderr.toFile()).start();
        try {
            assertTrue(child.waitFor(15, TimeUnit.SECONDS), "Encoding probe did not terminate");
            assertEquals(1, child.exitValue(), Files.readString(stderr));
            byte[] bytes = Files.readAllBytes(stdout);
            assertTrue(bytes.length > 1);
            assertEquals(0xe9, Byte.toUnsignedInt(bytes[0]), "The child stdout must really use windows-1252");
            String json = StandardCharsets.UTF_8.newDecoder().decode(
                    ByteBuffer.wrap(Arrays.copyOfRange(bytes, 1, bytes.length))).toString();
            assertTrue(json.contains("\"message\":\"Unknown command: " + UNKNOWN + "\""), json);
            assertEquals(1, json.lines().count(), "One intact JSONL event must be emitted");
            assertTrue(json.endsWith("\n"));
        } finally {
            if (child.isAlive()) {
                child.destroyForcibly();
                assertTrue(child.waitFor(5, TimeUnit.SECONDS), "Encoding probe did not stop");
            }
        }
    }

    public static final class Probe {
        public static void main(String[] args) {
            if (!Charset.defaultCharset().equals(Charset.forName("windows-1252"))) {
                throw new IllegalStateException("The child default charset is not windows-1252");
            }
            // This sentinel proves the process really uses a non-UTF8 stdout encoder.
            System.out.print("\u00e9");
            System.out.flush();
            AsrCli.main(new String[] {UNKNOWN});
        }
    }
}
