package org.openxray;

import android.content.DialogInterface;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.Display;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.util.Arrays;

public class OpenXRayActivity extends SDLActivity {

    private TouchOverlayView mTouchOverlay;

    private final Object mInitLock = new Object();
    private boolean mGameDataReady = false;
    private boolean mInitStarted = false;
    private View mLoadingOverlay;
    private boolean mUnlockFps;
    private SurfaceHolder.Callback mRefreshRateCallback;

    @Override
    protected String[] getLibraries() {
        return new String[] {
            "c++_shared",
            "SDL2",
            "openal",
            "xrCore",
            "xrAPI",
            "xrEngine",
            "xrRender_GL",
            "xrGame",
            "main"
        };
    }

    @Override
    public void loadLibraries() {
        Intent intent = getIntent();
        String backend = (intent != null && intent.hasExtra("extra_render_backend"))
            ? intent.getStringExtra("extra_render_backend")
            : "native";
        // SDLActivity calls this before its JNI initialization and context creation.
        // Keep driver configuration and preloading in the same transaction: a
        // failed preload must not leave SDL pointed at the failing ANGLE pair.
        try {
            configureRenderBackend(backend);
        } catch (RuntimeException failure) {
            AppLog.e("RenderBackend", "Graphics setup stopped before engine loading: " + failure.getMessage(), failure);
            AppLog.flush();
            throw failure;
        }

        String[] libs = getLibraries();
        AppLog.i("Libraries", "Starting native library loading (" + libs.length + " libraries)...");

        for (int i = 0; i < libs.length; i++) {
            String lib = libs[i];
            AppLog.i("Libraries", "[" + (i + 1) + "/" + libs.length + "] Loading library: " + lib);
            try {
                org.libsdl.app.SDL.loadLibrary(lib, this);
                AppLog.i("Libraries", "  -> SUCCESS: " + lib);
            } catch (UnsatisfiedLinkError e) {
                AppLog.e("Libraries", "  -> FATAL: UnsatisfiedLinkError loading " + lib + ": " + e.getMessage(), e);
                AppLog.flush();
                throw e;
            } catch (Throwable t) {
                AppLog.e("Libraries", "  -> FATAL: Error loading " + lib + ": " + t.getMessage(), t);
                AppLog.flush();
                throw new RuntimeException("Failed to load native library: " + lib, t);
            }
        }
        AppLog.i("Libraries", "All native libraries loaded successfully.");
    }

    @Override
    protected String[] getArguments() {
        Intent intent = getIntent();
        if (intent != null && intent.hasExtra("extra_args")) {
            String argsStr = intent.getStringExtra("extra_args");
            if (argsStr != null && !argsStr.trim().isEmpty()) {
                java.util.List<String> list = new java.util.ArrayList<>();
                java.util.regex.Matcher m = java.util.regex.Pattern.compile("([^\"\\s]+|\"[^\"]*\")").matcher(argsStr.trim());
                while (m.find()) {
                    String match = m.group(1);
                    if (match.startsWith("\"") && match.endsWith("\"") && match.length() >= 2) {
                        match = match.substring(1, match.length() - 1);
                    }
                    list.add(match);
                }
                if (!list.isEmpty()) {
                    return list.toArray(new String[0]);
                }
            }
        }
        return new String[] {
            "-soc",
            "-rgl",
            // There is no logo.bmp next to fsgame.ltx on Android, so ShowSplash()
            // would only log "Couldn't create surface from image" and exit again.
            "-nosplash",
            "-force_flushlog"
        };
    }

