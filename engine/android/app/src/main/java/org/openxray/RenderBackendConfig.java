package org.openxray;

import java.io.File;
import java.util.Locale;

/** The requested graphics route, before SDL creates an EGL display/context. */
final class RenderBackendConfig {
    static final String NATIVE = "native";
    static final String ANGLE_VULKAN = "angle_vulkan";
    static final String ANGLE_GLES = "angle_gles";
    // The bundled ANGLE build targets Android 10; the native route still supports API 24.
    static final int ANGLE_MIN_API = 29;
    static final ProcessSession PROCESS = new ProcessSession();

    private static final String[] DRIVER_ENV = {
        "SDL_VIDEO_EGL_DRIVER", "SDL_VIDEO_GL_DRIVER", "ANGLE_DEFAULT_PLATFORM"
    };

    private RenderBackendConfig() {}

    static String normalize(String backend) {
        if (backend == null) return NATIVE;
        String normalized = backend.toLowerCase(Locale.ROOT);
        return ANGLE_VULKAN.equals(normalized) || ANGLE_GLES.equals(normalized)
            ? normalized : NATIVE;
    }

    static boolean isAngle(String backend) {
        return ANGLE_VULKAN.equals(backend) || ANGLE_GLES.equals(backend);
    }

    static final class Availability {
        final String eglPath;
        final String glesPath;
        final String reason;

        Availability(String eglPath, String glesPath, String reason) {
            this.eglPath = eglPath;
            this.glesPath = glesPath;
            this.reason = reason;
        }

        boolean isAvailable() {
            return eglPath != null && glesPath != null;
        }
    }

    static Availability probe(int sdk, String nativeLibraryDir) {
        if (sdk < ANGLE_MIN_API) {
            return new Availability(null, null, "Bundled ANGLE requires Android 10 (API 29) or newer.");
        }
        if (nativeLibraryDir == null || nativeLibraryDir.isEmpty()) {
            return new Availability(null, null, "App native library directory is unavailable.");
        }
        File egl = new File(nativeLibraryDir, "libEGL_angle.so");
        File gles = new File(nativeLibraryDir, "libGLESv2_angle.so");
        if (!usableLibrary(egl) || !usableLibrary(gles)) {
            return new Availability(null, null,
                "Both packaged ANGLE libraries are required (libEGL_angle.so and libGLESv2_angle.so).");
        }
        // A matching APK pair is intentional: unrelated system/vendor libraries may be
        // inaccessible to the app linker namespace or use incompatible implementations.
        return new Availability(egl.getAbsolutePath(), gles.getAbsolutePath(), null);
    }

    private static boolean usableLibrary(File file) {
        return file.isFile() && file.canRead() && file.length() > 0;
    }

    interface Runtime {
        void setenv(String name, String value) throws Exception;
        void unsetenv(String name) throws Exception;
        void load(String absolutePath) throws Exception;
    }

    static final class Selection {
        final String requested;
        final String effective;
        final String fallbackReason;

        Selection(String requested, String effective, String fallbackReason) {
            this.requested = requested;
            this.effective = effective;
            this.fallbackReason = fallbackReason;
        }
    }

    static final class RestartRequiredException extends IllegalStateException {
        RestartRequiredException() {
            super("Graphics libraries are already initialized. Force-stop and reopen the app to change backend.");
        }
    }

    /** Native libraries and their cached function pointers outlive an Activity. */
    static final class ProcessSession {
        private Selection selection;

        synchronized String effectiveBackend() {
            return selection == null ? null : selection.effective;
        }

        synchronized boolean requiresRestart(String backend) {
            String requested = normalize(backend);
            return selection != null && !requested.equals(selection.effective);
        }

        synchronized Selection prepare(String backend, Availability available, Runtime runtime) {
            String requested = normalize(backend);
            if (requiresRestart(requested)) throw new RestartRequiredException();
            if (selection != null) {
                return new Selection(requested, selection.effective,
                    requested.equals(selection.requested) ? selection.fallbackReason : null);
            }

            clearDriverEnvironment(runtime);
            if (!isAngle(requested)) {
                selection = new Selection(requested, NATIVE, null);
                return selection;
            }
            if (!available.isAvailable()) {
                selection = new Selection(requested, NATIVE, available.reason);
                return selection;
            }

            try {
                runtime.setenv("SDL_VIDEO_EGL_DRIVER", available.eglPath);
                runtime.setenv("SDL_VIDEO_GL_DRIVER", available.glesPath);
                runtime.setenv("ANGLE_DEFAULT_PLATFORM", ANGLE_VULKAN.equals(requested) ? "vulkan" : "gl");
                // Absolute paths ensure the pair verified above is the pair loaded.
                runtime.load(available.eglPath);
                runtime.load(available.glesPath);
                selection = new Selection(requested, requested, null);
            } catch (Exception | LinkageError failure) {
                // A partially configured ANGLE environment must never reach SDL with
                // a message claiming that the system driver will be used instead.
                try {
                    clearDriverEnvironment(runtime);
                } catch (IllegalStateException cleanupFailure) {
                    cleanupFailure.addSuppressed(failure);
                    throw cleanupFailure;
                }
                selection = new Selection(requested, NATIVE,
                    "ANGLE initialization failed: " + failure.getClass().getSimpleName()
                        + ": " + String.valueOf(failure.getMessage()));
            }
            return selection;
        }
    }

    private static void clearDriverEnvironment(Runtime runtime) {
        Exception firstFailure = null;
        for (String variable : DRIVER_ENV) {
            try {
                runtime.unsetenv(variable);
            } catch (Exception failure) {
                if (firstFailure == null) firstFailure = failure;
                else firstFailure.addSuppressed(failure);
            }
        }
        if (firstFailure != null) {
            throw new IllegalStateException("Could not reset graphics driver environment; launch stopped.", firstFailure);
        }
    }
}
