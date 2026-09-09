// Copyright (c) Microsoft Corporation. Licensed under the MIT License.
package com.microsoft.foundry.local;

import com.sun.jna.Callback;
import com.sun.jna.Function;
import com.sun.jna.Memory;
import com.sun.jna.Native;
import com.sun.jna.NativeLibrary;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;
import com.sun.jna.ptr.PointerByReference;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.HexFormat;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Properties;

/**
 * Narrow binding to the header INCLUDED IN Microsoft.AI.Foundry.Local.Runtime 2.0.1.
 * All supported targets use 64-bit pointers/size_t. C bool is one byte, not JNA boolean.
 */
final class NativeApi {
    static final int VERSION = 1;
    static final ThreadLocal<Boolean> IN_CALLBACK = ThreadLocal.withInitial(() -> false);
    private static NativeApi resident;
    private final List<NativeLibrary> libraries = new ArrayList<>();
    final Path directory;
    final String version;
    final Table root, config, catalog, model, item, inference;

    static void outsideCallback() {
        if (IN_CALLBACK.get()) {
            throw new IllegalStateException("SDK lifecycle/input calls are not allowed from a native callback");
        }
    }

    static String target() {
        String arch = System.getProperty("os.arch").toLowerCase(Locale.ROOT);
        String cpu = switch (arch) {
            case "amd64", "x86_64" -> "x64";
            case "aarch64", "arm64" -> "arm64";
            default -> throw new IllegalStateException("Unsupported CPU: " + arch);
        };
        String os = System.getProperty("os.name").toLowerCase(Locale.ROOT);
        if (os.contains("win")) return "win-" + cpu;
        if (os.contains("linux")) return "linux-" + cpu;
        if (os.contains("mac") && cpu.equals("arm64")) return "osx-arm64";
        throw new IllegalStateException("No pinned native artifact for " + os + "/" + arch);
    }

    static synchronized NativeApi load(Path path) {
        outsideCallback();
        try {
            Path real = path.toRealPath();
            verify(real);
            if (resident != null) {
                if (!resident.directory.equals(real)) {
                    throw new IllegalStateException("One pinned native runtime directory per JVM is supported");
                }
                return resident;
            }
            resident = new NativeApi(real);
            return resident;
        } catch (IOException e) {
            throw new IllegalArgumentException("Cannot read prepared native runtime: " + path, e);
        }
    }

    static void verify(Path directory) throws IOException {
        if (Native.POINTER_SIZE != 8 || Native.SIZE_T_SIZE != 8) {
            throw new IllegalStateException("Only 64-bit JVMs are supported");
        }
        Properties pins = new Properties();
        try (var input = NativeApi.class.getResourceAsStream("native-lock.properties")) {
            if (input == null) throw new IllegalStateException("Missing bundled native lock");
            pins.load(input);
        }
        String prefix = target() + ".";
        int count = 0;
        for (String key : pins.stringPropertyNames()) {
            if (!key.startsWith(prefix)) continue;
            Path file = directory.resolve(key.substring(prefix.length()));
            if (!Files.isRegularFile(file)) throw new IOException("Missing pinned native file: " + file.getFileName());
            if (!sha256(file).equalsIgnoreCase(pins.getProperty(key))) {
                throw new IOException("Incompatible native file (SHA-256 mismatch): " + file.getFileName());
            }
            count++;
        }
        if (count < 3) throw new IllegalStateException("Incomplete native pin for " + target());
    }