    @Override
    public void setOrientationBis(int w, int h, boolean resizable, String hint) {
        setRequestedOrientation(android.content.pm.ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            android.view.WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode = android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }
        getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_FULLSCREEN | android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        Intent intent = getIntent();
        String path = (intent != null && intent.hasExtra("extra_game_path")) 
            ? intent.getStringExtra("extra_game_path") 
            : null;
        AppLog.init(this, path);
        LifecycleTrace.record(this, "onCreate");
        AppLog.i("Activity", "OpenXRayActivity onCreate started");
        // From here on the process is expected to die abnormally sometimes.
        // The marker is only removed in onDestroy, so whatever survives until
        // the next launcher start is what CrashReporter treats as a crash.
        CrashReporter.markGameStarted(this, path);

        String[] args = getArguments();
        mUnlockFps = Arrays.asList(args).contains("-unlock_fps");
        AppLog.i("Activity", "Launch arguments: " + Arrays.toString(args));

        super.onCreate(savedInstanceState);
        if (mBrokenLibraries) {
            AppLog.e("Activity", "Native initialization failed; SDL error dialog is active.");
            AppLog.flush();
            return;
        }

        if (mUnlockFps && mSurface != null) {
            mRefreshRateCallback = new SurfaceHolder.Callback() {
                @Override public void surfaceCreated(SurfaceHolder holder) {
                    requestHighRefreshRate();
                }
                @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                    requestHighRefreshRate();
                }
                @Override public void surfaceDestroyed(SurfaceHolder holder) {}
            };
            mSurface.getHolder().addCallback(mRefreshRateCallback);
            requestHighRefreshRate();
        }

        hideSystemUI();
        startGameDirectoriesAsync();
        setupTouchControls();

