package org.openxray;

import android.app.Activity;
import android.os.Process;
import android.os.SystemClock;

import java.util.concurrent.atomic.AtomicLong;

/** Rare Activity transitions only; no engine callbacks, polling or frame work. */
final class LifecycleTrace {
    private static final AtomicLong SEQUENCE = new AtomicLong();
    private static final ThreadLocal<Boolean> RECORDING = new ThreadLocal<>();

    private LifecycleTrace() {}

    static void record(Activity activity, String event) {
        if (Boolean.TRUE.equals(RECORDING.get())) return;
        try {
            RECORDING.set(Boolean.TRUE);
            AppLog.i("Lifecycle", "seq=" + SEQUENCE.incrementAndGet()
                    + " uptime_ms=" + SystemClock.uptimeMillis()
                    + " pid=" + Process.myPid()
                    + " tid=" + Process.myTid()
                    + " activity=" + activity.getClass().getSimpleName()
                    + " instance=" + Integer.toHexString(System.identityHashCode(activity))
                    + " task=" + activity.getTaskId()
                    + " event=" + event
                    + " finishing=" + activity.isFinishing()
                    + " changing_config=" + activity.isChangingConfigurations());
        } catch (RuntimeException | LinkageError ignored) {
            // Optional diagnostics must not change a framework transition.
        } finally {
            RECORDING.remove();
        }
    }
}