    static String sha256(Path file) throws IOException {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            try (var input = Files.newInputStream(file)) {
                byte[] bytes = new byte[65536];
                int n;
                while ((n = input.read(bytes)) != -1) digest.update(bytes, 0, n);
            }
            return HexFormat.of().formatHex(digest.digest());
        } catch (NoSuchAlgorithmException e) {
            throw new AssertionError(e);
        }
    }

    private NativeApi(Path path) {
        directory = path;
        String rid = target();
        String ort = rid.startsWith("win") ? "onnxruntime.dll"
                : rid.startsWith("linux") ? "libonnxruntime.so.1" : "libonnxruntime.1.dylib";
        String genai = rid.startsWith("win") ? "onnxruntime-genai.dll"
                : rid.startsWith("linux") ? "libonnxruntime-genai.so" : "libonnxruntime-genai.dylib";
        String foundry = rid.startsWith("win") ? "foundry_local.dll"
                : rid.startsWith("linux") ? "libfoundry_local.so" : "libfoundry_local.dylib";
        // Process-lifetime pinning is intentional: ORT/GenAI have process-global state.
        // Never dlclose these libraries while another thread or callback can reference them.
        libraries.add(open(path.resolve(ort)));
        libraries.add(open(path.resolve(genai)));
        NativeLibrary library = open(path.resolve(foundry));
        libraries.add(library);
        version = text(library.getFunction("FoundryLocalGetVersionString").invokePointer(new Object[0]));
        Pointer api = library.getFunction("FoundryLocalGetApi").invokePointer(new Object[] {VERSION});
        if (api == null) throw new IllegalStateException("Pinned runtime does not expose C API " + VERSION);
        root = new Table(api);
        catalog = new Table(root.pointer(10));
        config = new Table(root.pointer(11));
        item = new Table(root.pointer(12));
        inference = new Table(root.pointer(13));
        model = new Table(root.pointer(14));
    }

    private static NativeLibrary open(Path file) {
        try {
            return NativeLibrary.getInstance(file.toString(), Map.of(
                    com.sun.jna.Library.OPTION_STRING_ENCODING, "UTF-8"));
        } catch (UnsatisfiedLinkError e) {
            throw new IllegalStateException("Cannot load " + file.getFileName()
                    + "; use a matching 64-bit JVM and install platform loader prerequisites", e);
        }
    }

    static String text(Pointer pointer) { return pointer == null ? "" : pointer.getString(0, "UTF-8"); }
    static Memory utf8(String value) {
        if (value.indexOf('\0') >= 0) throw new IllegalArgumentException("Strings must not contain NUL");
        byte[] bytes = value.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        Memory memory = new Memory(bytes.length + 1L);
        memory.write(0, bytes, 0, bytes.length);
        memory.setByte(bytes.length, (byte) 0);
        return memory;
    }

    void check(Pointer status) {
        if (status == null) return;
        try {
            throw new FoundryLocalException(root.integer(2, status), text(root.pointer(3, status)));
        } finally {
            root.call(1, status);
        }
    }

    Pointer create(Table table, int slot, Object... args) {
        PointerByReference output = new PointerByReference();
        Object[] all = java.util.Arrays.copyOf(args, args.length + 1);
        all[args.length] = output;
        check(table.pointer(slot, all));
        if (output.getValue() == null) throw new IllegalStateException("Native API returned a null handle");
        return output.getValue();
    }

    static final class Table {
        private final Pointer table;
        Table(Pointer table) {
            if (table == null) throw new IllegalStateException("Missing native function table");
            this.table = table;
        }
        private Function function(int index) {
            Pointer function = table.getPointer(index * 8L);
            if (function == null) throw new IllegalStateException("Missing native function at slot " + index);
            return Function.getFunction(function, Function.C_CONVENTION, "UTF-8");
        }
        Pointer pointer(int index, Object... args) { return function(index).invokePointer(args); }
        int integer(int index, Object... args) { return function(index).invokeInt(args); }
        long size(int index, Object... args) { return function(index).invokeLong(args); }
        boolean bool(int index, Object... args) {
            return ((Byte) function(index).invoke(Byte.class, args)) != 0;
        }
        void call(int index, Object... args) { function(index).invokeVoid(args); }
    }

    interface ProgressCallback extends Callback { int invoke(float value, Pointer userData); }
    interface StreamCallback extends Callback { int invoke(CallbackData data, Pointer userData); }
    interface BytesDeleter extends Callback { void invoke(Pointer data, Pointer userData); }

    @Structure.FieldOrder({"version", "queue"})
    public static class CallbackData extends Structure implements Structure.ByValue {
        public int version;
        public Pointer queue;
    }

    @Structure.FieldOrder({"version", "data", "mutableData", "dataSize", "format", "uri",
            "sampleRate", "channels", "deleter", "userData"})
    public static class AudioData extends Structure {
        public int version = VERSION;
        public Pointer data, mutableData;
        public long dataSize;
        public Pointer format, uri;
        public int sampleRate, channels;
        public Pointer deleter, userData;
    }

    @Structure.FieldOrder({"version", "itemType", "data", "mutableData", "dataSize", "deleter", "userData"})
    public static class BytesData extends Structure {
        public int version = VERSION, itemType = 1;
        public Pointer data, mutableData;
        public long dataSize;
        public BytesDeleter deleter;
        public Pointer userData;
    }

    @Structure.FieldOrder({"version", "kind", "text", "start", "end", "utteranceStart",
            "words", "wordCount", "language"})
    public static class SegmentData extends Structure {
        public int version = VERSION, kind;
        public Pointer text;
        public long start, end;
        public byte utteranceStart;
        public Pointer words;
        public long wordCount;
        public Pointer language;
    }

    @Structure.FieldOrder({"version", "text", "language", "duration", "segments", "segmentCount"})
    public static class ResultData extends Structure {
        public int version = VERSION;
        public Pointer text, language;
        public long duration;
        public Pointer segments;
        public long segmentCount;
    }
}