        AppLog.i("Activity", "OpenXRayActivity onCreate finished successfully");
    }

    private void requestHighRefreshRate() {
        if (!mUnlockFps || mSurface == null) return;

        Display display = mSurface.getDisplay();
        if (display == null) display = getWindowManager().getDefaultDisplay();
        if (display == null) return;

        Display.Mode current = display.getMode();
        Display.Mode best = current;
        for (Display.Mode mode : display.getSupportedModes()) {
            // A lower game render resolution must not change the physical display mode.
            if (mode.getPhysicalWidth() == current.getPhysicalWidth()
                    && mode.getPhysicalHeight() == current.getPhysicalHeight()
                    && mode.getRefreshRate() > best.getRefreshRate()) {
                best = mode;
            }
        }

        android.view.WindowManager.LayoutParams lp = getWindow().getAttributes();
        if (lp.preferredDisplayModeId != best.getModeId()
                || lp.preferredRefreshRate != best.getRefreshRate()) {
            lp.preferredDisplayModeId = best.getModeId();
            lp.preferredRefreshRate = best.getRefreshRate();
            getWindow().setAttributes(lp);
        }

        // Surface voting is useful on Android versions that default games to 60 Hz.
        // This is a request to the compositor, not a guarantee of the actual FPS.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Surface surface = mSurface.getHolder().getSurface();
            if (surface != null && surface.isValid()) {
                try {
                    surface.setFrameRate(best.getRefreshRate(), Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);
                } catch (IllegalArgumentException | IllegalStateException e) {
                    AppLog.w("FrameRate", "Screen refresh request failed: " + e.getMessage());
                }
            }
        }
        AppLog.i("FrameRate", "FPS unlock: requested " + best.getRefreshRate()
                + " Hz at " + best.getPhysicalWidth() + "x" + best.getPhysicalHeight()
                + "; display currently reports " + display.getRefreshRate() + " Hz");
    }

    private void configureRenderBackend(String backend) {
        RenderBackendConfig.Availability available = RenderBackendConfig.probe(
            Build.VERSION.SDK_INT, getApplicationInfo().nativeLibraryDir);
        RenderBackendConfig.Selection selection = RenderBackendConfig.PROCESS.prepare(backend, available,
            new RenderBackendConfig.Runtime() {
                @Override public void setenv(String name, String value) throws Exception {
                    android.system.Os.setenv(name, value, true);
                }
                @Override public void unsetenv(String name) throws Exception {
                    android.system.Os.unsetenv(name);
                }
                @Override public void load(String absolutePath) {
                    AppLog.i("Libraries", "Preloading " + absolutePath);
                    System.load(absolutePath);
                }
            });
        try {
            // Native verification must use the configured route after any fallback,
            // while retaining the user's original request for diagnostics.
            android.system.Os.setenv("OPENXRAY_RENDER_BACKEND_REQUESTED", selection.requested, true);
            android.system.Os.setenv("OPENXRAY_RENDER_BACKEND_EFFECTIVE", selection.effective, true);
        } catch (Exception failure) {
            throw new IllegalStateException("Could not configure graphics route verification; launch stopped.", failure);
        }
        AppLog.i("RenderBackend", "Requested=" + selection.requested + "; configured=" + selection.effective
            + "; actual EGL/GL renderer will be verified by native initialization.");
        if (selection.fallbackReason != null) {
            AppLog.w("RenderBackend", selection.fallbackReason + " Configured Native OpenGL ES instead.");
            Toast.makeText(this, selection.fallbackReason + " Using Native OpenGL ES.", Toast.LENGTH_LONG).show();
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemUI();
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        LifecycleTrace.record(this, "onStart");
    }

    @Override
    protected void onResume() {
        AppLog.i("Activity", "OpenXRayActivity onResume");
        super.onResume();
        LifecycleTrace.record(this, "onResume");
        if (mBrokenLibraries) return;
        hideSystemUI();
        requestHighRefreshRate();
    }

    public void hideSystemUI() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            android.view.Window window = getWindow();
            if (window != null) {
                window.setDecorFitsSystemWindows(false);
                android.view.WindowInsetsController controller = window.getInsetsController();
                if (controller != null) {
                    controller.hide(android.view.WindowInsets.Type.statusBars() | android.view.WindowInsets.Type.navigationBars());
                    controller.setSystemBarsBehavior(android.view.WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                }
            }
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            View decorView = getWindow().getDecorView();
            decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN
            );
        }
        getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
        setWindowStyle(true);
    }

    @Override
    protected void onPause() {
        LifecycleTrace.record(this, "onPause");
        AppLog.i("Activity", "OpenXRayActivity onPause");
        super.onPause();
    }

    @Override
    protected void onStop() {
        LifecycleTrace.record(this, "onStop");
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        LifecycleTrace.record(this, "onDestroy");
        AppLog.i("Activity", "OpenXRayActivity onDestroy");
        if (mRefreshRateCallback != null && mSurface != null) {
            mSurface.getHolder().removeCallback(mRefreshRateCallback);
        }
        super.onDestroy();
        // A clean teardown: drop the crash marker so the next launcher start
        // does not offer to report a non-crash.
        CrashReporter.markGameStoppedCleanly(this);
        AppLog.close();
        // The underlying launcher may already have resumed before onDestroy.
        // Keep Java crash logging available after native/logcat teardown.
        AppLog.resumeCapture();
    }

    @Override
    protected boolean isNativeStartReady() {
        synchronized (mInitLock) {
            return mGameDataReady && !isFinishing() && !isDestroyed();
        }
    }

    // Runs the first-run setup (asset extraction etc.) off the main thread so
    // the UI stays responsive. On OEM ROMs with aggressive watchdog/ANR
    // monitoring (e.g. MIUI) blocking the main thread for several seconds in
    // onCreate() was reported as a hang and the app got killed.
    private void startGameDirectoriesAsync() {
        synchronized (mInitLock) {
            if (mInitStarted) {
                return;
            }
            mInitStarted = true;
        }
        showLoadingOverlay();
        AppLog.i("Activity", "Preparing game data on background thread...");
        new Thread(() -> {
            try {
                createGameDirectories();
            } finally {
                synchronized (mInitLock) {
                    mGameDataReady = true;
                }
                runOnUiThread(this::completeGameInit);
            }
        }, "GameDataInitThread").start();
    }

    private void completeGameInit() {
        if (isFinishing() || isDestroyed()) {
            return;
        }
        hideLoadingOverlay();
        AppLog.i("Activity", "Game data ready, starting engine.");
        SDLActivity.handleNativeState();
    }

    private void showLoadingOverlay() {
        if (mLoadingOverlay != null || mLayout == null) {
            return;
        }
        FrameLayout overlay = new FrameLayout(this);
        overlay.setBackgroundColor(0xFF000000);

        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setGravity(Gravity.CENTER);

        ProgressBar spinner = new ProgressBar(this);
        LinearLayout.LayoutParams spinnerLp = new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.WRAP_CONTENT,
            ViewGroup.LayoutParams.WRAP_CONTENT);
        content.addView(spinner, spinnerLp);

        TextView text = new TextView(this);
        text.setText("Preparing game data...");
        text.setTextColor(0xFFFFFFFF);
        text.setTextSize(16);
        LinearLayout.LayoutParams textLp = new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.WRAP_CONTENT,
            ViewGroup.LayoutParams.WRAP_CONTENT);
        textLp.topMargin = (int) (16 * getResources().getDisplayMetrics().density);
        content.addView(text, textLp);

        overlay.addView(content, new FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT));
        mLayout.addView(overlay, new FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT));
        overlay.bringToFront();
        mLoadingOverlay = overlay;
    }

    private void hideLoadingOverlay() {
        if (mLoadingOverlay != null) {
            ViewGroup parent = (ViewGroup) mLoadingOverlay.getParent();
            if (parent != null) {
                parent.removeView(mLoadingOverlay);
            }
            mLoadingOverlay = null;
        }
    }

    private void createGameDirectories() {
        try {
            Intent intent = getIntent();
            String path = (intent != null && intent.hasExtra("extra_game_path")) 
                ? intent.getStringExtra("extra_game_path") 
                : null;
            File dir = (path != null && !path.trim().isEmpty())
                ? new File(path.trim())
                : new File(getExternalFilesDir(null), "olr");

            AppLog.i("FileSystem", "Target game directory: " + dir.getAbsolutePath());
            if (!dir.exists()) {
                boolean created = dir.mkdirs();
                AppLog.i("FileSystem", "Created target directory: " + created);
            }
            // Game mode selected in launcher: soc / cs / cop.
            String gameMode = "soc";
            try {
                Intent modeIntent = getIntent();
                if (modeIntent != null && modeIntent.hasExtra("extra_game_mode")) {
                    String m = modeIntent.getStringExtra("extra_game_mode");
                    if ("cs".equals(m) || "cop".equals(m) || "soc".equals(m)) {
                        gameMode = m;
                    }
                }
            } catch (Exception ignored) {}
            AppLog.i("FileSystem", "Game mode for res overlay: " + gameMode);

            // res/ is split per game: res/soc/* for Shadow of Chernobyl,
            // res/cop/* for Call of Pripyat, res/cs/* for Clear Sky.
            // Clear Sky used to share res/cop; it has its own folder now, so a
            // fix made for one build stops leaking into the other.
            String resBase = "cop";
            if ("soc".equals(gameMode)) {
                resBase = "olr";
            } else if ("cs".equals(gameMode)) {
                resBase = "cs";
            }
            AppLog.i("FileSystem", "Res source folder: " + resBase);

            String[] gamedataAssets = null;
            try {
                gamedataAssets = getAssets().list(resBase + "/gamedata");
            } catch (Exception e) {
                AppLog.w("FileSystem", "Res folder missing in APK: " + resBase);
            }
            if (gamedataAssets != null && gamedataAssets.length > 0) {
                AppLog.i("FileSystem", "Extracting assets to: " + new File(dir, "gamedata").getAbsolutePath());
                extractAssetFolder(resBase + "/gamedata", new File(dir, "gamedata"));
                // Engine fallback reads game_mode from openxray.ltx — keep it
                // in sync with the selected mode (flags -soc/-cs/-cop win anyway).
                // SoC keeps configs in gamedata/config, CS/CoP in gamedata/configs.
                patchGameMode(dir, gameMode);
            } else {
                AppLog.i("FileSystem", "No gamedata in APK assets, using external gamedata.");
            }
            File targetFsgame = new File(dir, "fsgame.ltx");
            installFsgameConfig(resBase, targetFsgame);
            AppLog.i("FileSystem", "Asset check finished.");
            if (!"olr".equals(resBase)) verifyOverlayScripts(dir, gameMode, resBase);
        } catch (Exception e) {
            AppLog.e("FileSystem", "Error creating game directories / extracting assets: " + e.getMessage(), e);
        }
    }

    // The engine mounts game archives (.db) only for the directories listed in
    // fsgame.ltx. A stale or retail fsgame.ltx has no $arch_dir* entries, so the
    // engine ends up with "0 archives" and exits with "Cannot find file
    // system.ltx" while every log still looks healthy. Always install the
    // bundled config, keeping one backup of a previously used file.
    private void installFsgameConfig(String resBase, File targetFsgame) {
        try {
            byte[] bundled;
            try (java.io.InputStream in = getAssets().open(resBase + "/fsgame.ltx")) {
                bundled = readAll(in);
            }
            if (bundled == null || bundled.length == 0) {
                AppLog.w("FileSystem", "Bundled fsgame.ltx missing for " + resBase + ", keeping the existing one.");
                return;
            }

            if (targetFsgame.exists()) {
                byte[] current = readFile(targetFsgame);
                if (Arrays.equals(bundled, current)) {
                    AppLog.i("FileSystem", "fsgame.ltx is up to date (" + bundled.length + " bytes).");
                    return;
                }
                File backup = new File(targetFsgame.getParentFile(), "fsgame.ltx.user.bak");
                if (!backup.exists()) {
                    try (java.io.OutputStream out = new java.io.FileOutputStream(backup)) {
                        out.write(current);
                    }
                    AppLog.i("FileSystem", "Previous fsgame.ltx (" + (current == null ? 0 : current.length)
                            + " bytes) backed up as fsgame.ltx.user.bak");
                }
            }

            try (java.io.OutputStream out = new java.io.FileOutputStream(targetFsgame)) {
                out.write(bundled);
            }
            String text = new String(bundled, java.nio.charset.StandardCharsets.UTF_8);
            if (!text.contains("$arch_dir$")) {
                AppLog.w("FileSystem", "Bundled fsgame.ltx has no $arch_dir$ entry, game archives may not load.");
            }
            AppLog.i("FileSystem", "Installed bundled fsgame.ltx (" + bundled.length + " bytes).");
        } catch (Exception e) {
            AppLog.e("FileSystem", "Could not install fsgame.ltx: " + e.getMessage(), e);
        }
    }

    private static byte[] readAll(java.io.InputStream in) throws java.io.IOException {
        if (in == null) {
            return null;
        }
        java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
        byte[] buffer = new byte[8192];
        int read;
        while ((read = in.read(buffer)) != -1) {
            out.write(buffer, 0, read);
        }
        return out.toByteArray();
    }

    private static byte[] readFile(File file) {
        if (!file.exists()) {
            return null;
        }
        try (java.io.FileInputStream in = new java.io.FileInputStream(file)) {
            return readAll(in);
        } catch (Exception e) {
            return null;
        }
    }

    private void extractAssetFolder(String assetPath, File targetDir) {
        try {
            String[] files = getAssets().list(assetPath);
            if (files == null || files.length == 0) {
                if (!targetDir.getParentFile().exists()) {
                    targetDir.getParentFile().mkdirs();
                }
                boolean shouldExtract = !targetDir.exists() || targetDir.length() == 0 
                        || targetDir.getName().endsWith(".script")
                        || targetDir.getName().endsWith(".vs")
                        || targetDir.getName().endsWith(".ps")
                        || targetDir.getName().endsWith(".s")
                        || targetDir.getName().endsWith(".h");
                if (shouldExtract) {
                    try (java.io.InputStream in = getAssets().open(assetPath);
                         java.io.OutputStream out = new java.io.FileOutputStream(targetDir)) {
                        byte[] buffer = new byte[8192];
                        int read;
                        while ((read = in.read(buffer)) != -1) {
                            out.write(buffer, 0, read);
                        }
                    }
                }
            } else {
                if (!targetDir.exists()) {
                    targetDir.mkdirs();
                }
                for (String file : files) {
                    extractAssetFolder(assetPath + "/" + file, new File(targetDir, file));
                }
            }
        } catch (Exception e) {
            AppLog.e("FileSystem", "extractAssetFolder error for " + assetPath + ": " + e.getMessage(), e);
        }
    }

    // Overlay scripts that must NOT exist in the game directory.
    //
    // Dropping one from res/<mode>/gamedata/scripts/ is not enough on its own:
    // earlier launches already extracted it and it is still on disk, and a loose
    // file shadows the same-named entry inside the .db archives. Without this the
    // retired copy keeps being loaded forever, because extractAssetFolder only
    // ever writes and never deletes.
    //
    // The list is deliberately not hardcoded. res/<mode>/backup/gamedata/scripts/
    // holds both the archive of every retired file and the manifest of what to
    // remove, so putting a file in there is the single action that retires it.
    // That path sits outside gamedata/, which means it ships inside the APK but
    // is never extracted to the device.
    //
    // Clear Sky currently retires 23 files and keeps exactly one overlay
    // (gamedata/scripts/_g.script, the game's own file plus the device()
    // compatibility shim). Everything the game ships is better than a copy we
    // maintain: xrs_dyn_music.script in particular was a foreign generation of
    // the module (music_start/fade_theme vs the game's start_theme/on_actor_update)
    // and broke callbacks for everything that registered through it.
    private void removeRetiredOverlayScripts(File gameDir, String gameMode) {
        String[] retired = listAssets(gameMode + "/backup/gamedata/scripts");
        if (retired == null || retired.length == 0) {
            return;
        }
        File scriptsDir = new File(new File(gameDir, "gamedata"), "scripts");
        if (!scriptsDir.isDirectory()) {
            return;
        }
        for (String name : retired) {
            if (!name.endsWith(".script")) {
                continue;
            }
            File f = new File(scriptsDir, name);
            if (!f.isFile()) {
                continue;
            }
            // Rename rather than unlink, so a failed boot can still be
            // inspected and the file put back by hand.
            File stale = new File(scriptsDir, name + ".overlay_stale");
            if (stale.exists() && !stale.delete()) {
                AppLog.w("FileSystem", "Stale copy blocks retiring " + name + ", leaving it in place.");
                continue;
            }
            if (f.renameTo(stale)) {
                AppLog.i("FileSystem", "Retired overlay script (renamed): " + name);
            } else if (f.delete()) {
                AppLog.i("FileSystem", "Retired overlay script (deleted): " + name);
            } else {
                AppLog.w("FileSystem", "Could not retire overlay script: " + name);
            }
        }
    }

    private String[] listAssets(String assetPath) {
        try {
            return getAssets().list(assetPath);
        } catch (Exception e) {
            AppLog.w("FileSystem", "Could not list assets under " + assetPath);
            return null;
        }
    }

    // Sanity-check that the OpenXRay-specific overlay scripts/configs of the
    // selected game mode are actually on disk before starting the engine.
    // A missing script folder (e.g. a stale manual sync) makes the engine
    // fall back into a broken state — doors with Lua on_use silently fail,
    // touch UI callbacks disappear, etc. So detect it and re-extract.
    private void verifyOverlayScripts(File gameDir, String gameMode, String resBase) {
        // Runs before the presence check on every launch: a loose file on disk
        // shadows the same-named entry inside the .db archives, so an overlay
        // file retired from the APK would otherwise keep winning forever,
        // because extractAssetFolder only ever writes and never deletes.
        if (!"olr".equals(resBase)) {
            removeRetiredOverlayScripts(gameDir, gameMode);
        }

        // Clear Sky keeps its own scripts in the .db archives and retires the whole
        // overlay, except xrs_dyn_music.script which needs the surge_manager
        // guard that a CoP/SoC mix otherwise trips on every frame.
        String[] requiredScripts = "olr".equals(resBase) ? new String[0]
                : "soc".equals(gameMode)
                ? new String[] { "class_registrator.script", "move_mgr.script",
                                 "task_manager.script", "death_manager.script", "ui_main_menu.script" }
                : "cs".equals(gameMode)
                ? new String[] { "xrs_dyn_music.script", "xr_s.script" }
                : new String[] { "_g.script", "ui_main_menu.script",
                                 "xr_s.script", "xr_logic.script" };
        File scriptsDir = new File(new File(gameDir, "gamedata"), "scripts");
        java.util.List<String> missing = new java.util.ArrayList<>();
        if (!scriptsDir.isDirectory()) {
            missing.add("gamedata/scripts (folder missing)");
        } else {
            for (String name : requiredScripts) {
                File f = new File(scriptsDir, name);
                if (!f.isFile() || f.length() == 0) {
                    missing.add("gamedata/scripts/" + name);
                }
            }
        }
        File configDir = new File(new File(gameDir, "gamedata"),
                "soc".equals(gameMode) ? "config" : "configs");
        File openxrayLtx = new File(configDir, "openxray.ltx");
        if (!openxrayLtx.isFile() || openxrayLtx.length() == 0) {
            missing.add("gamedata/" + configDir.getName() + "/openxray.ltx");
        }

        if (!missing.isEmpty()) {
            AppLog.w("FileSystem", "Overlay scripts missing for mode '" + gameMode
                    + "': " + missing + " — re-extracting from APK assets.");
            try {
                extractAssetFolder(resBase + "/gamedata", new File(gameDir, "gamedata"));
                patchGameMode(gameDir, gameMode);
            } catch (Exception e) {
                AppLog.e("FileSystem", "Failed to re-extract overlay: " + e.getMessage(), e);
            }
        } else {
            int count = scriptsDir.list() != null ? scriptsDir.list().length : 0;
            AppLog.i("FileSystem", "Overlay scripts check OK: " + count
                    + " script files present in " + scriptsDir.getAbsolutePath());
        }
    }

    // Keeps [compatibility] game_mode in the extracted openxray.ltx matched
    // with the launcher-selected game (soc/cs/cop).
    private void patchGameMode(File gameDir, String gameMode) {
        try {
            // SoC: gamedata/config, Clear Sky / Call of Pripyat: gamedata/configs.
            String configName = "soc".equals(gameMode) ? "config" : "configs";
            File configDir = new File(new File(gameDir, "gamedata"), configName);
            if (!configDir.exists()) {
                configDir.mkdirs();
            }
            File cfg = new File(configDir, "openxray.ltx");
            java.util.List<String> lines = new java.util.ArrayList<>();
            if (cfg.exists()) {
                try (java.io.BufferedReader reader = new java.io.BufferedReader(new java.io.FileReader(cfg))) {
                    String line;
                    while ((line = reader.readLine()) != null) {
                        lines.add(line);
                    }
                }
            }
            boolean inCompat = false;
            boolean compatFound = false;
            boolean modeSet = false;
            int compatHeaderIndex = -1;
            for (int i = 0; i < lines.size(); i++) {
                String trimmed = lines.get(i).trim();
                if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
                    inCompat = "[compatibility]".equalsIgnoreCase(trimmed);
                    if (inCompat) {
                        compatFound = true;
                        compatHeaderIndex = i;
                    }
                } else if (inCompat && trimmed.startsWith("game_mode")) {
                    lines.set(i, "game_mode = " + gameMode);
                    modeSet = true;
                }
            }
            if (!compatFound) {
                lines.add("[compatibility]");
                lines.add("game_mode = " + gameMode);
            } else if (!modeSet && compatHeaderIndex >= 0) {
                lines.add(compatHeaderIndex + 1, "game_mode = " + gameMode);
            }
            try (java.io.BufferedWriter writer = new java.io.BufferedWriter(new java.io.FileWriter(cfg))) {
                for (String l : lines) {
                    writer.write(l);
                    writer.newLine();
                }
            }
            AppLog.i("FileSystem", "Patched openxray.ltx game_mode=" + gameMode);
        } catch (Exception e) {
            AppLog.e("FileSystem", "patchGameMode failed: " + e.getMessage(), e);
        }
    }

    // CMainMenu sends this on SDL's Android UI thread.
    private static final int COMMAND_TOUCH_MENU = COMMAND_USER;
    private static final int COMMAND_TOUCH_DIALOG = COMMAND_USER + 1;
    private boolean touchMenuOpen;
    private boolean touchDialogOpen;

    @Override
    protected boolean onUnhandledMessage(int command, Object param) {
        if ((command == COMMAND_TOUCH_MENU || command == COMMAND_TOUCH_DIALOG) && param instanceof Integer) {
            boolean open = ((Integer) param) != 0;
            if (command == COMMAND_TOUCH_MENU) {
                touchMenuOpen = open;
            } else {
                touchDialogOpen = open;
            }
            if (mTouchOverlay != null) {
                mTouchOverlay.setVisibility(touchMenuOpen || touchDialogOpen ? View.INVISIBLE : View.VISIBLE);
            }
            return true;
        }
        return super.onUnhandledMessage(command, param);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (touchDialogOpen && !touchMenuOpen && event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            if (event.getAction() == KeyEvent.ACTION_UP && !event.isCanceled()) {
                onBackPressed();
            }
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    public void onBackPressed() {
        if (touchDialogOpen && !touchMenuOpen) {
            SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_ESCAPE);
            SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_ESCAPE);
            return;
        }
        super.onBackPressed();
    }

    private void setupTouchControls() {
        if (mLayout == null) {
            AppLog.w("TouchControls", "mLayout is null, cannot setup touch overlay");
            return;
        }

        mTouchOverlay = new TouchOverlayView(this);

        Intent intent = getIntent();
        if (intent != null) {
            float opacity = intent.getFloatExtra("extra_touch_opacity", 0.6f);
            float scale = intent.getFloatExtra("extra_touch_scale", 1.0f);
            float sens = intent.getFloatExtra("extra_look_sensitivity", 1.0f);

            AppLog.i("TouchControls", "Touch controls params: opacity=" + opacity + ", scale=" + scale + ", sens=" + sens);
            // Кнопка детектора артефактов — только CoP и Clear Sky.
            String mode = intent.getStringExtra("extra_game_mode");
            boolean detector = "cop".equals(mode) || "cs".equals(mode);
            AppLog.i("TouchControls", "Detector button enabled: " + detector + " (mode=" + mode + ")");
            mTouchOverlay.setDetectorEnabled(detector);
            mTouchOverlay.setOverlayAlpha(opacity);
            mTouchOverlay.setControlsScale(scale);
            mTouchOverlay.setLookSensitivity(sens);
        }

        android.widget.RelativeLayout.LayoutParams params = new android.widget.RelativeLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT
        );
        mLayout.addView(mTouchOverlay, params);
        AppLog.i("TouchControls", "Touch overlay added to layout.");
    }

}
