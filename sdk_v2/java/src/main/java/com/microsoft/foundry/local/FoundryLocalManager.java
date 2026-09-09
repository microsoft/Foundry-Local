// Copyright (c) Microsoft Corporation. Licensed under the MIT License.
package com.microsoft.foundry.local;

import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;

/** Owns the native singleton and every session. Catalogs/models are borrowed views. */
public final class FoundryLocalManager implements AutoCloseable {
    private static boolean active;
    private static boolean shutDown;
    final NativeApi api;
    final Set<AudioSession> sessions = new HashSet<>();
    private Pointer handle;

    public FoundryLocalManager(Configuration configuration) {
        NativeApi.outsideCallback();
        synchronized (FoundryLocalManager.class) {
            if (active) throw new IllegalStateException("Only one FoundryLocalManager may be open per JVM");
            if (shutDown) throw new IllegalStateException(
                    "The pinned native runtime cannot recreate a manager after shutdown; use a new JVM");
            api = NativeApi.load(configuration.runtimeDirectory());
            Pointer config = api.create(api.config, 0, configuration.appName());
            try {
                // Native status errors still propagate; suppress console logs that would corrupt CLI JSONL.
                api.check(api.config.pointer(2, config, 5));
                api.check(api.config.pointer(3, config, configuration.appDataDirectory().toString()));
                api.check(api.config.pointer(5, config, configuration.modelCacheDirectory().toString()));
                PointerByReference pairs = new PointerByReference();
                api.root.call(15, pairs);
                try {
                    api.root.call(16, pairs.getValue(), "DisableNonessentialTelemetry", "true");
                    api.check(api.config.pointer(10, config, pairs.getValue()));
                } finally {
                    api.root.call(20, pairs.getValue());
                }
                handle = api.create(api.root, 4, config);
                active = true;
            } finally {
                api.config.call(1, config);
            }
        }
    }

    public String runtimeVersion() { return api.version; }
    public String nativeTarget() { return NativeApi.target(); }

    public Catalog catalog() {
        NativeApi.outsideCallback();
        synchronized (this) {
            checkOpen();
            return new Catalog(this, api.create(api.root, 6, handle));
        }
    }

    void checkOpen() {
        NativeApi.outsideCallback();
        if (handle == null) throw new IllegalStateException("Manager is closed");
    }

    @Override public void close() {
        NativeApi.outsideCallback();
        synchronized (this) {
            if (handle == null) return;
            // Join workers before destroying their session, model or manager dependencies.
            for (AudioSession session : new ArrayList<>(sessions)) session.close();
            api.check(api.root.pointer(27, handle));
            api.root.call(5, handle);
            handle = null;
            synchronized (FoundryLocalManager.class) {
                active = false;
                shutDown = true;
            }
        }
    }
}
