// Copyright (c) Microsoft Corporation. Licensed under the MIT License.
package com.microsoft.foundry.local;

import com.sun.jna.Memory;
import com.sun.jna.Native;
import com.sun.jna.NativeLibrary;
import com.sun.jna.Pointer;
import com.sun.jna.WString;
import com.sun.jna.ptr.IntByReference;
import java.nio.file.Path;
import java.util.Locale;

/** Explicit Windows load-only diagnostic: no manager, catalog, model, or inference calls. */
public final class WindowsLoaderProbe {
    private static final NativeLibrary KERNEL = NativeLibrary.getInstance("kernel32");

    private WindowsLoaderProbe() {}

    public static void main(String[] args) throws Exception {
        if (!System.getProperty("os.name").toLowerCase(Locale.ROOT).contains("win")) {
            throw new IllegalStateException("This diagnostic requires Windows");
        }
        Path directory = Path.of(args[0]).toRealPath();
        int flags = args.length > 1 ? Integer.decode(args[1]) : 0;
        System.out.println("{\"event\":\"jvm\",\"version\":" + quoted(System.getProperty("java.runtime.version"))
                + ",\"vendor\":" + quoted(System.getProperty("java.vendor"))
                + ",\"javaHome\":" + quoted(System.getProperty("java.home")) + "}");
        NativeApi.verify(directory);
        modules("before");
        for (String name : new String[] {"onnxruntime.dll", "onnxruntime-genai.dll", "foundry_local.dll"}) {
            Native.setLastError(0);
            Pointer library = KERNEL.getFunction("LoadLibraryExW").invokePointer(
                    new Object[] {new WString(directory.resolve(name).toString()), null, flags});
            int error = Native.getLastError();
            System.out.println("{\"event\":\"load\",\"file\":" + quoted(name)
                    + ",\"flags\":" + flags + ",\"success\":" + (library != null)
                    + ",\"win32Error\":" + (library == null ? error : 0) + "}");
            modules("after-" + name);
            if (library == null) System.exit(1);
        }
        System.out.println("{\"event\":\"loadOnlySucceeded\"}");
        // Libraries intentionally remain resident until this diagnostic process exits.
    }

    private static void modules(String stage) {
        Pointer process = KERNEL.getFunction("GetCurrentProcess").invokePointer(new Object[0]);
        try (Memory handles = new Memory(32768); Memory name = new Memory(65536)) {
            IntByReference needed = new IntByReference();
            int ok = KERNEL.getFunction("K32EnumProcessModules").invokeInt(
                    new Object[] {process, handles, (int) handles.size(), needed});
            if (ok == 0 || needed.getValue() > handles.size()) {
                throw new IllegalStateException("Module enumeration failed: " + Native.getLastError());
            }
            for (long offset = 0; offset < needed.getValue(); offset += Native.POINTER_SIZE) {
                Pointer module = handles.getPointer(offset);
                int length = KERNEL.getFunction("K32GetModuleFileNameExW").invokeInt(
                        new Object[] {process, module, name, 32768});
                if (length == 0) throw new IllegalStateException("Module path lookup failed: " + Native.getLastError());
                System.out.println("{\"event\":\"module\",\"stage\":" + quoted(stage)
                        + ",\"path\":" + quoted(name.getWideString(0)) + "}");
            }
        }
    }

    private static String quoted(String value) {
        return "\"" + value.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
    }
}
