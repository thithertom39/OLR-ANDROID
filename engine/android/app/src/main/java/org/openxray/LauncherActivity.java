package org.openxray;

import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.util.DisplayMetrics;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import android.content.ClipData;
import android.content.ClipboardManager;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.util.Locale;

public class LauncherActivity extends AppCompatActivity {

    private static final String PREFS_NAME = "openxray_launcher_prefs";
    private static final String OLR_BLOOM_MIGRATED = "olr_bloom_threshold_migrated";
    private static final String OLR_TONEMAP_MIGRATED = "olr_tonemap_defaults_migrated";
    // OLR 2.5 FIXX 59 values and the previously inherited engine defaults.
    private static final String[][] OLR_TONEMAP_DEFAULTS = {
        {"r2_tonemap_adaptation", "4.0", "1.0"},
        {"r2_tonemap_amount", "1.0", "0.7"},
        {"r2_tonemap_middlegray", "0.85", "1.0"},
    };
    private static final int BROWSE_FOLDER_REQUEST_CODE = 2002;

    // Keep the tablet-tested fixes available without a pasted command line.
    private static final String NORMAL_LAUNCH_ARGS =
            "-xclsx -gles_shadow_detach -gles_stream_upload -gles_uniform_batch";
    private static final String DIAGNOSTIC_LAUNCH_ARGS =
            "-gles_shadow_audit -gles_spot_shadow_audit -gles_spot_writer_audit -gles_perf";
    private static final String[] PROFILE_FLAGS = {
            "-xclsx", "-gles_shadow_detach", "-gles_stream_upload", "-gles_uniform_batch",
            "-gles_shadow_audit", "-gles_spot_shadow_audit", "-gles_spot_writer_audit", "-gles_perf"
    };
    private static final String[] REJECTED_FLAGS = {
            "-gles_state_rebind", "-gles_stream_orphan", "-gles_stream_sync"
    };
    // Retain existing diagnostic intent, but do not enable dumps by default.
    private static final String[] LEGACY_DIAGNOSTIC_FLAGS = {
            "-ai_task_audit", "-gles_spot_shadow_dump", "-trade_audit", "-physics_audit"
    };

    private EditText mEditGamePath;
    private Button mBtnBrowsePath;
    private Button mBtnCheckPath;
    private Button mBtnViewLogs;
    private Button mBtnReportBug;
    private TextView mTextPathStatus;

    private RadioGroup mRadioGroupGameMode;
    private RadioButton mRadioSoc;
    private RadioButton mRadioCs;
    private RadioButton mRadioCop;

    private CheckBox mCheckNoIntro;
    private CheckBox mCheckNoSound;
    private CheckBox mCheckNoShadows;
    private CheckBox mCheckDLights;
    private CheckBox mCheckNoSun;
    private CheckBox mCheckFastDxt;
    private CheckBox mCheckUnlockFps;
    private Spinner mSpinnerGraphicsPreset;
    private java.util.List<GraphicsPresetItem> mGraphicsPresetList = new java.util.ArrayList<>();
    private ArrayAdapter<GraphicsPresetItem> mGraphicsPresetAdapter;
    private Spinner mSpinnerResolution;
    private java.util.List<ResolutionItem> mResolutionList = new java.util.ArrayList<>();
    private ArrayAdapter<ResolutionItem> mResolutionAdapter;
    private Spinner mSpinnerRenderBackend;
    private TextView mTextRenderBackendStatus;
    private java.util.List<RenderBackendItem> mRenderBackendList = new java.util.ArrayList<>();
    private ArrayAdapter<RenderBackendItem> mRenderBackendAdapter;
    private EditText mEditCustomArgs;
    private Spinner mSpinnerLaunchProfile;
    private TextView mTextLaunchProfileStatus;
    private CheckBox mCheckFxaa;
    private Spinner mSpinnerFxaaQuality;
    private boolean mFxaaDirty;
    private boolean mFxaaDetailDirty;
    private int mFxaaDetailOriginal;
    private String mFxaaModePath;

    private TextView mLabelLookSens;
    private SeekBar mSeekLookSens;

    private TextView mLabelTouchOpacity;
    private SeekBar mSeekTouchOpacity;

    private TextView mLabelTouchScale;
    private SeekBar mSeekTouchScale;

    private Button mBtnEditTouchLayout;


    private Button mBtnStartGame;
    private Button mBtnOpenSettings;
    private Button mBtnSettingsBack;
    private View mLauncherHero;
    private View mLauncherPageContainer;
    private View mLauncherMainPage;
    private View mLauncherSettingsPage;
    private View mLauncherColumns;
    private View mLauncherMainOverlay;
    private LauncherBackdropView mLauncherBackdrop;
    private boolean mShowingSettings;

    private SharedPreferences mPrefs;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_launcher);
        LauncherSkin.apply(this);

        mPrefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String savedPath = new File(getExternalFilesDir(null), DATA_FOLDER).getAbsolutePath();
        AppLog.init(this, savedPath);
        LifecycleTrace.record(this, "onCreate");
        AppLog.i("Launcher", "LauncherActivity onCreate started");

        initViews();
        loadPreferences();
        showSettingsPage(savedInstanceState != null
                && savedInstanceState.getBoolean("launcher_showing_settings", false));

        checkGamePathStatus();
        offerCrashReportIfPending();
        AppLog.i("Launcher", "LauncherActivity onCreate finished");
    }

    /**
     * A session marker that survived the last run means the process died
     * (native SIGSEGV/SIGABRT or an uncaught Java exception), because a clean
     * exit deletes it in OpenXRayActivity.onDestroy. Offer to send the logs to
     * GitHub before the user can touch anything else.
     */
    private void offerCrashReportIfPending() {
        final CrashReporter.CrashInfo info = CrashReporter.checkPendingCrash(this);
        if (info == null) return;

        final String full = CrashReporter.buildReport(info, this);
        StringBuilder msg = new StringBuilder();
        msg.append(CrashReporter.describeCrash(info));
        if (info.crashLine != null) {
            msg.append("\n\n").append(info.crashLine);
        }
        msg.append('\n');

        androidx.appcompat.app.AlertDialog.Builder b = new AlertDialog.Builder(this)
                .setTitle("Previous session ended unexpectedly")
                .setCancelable(true);

        if (GitHubIssueReporter.isConfigured()) {
            // One tap: the logs go straight into an issue, no browser, no CAPTCHA.
            msg.append("\nSend these logs to the GitHub issue tracker?");
            b.setMessage(msg.toString())
                .setPositiveButton("Send report", (d, w) -> sendCrashReportToGitHub(info));
        } else {
            msg.append("\nAutomatic reporting is not enabled in this build "
                    + "(no repository token was baked in at compile time).");
            b.setMessage(msg.toString());
        }
        b.setNeutralButton("Copy logs", (d, w) -> copyLogs(full));
        b.setNegativeButton("Skip", (d, w) -> CrashReporter.consumePendingCrash(this));
        b.show();
    }

    private void copyLogs(String text) {
        ClipboardManager clipboard = (ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
        if (clipboard != null) {
            clipboard.setPrimaryClip(ClipData.newPlainText("OpenXRay Logs", text));
            Toast.makeText(this, "Logs copied to clipboard", Toast.LENGTH_SHORT).show();
        }
    }

    /** One tap -> one issue. Falls back to a copyable report if it cannot send. */
    private void sendCrashReportToGitHub(final CrashReporter.CrashInfo info) {
        sendCrashReportToGitHub(info, true);
    }

    private void sendCrashReportToGitHub(final CrashReporter.CrashInfo info,
                                         final boolean consumeOnSuccess) {
        Toast.makeText(this, "Sending report to GitHub...", Toast.LENGTH_SHORT).show();
        GitHubIssueReporter.report(info, this, result -> {
            if (result.success) {
                Toast.makeText(this, result.duplicate
                        ? "Appended to an existing issue" : "Issue created", Toast.LENGTH_LONG).show();
                // Only a real crash report may clear the pending marker: a
                // diagnostics upload must not silently discard a crash.
                if (consumeOnSuccess) CrashReporter.consumePendingCrash(this);
                GitHubIssueReporter.openIssue(this, result.issueUrl);
            } else {
                // Network/token trouble must not eat the report: keep the text
                // reachable so the user can paste it somewhere by hand.
                Toast.makeText(this, result.error, Toast.LENGTH_LONG).show();
                offerCrashReportFallback(info);
            }
        });
    }

    private void offerCrashReportFallback(final CrashReporter.CrashInfo info) {
        final String full = CrashReporter.buildReport(info, this);
        new AlertDialog.Builder(this)
                .setTitle("Report not sent")
                .setMessage("The logs are still available. Copy them and paste them "
                        + "into a new issue, or share a zip instead.")
                .setPositiveButton("Retry", (d, w) -> sendCrashReportToGitHub(info, false))
                .setNeutralButton("Copy logs", (d, w) -> copyLogs(full))
                .setNegativeButton("Close", null)
                .show();
    }

    /**
     * Manual bug report: the current session's logs, sent to the same issue
     * tracker with one tap. Always available, whether or not a crash happened.
     */
    private void showBugTrackerDialog() {
        String repo = BuildConfig.GITHUB_REPO;
        StringBuilder msg = new StringBuilder();
        msg.append("Report a problem with OpenXRay Android.\n\n");
        msg.append("Issue tracker: ").append(repo).append('\n');
        if (GitHubIssueReporter.isConfigured()) {
            msg.append("\nOne tap sends the engine log, logcat and app log as a new "
                    + "issue. Nothing else is uploaded.");
        } else {
            msg.append("\nAutomatic reporting is not enabled in this build, so you will "
                    + "get the logs to paste into a new issue by hand.");
        }

        androidx.appcompat.app.AlertDialog.Builder b = new AlertDialog.Builder(this)
                .setTitle("Report a bug")
                .setMessage(msg.toString())
                .setNegativeButton("Close", null);

        if (GitHubIssueReporter.isConfigured()) {
            b.setPositiveButton("Send report", (d, w) ->
                    sendCrashReportToGitHub(CrashReporter.forCurrentSession(this), false));
        }
        // Falls back to the repository page, so this button is useful whether or
        // not a token was baked in.
        b.setNeutralButton("Open tracker", (d, w) -> {
            try {
                startActivity(new Intent(Intent.ACTION_VIEW,
                        Uri.parse("https://github.com/" + repo + "/issues")));
            } catch (Exception e) {
                Toast.makeText(this, "Could not open the browser", Toast.LENGTH_SHORT).show();
            }
        });
        // The crash dialog offers "Copy logs", but this manual path only offered
        // "Send report" or the browser -- so once the crash marker had been
        // consumed there was no way left to get the report text out at all.
        // Always keep it reachable, token or no token.
        b.setNeutralButton("Copy logs", (d, w) ->
                copyLogs(CrashReporter.buildReport(CrashReporter.forCurrentSession(this), this)));
        b.show();
    }

    private void initViews() {
        mEditGamePath = findViewById(R.id.edit_game_path);
        mBtnBrowsePath = findViewById(R.id.btn_browse_path);
        mBtnCheckPath = findViewById(R.id.btn_check_path);
        mBtnViewLogs = findViewById(R.id.btn_view_logs);
        mBtnReportBug = findViewById(R.id.btn_report_bug);
        mTextPathStatus = findViewById(R.id.text_path_status);

        mBtnBrowsePath.setOnClickListener(v -> openFolderPicker());
        mBtnCheckPath.setOnClickListener(v -> checkGamePathStatus());
        if (mBtnViewLogs != null) {
            mBtnViewLogs.setOnClickListener(v -> showLogsDialog());
        }
        if (mBtnReportBug != null) {
            mBtnReportBug.setOnClickListener(v -> showBugTrackerDialog());
        }

        mRadioGroupGameMode = findViewById(R.id.radiogroup_gamemode);
        mRadioSoc = findViewById(R.id.radio_soc);
        mRadioCs = findViewById(R.id.radio_cs);
        mRadioCop = findViewById(R.id.radio_cop);
        if (mRadioGroupGameMode != null) {
            // Each game lives in its own subfolder (soc/cs/cop) — re-check files on switch.
            mRadioGroupGameMode.setOnCheckedChangeListener((group, checkedId) -> checkGamePathStatus());
        }

        mCheckNoIntro = findViewById(R.id.check_nointro);
        mCheckNoSound = findViewById(R.id.check_nosound);
        mCheckNoShadows = findViewById(R.id.check_noshadows);
        mCheckDLights = findViewById(R.id.check_dlights);
        mCheckNoSun = findViewById(R.id.check_nosun);
        mCheckFastDxt = findViewById(R.id.check_fastdxt);
        mCheckUnlockFps = findViewById(R.id.check_unlock_fps);
        mSpinnerGraphicsPreset = findViewById(R.id.spinner_graphics_preset);
        setupGraphicsPresetSpinner();
        mSpinnerResolution = findViewById(R.id.spinner_resolution);
        setupResolutionSpinner();
        mSpinnerRenderBackend = findViewById(R.id.spinner_render_backend);
        mTextRenderBackendStatus = findViewById(R.id.text_render_backend_status);
        setupRenderBackendSpinner();
        mEditCustomArgs = findViewById(R.id.edit_custom_args);
        mSpinnerLaunchProfile = findViewById(R.id.spinner_launch_profile);
        mTextLaunchProfileStatus = findViewById(R.id.text_launch_profile_status);
        ArrayAdapter<String> profileAdapter = new ArrayAdapter<>(this,
                R.layout.item_resolution_spinner, new String[] {"Normal (play)", "Diagnostic (logs)"});
        profileAdapter.setDropDownViewResource(R.layout.item_resolution_dropdown);
        mSpinnerLaunchProfile.setAdapter(profileAdapter);
        mTextLaunchProfileStatus.setOnClickListener(v -> showLaunchProfileDetails());
        mEditCustomArgs.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            @Override public void onTextChanged(CharSequence s, int start, int before, int count) {
                updateLaunchProfileStatus();
            }
            @Override public void afterTextChanged(Editable s) {}
        });
        mCheckFxaa = findViewById(R.id.check_fxaa);
        mCheckFxaa.setOnClickListener(v -> {
            mFxaaDirty = true;
            updateFxaaLabel();
        });
        mSpinnerFxaaQuality = findViewById(R.id.spinner_fxaa_quality);
        ArrayAdapter<String> aaQualityAdapter = new ArrayAdapter<>(this,
                R.layout.item_resolution_spinner, new String[] {"Fast", "Fine edges"});
        aaQualityAdapter.setDropDownViewResource(R.layout.item_resolution_dropdown);
        mSpinnerFxaaQuality.setAdapter(aaQualityAdapter);
        mSpinnerFxaaQuality.setOnItemSelectedListener(new android.widget.AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(android.widget.AdapterView<?> parent, View view, int position, long id) {
                if (position != parent.getSelectedItemPosition()) return;
                mFxaaDetailDirty = position != mFxaaDetailOriginal;
                updateFxaaLabel();
            }
            @Override public void onNothingSelected(android.widget.AdapterView<?> parent) {}
        });
        androidx.appcompat.widget.TooltipCompat.setTooltipText(mSpinnerFxaaQuality,
                "Fast keeps the current balance. Fine edges smooths more dark detail and may lower FPS. "
                + "Available for Shadow of Chernobyl; saved on launch.");

        mLabelLookSens = findViewById(R.id.label_look_sens);
        mSeekLookSens = findViewById(R.id.seek_look_sens);

        mLabelTouchOpacity = findViewById(R.id.label_touch_opacity);
        mSeekTouchOpacity = findViewById(R.id.seek_touch_opacity);

        mLabelTouchScale = findViewById(R.id.label_touch_scale);
        mSeekTouchScale = findViewById(R.id.seek_touch_scale);

        mBtnEditTouchLayout = findViewById(R.id.btn_edit_touch_layout);

        if (mBtnEditTouchLayout != null) {
            mBtnEditTouchLayout.setOnClickListener(v -> {
                Intent intent = new Intent(this, ControlLayoutEditorActivity.class);
                startActivity(intent);
            });
        }

        mBtnStartGame = findViewById(R.id.btn_start_game);

        mBtnCheckPath.setOnClickListener(v -> checkGamePathStatus());

        mEditGamePath.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            @Override public void onTextChanged(CharSequence s, int start, int count, int after) {}
            @Override
            public void afterTextChanged(Editable s) {
                checkGamePathStatus();
            }
        });

        // Seekbar listeners
        mSeekLookSens.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                float val = Math.max(5, progress) / 10.0f;
                mLabelLookSens.setText(String.format(Locale.US, "Camera sensitivity: %.1fx", val));
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        mSeekTouchOpacity.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                int val = Math.max(20, progress);
                mLabelTouchOpacity.setText(String.format(Locale.US, "Button opacity: %d%%", val));
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        mSeekTouchScale.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                int val = Math.max(70, progress);
                mLabelTouchScale.setText(String.format(Locale.US, "Button size: %d%%", val));
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        mBtnStartGame = findViewById(R.id.btn_start_game);
        mBtnOpenSettings = findViewById(R.id.btn_open_settings);
        mBtnSettingsBack = findViewById(R.id.btn_settings_back);
        mLauncherHero = findViewById(R.id.launcher_hero);
        mLauncherPageContainer = findViewById(R.id.launcher_page_container);
        mLauncherMainPage = findViewById(R.id.launcher_actions_column);
        mLauncherSettingsPage = findViewById(R.id.launcher_settings_screen);
        mLauncherColumns = findViewById(R.id.launcher_columns);
        mLauncherMainOverlay = findViewById(R.id.launcher_main_overlay);
        mLauncherBackdrop = findViewById(R.id.launcher_backdrop);

        getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        mBtnOpenSettings.setOnClickListener(v -> showSettingsPage(true));
        mBtnSettingsBack.setOnClickListener(v -> showSettingsPage(false));
        mBtnStartGame.setOnClickListener(v -> launchGame(false));
    }

    /** Switch pages without reinflating controls, preserving all settings and listeners. */
    private void showSettingsPage(boolean showSettings) {
        if (mLauncherMainPage == null || mLauncherSettingsPage == null) return;
        if (mShowingSettings && !showSettings) savePreferences();
        mShowingSettings = showSettings;
        mLauncherMainPage.setVisibility(showSettings ? View.GONE : View.VISIBLE);
        mLauncherSettingsPage.setVisibility(showSettings ? View.VISIBLE : View.GONE);
        mLauncherColumns.setVisibility(showSettings ? View.VISIBLE : View.GONE);
        mLauncherMainOverlay.setVisibility(showSettings ? View.GONE : View.VISIBLE);
        mLauncherBackdrop.setSettingsPage(showSettings);

        // The launcher home is an independent lower-left overlay. The two
        // settings columns retain their original large-screen proportions.
        if (getResources().getConfiguration().smallestScreenWidthDp >= 600) {
            setLauncherColumnWeight(mLauncherHero, showSettings ? 0.265f : 0.70f);
            setLauncherColumnWeight(mLauncherPageContainer, showSettings ? 0.735f : 0.30f);
        }
    }

    private void setLauncherColumnWeight(View view, float weight) {
        android.widget.LinearLayout.LayoutParams params =
                (android.widget.LinearLayout.LayoutParams) view.getLayoutParams();
        params.weight = weight;
        view.setLayoutParams(params);
    }

    @Override
    public void onBackPressed() {
        if (mShowingSettings) {
            showSettingsPage(false);
        } else {
            super.onBackPressed();
        }
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        outState.putBoolean("launcher_showing_settings", mShowingSettings);
        super.onSaveInstanceState(outState);
    }

    private void loadPreferences() {
        String defaultPath = new File(getExternalFilesDir(null), DATA_FOLDER).getAbsolutePath();
        mEditGamePath.setText(mPrefs.getString("olr_assets_root", defaultPath));

        mCheckNoIntro.setChecked(mPrefs.getBoolean("nointro", false));
        mCheckNoSound.setChecked(mPrefs.getBoolean("nosound", false));
        mCheckNoShadows.setChecked(mPrefs.getBoolean("noshadows", false));
        mCheckDLights.setChecked(mPrefs.getBoolean("dlights", true));
        if (mCheckNoSun != null) {
            mCheckNoSun.setChecked(mPrefs.getBoolean("nosun", false));
        }
        if (mCheckFastDxt != null) {
            mCheckFastDxt.setChecked(mPrefs.getBoolean("fastdxt", true));
        }
        mCheckUnlockFps.setChecked(mPrefs.getBoolean("unlock_fps", false));

        String savedPreset = mPrefs.getString("graphics_preset", "none");
        int presetIndex = 0;
        for (int i = 0; i < mGraphicsPresetList.size(); i++) {
            if (mGraphicsPresetList.get(i).id.equals(savedPreset)) {
                presetIndex = i;
                break;
            }
        }
        if (mSpinnerGraphicsPreset != null) {
            mSpinnerGraphicsPreset.setSelection(presetIndex);
        }

        // No destructive migration: keep the previous text and an original
        // backup. Profiles only filter known flags from the effective arguments.
        String customArgs = mPrefs.getString("custom_args", "");
        if (!mPrefs.getBoolean("launch_profiles_v1_initialized", false)) {
            mPrefs.edit()
                    .putString("custom_args_before_profiles_v1", customArgs)
                    .putString("launch_profile", "normal")
                    .putBoolean("launch_profiles_v1_initialized", true)
                    .apply();
        }
        mEditCustomArgs.setText(customArgs);
        mSpinnerLaunchProfile.setSelection(
                "diagnostic".equals(mPrefs.getString("launch_profile", "normal")) ? 1 : 0);
        mSpinnerLaunchProfile.setOnItemSelectedListener(new android.widget.AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(android.widget.AdapterView<?> parent, View view, int position, long id) {
                mPrefs.edit().putString("launch_profile", position == 1 ? "diagnostic" : "normal").apply();
                updateLaunchProfileStatus();
            }
            @Override public void onNothingSelected(android.widget.AdapterView<?> parent) {}
        });
        updateLaunchProfileStatus();
        refreshFxaaFromConfig();

        int lookSens = mPrefs.getInt("look_sens", 10);
        mSeekLookSens.setProgress(lookSens);
        mLabelLookSens.setText(String.format(Locale.US, "Camera sensitivity: %.1fx", lookSens / 10.0f));

        int opacity = mPrefs.getInt("touch_opacity", 60);
        mSeekTouchOpacity.setProgress(opacity);
        mLabelTouchOpacity.setText(String.format(Locale.US, "Button opacity: %d%%", opacity));

        int scale = mPrefs.getInt("touch_scale", 100);
        mSeekTouchScale.setProgress(scale);
        mLabelTouchScale.setText(String.format(Locale.US, "Button size: %d%%", scale));

        int savedW = mPrefs.getInt("res_width", 0);
        int savedH = mPrefs.getInt("res_height", 0);
        int selectedIndex = 0;
        for (int i = 0; i < mResolutionList.size(); i++) {
            ResolutionItem item = mResolutionList.get(i);
            if (item.width == savedW && item.height == savedH) {
                selectedIndex = i;
                break;
            }
        }
        if (mSpinnerResolution != null) {
            mSpinnerResolution.setSelection(selectedIndex);
        }

        String savedBackend = mPrefs.getString("render_backend", "native");
        int backendIndex = 0;
        for (int i = 0; i < mRenderBackendList.size(); i++) {
            RenderBackendItem item = mRenderBackendList.get(i);
            if (item.id.equals(savedBackend)) {
                backendIndex = i;
                break;
            }
        }
        if (mSpinnerRenderBackend != null) {
            mSpinnerRenderBackend.setSelection(backendIndex);
            updateRenderBackendStatus();
        }
    }

    private void savePreferences() {

        mPrefs.edit()
            .putString("game_path", mEditGamePath.getText().toString().trim())
            .putBoolean("nointro", mCheckNoIntro.isChecked())
            .putBoolean("nosound", mCheckNoSound.isChecked())
            .putBoolean("noshadows", mCheckNoShadows.isChecked())
            .putBoolean("dlights", mCheckDLights.isChecked())
            .putBoolean("nosun", mCheckNoSun != null && mCheckNoSun.isChecked())
            .putBoolean("fastdxt", mCheckFastDxt != null && mCheckFastDxt.isChecked())
            .putBoolean("unlock_fps", mCheckUnlockFps.isChecked())
            .putString("custom_args", mEditCustomArgs.getText().toString().trim())
            .putString("launch_profile", isDiagnosticProfile() ? "diagnostic" : "normal")
            .putInt("look_sens", Math.max(5, mSeekLookSens.getProgress()))
            .putInt("touch_opacity", Math.max(20, mSeekTouchOpacity.getProgress()))
            .putInt("touch_scale", Math.max(70, mSeekTouchScale.getProgress()))
            .apply();

        if (mSpinnerResolution != null && mSpinnerResolution.getSelectedItem() instanceof ResolutionItem) {
            ResolutionItem item = (ResolutionItem) mSpinnerResolution.getSelectedItem();
            mPrefs.edit()
                .putInt("res_width", item.width)
                .putInt("res_height", item.height)
                .apply();
        }

        if (mSpinnerRenderBackend != null && mSpinnerRenderBackend.getSelectedItem() instanceof RenderBackendItem) {
            RenderBackendItem item = (RenderBackendItem) mSpinnerRenderBackend.getSelectedItem();
            mPrefs.edit()
                .putString("render_backend", item.id)
                .apply();
        }

        if (mSpinnerGraphicsPreset != null && mSpinnerGraphicsPreset.getSelectedItem() instanceof GraphicsPresetItem) {
            GraphicsPresetItem item = (GraphicsPresetItem) mSpinnerGraphicsPreset.getSelectedItem();
            mPrefs.edit()
                .putString("graphics_preset", item.id)
                .apply();
        }
    }

    private static final String DATA_FOLDER = "olr";

    private boolean isDiagnosticProfile() {
        return mSpinnerLaunchProfile != null && mSpinnerLaunchProfile.getSelectedItemPosition() == 1;
    }

    private static boolean isProfileFlag(String token) {
        for (String flag : PROFILE_FLAGS) if (flag.equals(token)) return true;
        return false;
    }

    private static boolean isRejectedFlag(String token) {
        for (String flag : REJECTED_FLAGS) if (flag.equals(token)) return true;
        // The legacy native rebind switch uses strstr rather than token matching.
        return token.contains("-gles_state_rebind");
    }

    private static boolean isLegacyDiagnosticFlag(String token) {
        for (String flag : LEGACY_DIAGNOSTIC_FLAGS) if (flag.equals(token)) return true;
        return false;
    }

    private static String buildProfileArguments(String customArgs, boolean diagnostic) {
        String raw = customArgs == null ? "" : customArgs;
        StringBuilder custom = new StringBuilder();
        java.util.Set<String> legacyDiagnostics = new java.util.LinkedHashSet<>();
        java.util.regex.Matcher matcher = java.util.regex.Pattern
                .compile("([^\"\\s]+|\"[^\"]*\")").matcher(raw);
        int copiedThrough = 0;
        while (matcher.find()) {
            String token = matcher.group();
            if (token.startsWith("\"") && token.endsWith("\"") && token.length() >= 2) {
                token = token.substring(1, token.length() - 1);
            }
            boolean legacyDiagnostic = isLegacyDiagnosticFlag(token);
            if (legacyDiagnostic && diagnostic) legacyDiagnostics.add(token);
            if (isProfileFlag(token) || isRejectedFlag(token) || legacyDiagnostic) {
                custom.append(raw, copiedThrough, matcher.start());
                copiedThrough = matcher.end();
            }
        }
        custom.append(raw, copiedThrough, raw.length());
        String extra = custom.toString().trim();
        StringBuilder legacyArgs = new StringBuilder();
        for (String flag : legacyDiagnostics) legacyArgs.append(" ").append(flag);
        return NORMAL_LAUNCH_ARGS + (diagnostic ? " " + DIAGNOSTIC_LAUNCH_ARGS : "")
                + legacyArgs
                + (extra.isEmpty() ? "" : " " + extra);
    }

    private void updateLaunchProfileStatus() {
        if (mTextLaunchProfileStatus == null) return;
        mTextLaunchProfileStatus.setText(isDiagnosticProfile()
                ? "Diagnostics active · details"
                : "Normal play · diagnostics paused · details");
    }

    private void showLaunchProfileDetails() {
        String message = "Normal includes the tested shadow, stream upload and uniform fixes. "
                + "Diagnostic adds bounded shadow audits and periodic performance logs.\n\n"
                + "Your additional arguments are retained. Known profile flags are supplied once by the selected profile; "
                + "saved audit flags are paused in Normal and enabled in Diagnostic. "
                + "Unknown custom arguments are passed through.\n\n"
                + "Previously saved task/trade/physics audits and shadow dumps are restored only in Diagnostic; "
                + "shadow dumps are not added to new configurations.\n\n"
                + "The rejected state rebind, stream orphan and stream sync experiments stay paused in both profiles.\n\n"
                + "Effective profile arguments:\n"
                + buildProfileArguments(mEditCustomArgs.getText().toString(), isDiagnosticProfile());
        AlertDialog.Builder dialog = new AlertDialog.Builder(this)
                .setTitle("Launch profile")
                .setMessage(message)
                .setPositiveButton("Close", null);
        if (mPrefs.contains("custom_args_before_profiles_v1")) {
            dialog.setNeutralButton("Restore original arguments", (d, w) ->
                    mEditCustomArgs.setText(mPrefs.getString("custom_args_before_profiles_v1", "")));
        }
        dialog.show();
    }

    private static File findUserLtx(String modePath) {
        File[] candidates = {
                new File(modePath, "_appdata_/user.ltx"),
                new File(modePath, "appdata/user.ltx"),
                new File(modePath, "user.ltx")
        };
        for (File candidate : candidates) if (userLtxExists(candidate)) return candidate;
        return candidates[0];
    }

    private static boolean userLtxExists(File target) {
        return target.exists() || new File(target.getPath() + ".bak").exists();
    }

    private static java.util.List<String> readUserLtxLines(File target) throws java.io.IOException {
        java.util.List<String> lines = new java.util.ArrayList<>();
        if (!userLtxExists(target)) return lines;
        // openRead restores a backup after an interrupted atomic write.
        try (BufferedReader reader = new BufferedReader(new java.io.InputStreamReader(
                new android.util.AtomicFile(target).openRead(), java.nio.charset.StandardCharsets.UTF_8))) {
            String line;
            while ((line = reader.readLine()) != null) lines.add(line);
        }
        return lines;
    }

    private static String settingValue(String line, String key) {
        String trimmed = line.trim();
        if (!trimmed.startsWith(key) || trimmed.length() == key.length()
                || !Character.isWhitespace(trimmed.charAt(key.length()))) return null;
        String value = trimmed.substring(key.length()).trim();
        int comment = value.indexOf(';');
        return (comment >= 0 ? value.substring(0, comment) : value).trim();
    }

    private static String lastSettingValue(java.util.List<String> lines, String key) {
        String result = null;
        for (String line : lines) {
            String value = settingValue(line, key);
            if (value != null) result = value;
        }
        return result;
    }

    private static boolean matchesTonemapDefault(String value, String inherited) {
        if (value == null) return true;
        try {
            return Float.parseFloat(value) == Float.parseFloat(inherited);
        } catch (NumberFormatException e) {
            return false;
        }
    }

    private static Boolean readFxaaSetting(File target) throws java.io.IOException {
        if (!userLtxExists(target)) return null;
        Boolean enabled = null;
        for (String line : readUserLtxLines(target)) {
            String value = settingValue(line, "r2_aa");
            if ("on".equalsIgnoreCase(value) || "1".equals(value)) enabled = true;
            else if ("off".equalsIgnoreCase(value) || "0".equals(value)) enabled = false;
        }
        return enabled;
    }

    private static int readFxaaDetailSetting(File target) throws java.io.IOException {
        int mode = 0; // Missing setting uses the engine's Fast default.
        for (String line : readUserLtxLines(target)) {
            String value = settingValue(line, "r2_aa_detail_mode");
            if ("0".equals(value)) mode = 0;
            else if ("1".equals(value)) mode = 1;
        }
        return mode;
    }

    private void refreshFxaaFromConfig() {
        if (mCheckFxaa == null || mEditGamePath == null) return;
        String basePath = mEditGamePath.getText().toString().trim();
        String modePath = basePath.isEmpty() ? "" : basePath;
        if ((mFxaaDirty || mFxaaDetailDirty) && modePath.equals(mFxaaModePath)) return;
        mFxaaModePath = modePath;
        mFxaaDirty = false;
        mFxaaDetailDirty = false;
        try {
            File config = modePath.isEmpty() ? null : findUserLtx(modePath);
            Boolean enabled = config == null ? null : readFxaaSetting(config);
            mFxaaDetailOriginal = config == null ? 0 : readFxaaDetailSetting(config);
            mSpinnerFxaaQuality.setSelection(mFxaaDetailOriginal);
            // R2FLAG_AA is absent from the engine's initial flags: no setting means off.
            mCheckFxaa.setChecked(Boolean.TRUE.equals(enabled));
            mCheckFxaa.setEnabled(!modePath.isEmpty());
            updateFxaaLabel();
            mCheckFxaa.setContentDescription("FXAA " + (mCheckFxaa.isChecked() ? "on" : "off")
                    + ". Reads r2_aa from this game's user.ltx; changes are saved on launch.");
        } catch (java.io.IOException e) {
            mCheckFxaa.setEnabled(false);
            mSpinnerFxaaQuality.setEnabled(false);
            mCheckFxaa.setText("FXAA: check access");
            AppLog.e("Launcher", "Could not read FXAA setting", e);
        }
    }

    private void updateFxaaLabel() {
        mCheckFxaa.setText("FXAA: " + (mCheckFxaa.isChecked() ? "on" : "off"));
        if (mSpinnerFxaaQuality != null) {
            boolean supported = true;
            mSpinnerFxaaQuality.setEnabled(supported && mCheckFxaa.isEnabled() && mCheckFxaa.isChecked());
            mSpinnerFxaaQuality.setContentDescription(supported
                    ? "FXAA quality. Fine edges smooths more dark detail and may lower FPS. Saved on launch."
                    : "Fine edges is available for Shadow of Chernobyl.");
        }
    }

    private static java.util.List<String> replaceSingleSetting(java.util.List<String> lines,
            String key, String value) {
        java.util.List<String> result = new java.util.ArrayList<>();
        for (String line : lines) {
            if (!line.trim().equals(key) && settingValue(line, key) == null) result.add(line);
        }
        // After _preset: loading a preset must not undo the explicit AA choice.
        result.add(key + " " + value);
        return result;
    }

    private static void writeUserLtxLines(File target, java.util.List<String> lines) throws java.io.IOException {
        File parent = target.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new java.io.IOException("Could not create " + parent);
        }
        android.util.AtomicFile atomic = new android.util.AtomicFile(target);
        java.io.FileOutputStream stream = null;
        try {
            stream = atomic.startWrite();
            BufferedWriter writer = new BufferedWriter(new java.io.OutputStreamWriter(stream,
                    java.nio.charset.StandardCharsets.UTF_8));
            for (String line : lines) {
                writer.write(line);
                writer.newLine();
            }
            writer.flush();
            atomic.finishWrite(stream);
        } catch (java.io.IOException e) {
            if (stream != null) atomic.failWrite(stream);
            throw e;
        }
    }

    private boolean writeFxaaSetting(File target, boolean enabled) {
        return writeAntialiasingSettings(target, enabled, 0, true, false);
    }

    private boolean writeAntialiasingSettings(File target, boolean enabled, int detail,
            boolean writeFxaa, boolean writeDetail) {
        try {
            if (writeDetail && detail != 0 && detail != 1) throw new java.io.IOException("Invalid FXAA detail mode");
            java.util.List<String> lines = readUserLtxLines(target);
            if (writeFxaa) lines = replaceSingleSetting(lines, "r2_aa", enabled ? "on" : "off");
            if (writeDetail) lines = replaceSingleSetting(lines, "r2_aa_detail_mode", Integer.toString(detail));
            writeUserLtxLines(target, lines);
            AppLog.i("Launcher", "Saved anti-aliasing settings to " + target);
            return true;
        } catch (java.io.IOException e) {
            AppLog.e("Launcher", "Could not save FXAA setting", e);
            return false;
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        LifecycleTrace.record(this, "onStart");
    }

    @Override
    protected void onResume() {
        super.onResume();
        AppLog.resumeCapture();
        checkGamePathStatus();
        LifecycleTrace.record(this, "onResume");
        refreshFxaaFromConfig();
        updateRenderBackendStatus();
    }

    @Override
    protected void onPause() {
        LifecycleTrace.record(this, "onPause");
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
        super.onDestroy();
    }

    private static final String[] OLR_ARCHIVE_SUFFIXES = {
        "dbd1", "dbd2", "dbd3", "dbd93", "dbd94", "dbd96", "dbe", "dbk", "dbn",
        "dbp", "dbq", "dbr", "dbs", "dbt", "dbu", "dbv", "dbx", "dby"
    };

    private boolean checkGamePathStatus() {
        refreshFxaaFromConfig();
        File dir = new File(mEditGamePath.getText().toString().trim());
        int count = 0;
        for (String suffix : OLR_ARCHIVE_SUFFIXES) {
            File archive = new File(dir, "gamedata." + suffix);
            if (archive.isFile() && archive.length() > 0) {
                count++;
            }
        }
        int required = OLR_ARCHIVE_SUFFIXES.length;
        boolean ready = count == required;
        mTextPathStatus.setText(count + "/" + required + " OLR archives in " + dir.getAbsolutePath());
        mTextPathStatus.setTextColor(ContextCompat.getColor(this, ready ? R.color.status_green : R.color.status_red));
        mBtnStartGame.setEnabled(ready);
        return ready;
    }

    private void launchGame(boolean isNewGame) {
        if (!checkGamePathStatus()) return;
        savePreferences();

        String renderBackend = "native";
        if (mSpinnerRenderBackend != null && mSpinnerRenderBackend.getSelectedItem() instanceof RenderBackendItem) {
            renderBackend = ((RenderBackendItem) mSpinnerRenderBackend.getSelectedItem()).id;
        }
        if (RenderBackendConfig.PROCESS.requiresRestart(renderBackend)) {
            new AlertDialog.Builder(this)
                .setTitle("Restart required")
                .setMessage("Graphics libraries are already initialized in this process. To change backend, "
                    + "force-stop OpenXRay in Android app settings and reopen it. Your selected backend is saved.")
                .setPositiveButton("OK", null)
                .show();
            return;
        }

        String basePath = mEditGamePath.getText().toString().trim();

        String gameMode = "soc";

        // Each game lives in its own subfolder: <base>/soc, <base>/cs, <base>/cop.
        // Saves, configs, logs and gamedata.db of different games never mix.
        String pathStr = basePath.isEmpty() ? basePath : basePath;
        File dir = new File(pathStr);
        if (!pathStr.isEmpty() && !dir.exists()) {
            try {
                dir.mkdirs();
            } catch (Exception ignored) {}
        }

        // Build args string
        StringBuilder argsBuilder = new StringBuilder();

        argsBuilder.append("-soc -olr -nosplash ");

        // OpenGL (-rgl) is always enabled
        argsBuilder.append("-rgl ");
        if (mCheckNoIntro.isChecked()) {
            argsBuilder.append("-nointro ");
        }
        if (mCheckNoSound.isChecked()) {
            argsBuilder.append("-nosound ");
        }
        if (mCheckUnlockFps.isChecked()) {
            argsBuilder.append("-unlock_fps ");
        }
        if (mCheckNoShadows.isChecked()) {
            // -nodlights drops every dynamic light source outright (Light_DB.cpp),
            // which is cheaper on a mobile GPU than -noshadows and still reads as
            // "shadows gone" because without lights there is nothing casting them.
            argsBuilder.append("-nodlights ");
        }
        if (!mCheckDLights.isChecked()) {
            // Static lighting: skip the dynamic sun shadow pass only. The sun keeps
            // lighting the scene, so this stays readable -- unlike -nodlights.
            argsBuilder.append("-sunstatic ");
        }

        argsBuilder.append(buildProfileArguments(mEditCustomArgs.getText().toString(),
                isDiagnosticProfile())).append(" ");

        if (!pathStr.isEmpty()) {
            argsBuilder.append("-game_path \"").append(pathStr).append("\" ");
        }

        if (isNewGame) {
            // No launcher difficulty selector: the game menu owns difficulty selection.
            argsBuilder.append("-difficulty gd_novice ");
            argsBuilder.append("-start server(olr/single/alife/new) client(localhost) ");
        }

        ResolutionItem selectedRes = (mSpinnerResolution != null && mSpinnerResolution.getSelectedItem() instanceof ResolutionItem)
            ? (ResolutionItem) mSpinnerResolution.getSelectedItem()
            : null;
        int resW = (selectedRes != null) ? selectedRes.width : 0;
        int resH = (selectedRes != null) ? selectedRes.height : 0;

        boolean noSun = mCheckNoSun != null && mCheckNoSun.isChecked();
        String graphicsPreset = "balanced";
        if (mSpinnerGraphicsPreset != null && mSpinnerGraphicsPreset.getSelectedItem() instanceof GraphicsPresetItem) {
            graphicsPreset = ((GraphicsPresetItem) mSpinnerGraphicsPreset.getSelectedItem()).id;
        }

        if (!updateUserLtxSettingsIfChanged(pathStr, gameMode, null, mCheckNoShadows.isChecked(),
                mCheckDLights.isChecked(), noSun, mCheckFastDxt != null && mCheckFastDxt.isChecked(),
                graphicsPreset, resW, resH)) {
            Toast.makeText(this, "Could not save game settings. Check folder access and try again.", Toast.LENGTH_LONG).show();
            return;
        }
        if ((mFxaaDirty || mFxaaDetailDirty) && !writeAntialiasingSettings(findUserLtx(pathStr),
                mCheckFxaa.isChecked(), mSpinnerFxaaQuality.getSelectedItemPosition(),
                mFxaaDirty, mFxaaDetailDirty && "soc".equals(gameMode))) {
            Toast.makeText(this, "Could not save anti-aliasing. Check folder access and try again.", Toast.LENGTH_LONG).show();
            return;
        }
        mFxaaDirty = false;
        mFxaaDetailDirty = false;
        refreshFxaaFromConfig();

        float sens = Math.max(5, mSeekLookSens.getProgress()) / 10.0f;
        float opacity = Math.max(20, mSeekTouchOpacity.getProgress()) / 100.0f;
        float scale = Math.max(70, mSeekTouchScale.getProgress()) / 100.0f;

        String finalArgs = argsBuilder.toString().trim();
        AppLog.i("Launcher", "Starting OpenXRayActivity: isNewGame=" + isNewGame + ", renderBackend=" + renderBackend + ", args: " + finalArgs);

        Intent gameIntent = new Intent(this, OpenXRayActivity.class);
        gameIntent.putExtra("extra_args", finalArgs);
        gameIntent.putExtra("extra_game_path", pathStr);
        gameIntent.putExtra("extra_game_mode", gameMode);
        gameIntent.putExtra("extra_render_backend", renderBackend);
        gameIntent.putExtra("extra_look_sensitivity", sens);
        gameIntent.putExtra("extra_touch_opacity", opacity);
        gameIntent.putExtra("extra_touch_scale", scale);

        startActivity(gameIntent);
    }

    private void showLogsDialog() {
        String basePath = mEditGamePath.getText().toString().trim();
        String pathStr = (basePath != null && !basePath.isEmpty())
            ? basePath
            : basePath;
        File logDir = (pathStr != null && !pathStr.isEmpty())
            ? new File(pathStr)
            : new File(Environment.getExternalStorageDirectory(), "OpenXRay");

        File appLogFile = new File(logDir, "openxray_app.log");
        File logcatFile = new File(logDir, "openxray_logcat.log");
        File appLogBkpFile = new File(logDir, "openxray_app.log.bkp");
        File logcatBkpFile = new File(logDir, "openxray_logcat.log.bkp");
        File engineLog = AppLog.findEngineLog(logDir);

        StringBuilder content = new StringBuilder();
        content.append("Log directory: ").append(logDir.getAbsolutePath()).append("\n\n");

        if (appLogFile.exists()) {
            content.append("=== [openxray_app.log (Current Session)] ===\n");
            content.append(readLastLines(appLogFile, 150)).append("\n\n");
        } else {
            content.append("openxray_app.log: not found\n\n");
        }

        if (appLogBkpFile.exists() && appLogBkpFile.length() > 0) {
            content.append("=== [openxray_app.log.bkp (Previous Session / Crash)] ===\n");
            content.append(readLastLines(appLogBkpFile, 150)).append("\n\n");
        }

        // The engine writes its own log; fatal engine errors (CHECK_OR_EXIT,
        // R_ASSERT) only ever appear there.
        if (engineLog != null) {
            content.append("=== [").append(engineLog.getName()).append(" (Engine Log)] (last 200 lines) ===\n");
            content.append(readLastLines(engineLog, 200)).append("\n\n");
            File engineLogBkp = new File(engineLog.getParentFile(),
                    engineLog.getName().replaceAll("(?i)\\.log$", ".bkp"));
            if (engineLogBkp.exists() && engineLogBkp.length() > 0) {
                content.append("=== [").append(engineLogBkp.getName()).append(" (Engine Log, Previous Session)] (last 200 lines) ===\n");
                content.append(readLastLines(engineLogBkp, 200)).append("\n\n");
            }
        } else {
            content.append("=== [Engine Log] ===\n");
            content.append("No native engine log found in _appdata_/logs or the game mode directory.\n\n");
        }

        // fsgame.ltx decides which .db archives get mounted - dump it verbatim.
        File fsgame = new File(logDir, "fsgame.ltx");
        if (fsgame.exists()) {
            content.append("=== [fsgame.ltx] ===\n");
            String fsgameText = AppLog.readTextFile(fsgame);
            content.append(fsgameText != null ? fsgameText : "(unreadable)").append("\n\n");
        }

        if (logcatFile.exists()) {
            content.append("=== [openxray_logcat.log] (last 100 lines) ===\n");
            content.append(readLastLines(logcatFile, 100)).append("\n");
        }

        if (logcatBkpFile.exists() && logcatBkpFile.length() > 0) {
            content.append("\n=== [openxray_logcat.log.bkp (Previous Session Logcat)] (last 100 lines) ===\n");
            content.append(readLastLines(logcatBkpFile, 100)).append("\n");
        }

        // Native crashes are logged by the system ("Fatal signal 11"), not by the
        // app, so a plain logcat capture of our own pid never sees them.
        String crashBuffer = readLogcatBuffer("crash");
        if (crashBuffer != null && !crashBuffer.isEmpty()) {
            content.append("\n=== [logcat -b crash (Native Crash / Tombstone)] (last 80 lines) ===\n");
            content.append(crashBuffer).append("\n");
        }

        android.widget.ScrollView sv = new android.widget.ScrollView(this);
        TextView tv = new TextView(this);
        tv.setText(content.toString());
        tv.setTextSize(11);
        tv.setPadding(30, 20, 30, 20);
        tv.setTextIsSelectable(true);
        tv.setTypeface(android.graphics.Typeface.MONOSPACE);
        sv.addView(tv);

        androidx.appcompat.app.AlertDialog.Builder builder = new AlertDialog.Builder(this)
            .setTitle("OpenXRay Logs")
            .setView(sv)
            .setPositiveButton("Share Zip", (dialog, which) -> {
                File report = new File(logDir, "openxray_report.txt");
                try (BufferedWriter writer = new BufferedWriter(new FileWriter(report, false))) {
                    writer.write(content.toString());
                } catch (Exception e) {
                    Toast.makeText(this, "Could not save report: " + e.getMessage(), Toast.LENGTH_LONG).show();
                    return;
                }
                shareLogsZip(logDir, report);
            });

        // Diagnostics upload for "broken but not crashing" reports. Consumes no
        // crash marker: a pending crash stays reportable afterwards.
        if (GitHubIssueReporter.isConfigured()) {
            builder.setNeutralButton("Send to GitHub", (dialog, which) ->
                    sendCrashReportToGitHub(CrashReporter.forCurrentSession(this), false));
        }
        builder.setNegativeButton("Close", null);
        builder.show();
    }

    private void shareLogsZip(File logDir, File report) {
        File zipFile = new File(logDir, "openxray_logs.zip");
        try (java.util.zip.ZipOutputStream zos = new java.util.zip.ZipOutputStream(
                new java.io.FileOutputStream(zipFile))) {
            addToZip(zos, report, report.getName());
            for (String name : new String[]{
                    "openxray_app.log", "openxray_app.log.bkp",
                    "openxray_logcat.log", "openxray_logcat.log.bkp",
                    "frame_trace.log", "fsgame.ltx"}) {
                File f = new File(logDir, name);
                if (f.exists() && f.length() > 0) addToZip(zos, f, name);
            }
            File engineLog = AppLog.findEngineLog(logDir);
            if (engineLog != null) {
                addToZip(zos, engineLog, engineLog.getName());
                File bkp = new File(engineLog.getParentFile(),
                        engineLog.getName().replaceAll("(?i)\\.log$", ".bkp"));
                if (bkp.exists() && bkp.length() > 0) addToZip(zos, bkp, bkp.getName());
            }
            for (File profile : AppLog.findRecentGpuProfiles(logDir)) {
                addToZip(zos, profile, "_appdata_/logs/" + profile.getName());
            }
            File userLtx = new File(logDir, "_appdata_/user.ltx");
            if (userLtx.exists()) addToZip(zos, userLtx, "user.ltx");
        } catch (Exception e) {
            Toast.makeText(this, "Could not create zip: " + e.getMessage(), Toast.LENGTH_LONG).show();
            return;
        }

        android.net.Uri uri = androidx.core.content.FileProvider.getUriForFile(
                this, getPackageName() + ".fileprovider", zipFile);
        android.content.Intent share = new android.content.Intent(android.content.Intent.ACTION_SEND);
        share.setType("application/zip");
        share.putExtra(android.content.Intent.EXTRA_STREAM, uri);
        share.addFlags(android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION);
        share.setClipData(android.content.ClipData.newUri(getContentResolver(), "logs", uri));
        startActivity(android.content.Intent.createChooser(share, "Share logs"));
    }

    private static void addToZip(java.util.zip.ZipOutputStream zos, File f, String entryName)
            throws java.io.IOException {
        zos.putNextEntry(new java.util.zip.ZipEntry(entryName));
        try (java.io.FileInputStream fis = new java.io.FileInputStream(f)) {
            byte[] buf = new byte[64 * 1024];
            int n;
            while ((n = fis.read(buf)) > 0) zos.write(buf, 0, n);
        }
        zos.closeEntry();
    }

    // Reads a logcat buffer without -f. Returns null when the app is not
    // allowed to read it (crash buffer needs READ_LOGS on modern Android).
    private String readLogcatBuffer(String buffer) {
        try {
            ProcessBuilder pb = new ProcessBuilder("logcat", "-b", buffer, "-d", "-v", "threadtime", "-t", "80");
            Process p = pb.start();
            StringBuilder sb = new StringBuilder();
            try (BufferedReader reader = new BufferedReader(
                    new java.io.InputStreamReader(p.getInputStream(), java.nio.charset.StandardCharsets.UTF_8))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    sb.append(line).append('\n');
                }
            }
            p.waitFor();
            return sb.toString().trim();
        } catch (Exception e) {
            return null;
        }
    }

    private String readLastLines(File file, int maxLines) {
        if (!file.exists()) return "";
        java.util.LinkedList<String> lines = new java.util.LinkedList<>();
        try (BufferedReader reader = new BufferedReader(new FileReader(file))) {
            String line;
            while ((line = reader.readLine()) != null) {
                lines.add(line);
                if (lines.size() > maxLines) {
                    lines.removeFirst();
                }
            }
        } catch (Exception e) {
            return "Error reading log: " + e.getMessage();
        }
        StringBuilder sb = new StringBuilder();
        for (String l : lines) {
            sb.append(l).append("\n");
        }
        return sb.toString();
    }

    // Engine _preset tokens (see qpreset_token in xrRender_console.cpp).
    // "none" means the launcher does not manage graphics at all (set in game).
    private static String presetTokenForId(String presetId) {
        if ("none".equals(presetId)) return null;
        if ("minimum".equals(presetId)) return "Minimum";
        if ("low".equals(presetId)) return "Low";
        if ("high".equals(presetId)) return "High";
        if ("extreme".equals(presetId)) return "Extreme";
        if ("potato".equals(presetId)) return "Minimum";
        return "Default";
    }

    // Extra cheap settings applied on top of Minimum for the Potato preset.
    // All names verified against xrRender_console.cpp console commands.
    private static final String[][] POTATO_EXTRA_SETTINGS = {
        {"r2_ssao_mode", "disabled"},
        {"r2_ssao", "off"},
        {"r2_sun_shafts", "st_opt_off"},
        {"r2_sun_quality", "st_opt_low"},
        {"r2_smap_size", "1024"},
        {"r2_dof_enable", "off"},
        {"r3_msaa", "st_opt_off"},
        {"r2_volumetric_lights", "off"},
        {"r3_volumetric_smoke", "off"},
        {"r3_dynamic_wet_surfaces", "off"},
        {"r3_water_refl", "st_opt_off"},
        {"r2_steep_parallax", "off"},
        {"r2_detail_bump", "off"},
        {"r2_soft_water", "off"},
        {"r2_soft_particles", "off"},
        {"r__tf_aniso", "1"},
        {"r__detail_density", "0.3"},
        {"r2_aa", "off"},
    };

    // Graphics keys fully managed by the launcher: old values are dropped from
    // user.ltx and re-appended after the _preset line, so switching presets
    // never leaves stale overrides behind.
    private static final String[] MANAGED_GRAPHICS_KEYS = {
        "_preset",
        "r2_sun", "r2_sun_details", "r__actor_shadow",
        "r2_volumetric_lights", "r2_slight_fade",
        "r2_ssao_mode", "r2_ssao", "r2_sun_shafts", "r2_sun_quality",
        "r2_smap_size", "r2_dof_enable", "r3_msaa",
        "r3_volumetric_smoke", "r3_dynamic_wet_surfaces", "r3_water_refl",
        "r2_steep_parallax", "r2_detail_bump", "r2_soft_water",
        "r2_soft_particles", "r__tf_aniso", "r__detail_density", "r2_aa",
        // Extra aspects covered per preset below.
        "r2_tf_mipbias", "r__geometry_lod", "r2_ssa_lod_a", "r2_ssa_lod_b",
        "r2_sun_far", "rs_skeleton_update", "r__fast_dxt",
    };

    // Full-aspect values per graphics preset. Key names verified against
    // xrRender_console.cpp console commands and their min/max ranges.
    // Texture quality: r__tf_aniso (filtering) + r2_tf_mipbias (detail).
    // Draw distance: r2_ssa_lod_a/b (object cull) + r2_sun_far.
    // Model quality: r__geometry_lod + rs_skeleton_update (lower = smoother).
    private static java.util.Map<String, String[][]> presetAspectSettings() {
        java.util.Map<String, String[][]> m = new java.util.LinkedHashMap<>();
        String[][] potato = {
            {"r__tf_aniso", "1"}, {"r2_tf_mipbias", "1.0"},
            {"r__geometry_lod", "0.5"}, {"rs_skeleton_update", "32"},
            {"r2_ssa_lod_a", "32"}, {"r2_ssa_lod_b", "32"},
            {"r__detail_density", "0.3"},
            {"r2_smap_size", "1024"}, {"r2_sun_far", "60"},
        };
        String[][] minimum = {
            {"r__tf_aniso", "2"}, {"r2_tf_mipbias", "0.5"},
            {"r__geometry_lod", "0.6"}, {"rs_skeleton_update", "24"},
            {"r2_ssa_lod_a", "32"}, {"r2_ssa_lod_b", "32"},
            {"r__detail_density", "0.3"},
            {"r2_smap_size", "1024"}, {"r2_sun_far", "70"},
        };
        String[][] low = {
            {"r__tf_aniso", "4"}, {"r2_tf_mipbias", "0"},
            {"r__geometry_lod", "0.75"}, {"rs_skeleton_update", "16"},
            {"r2_ssa_lod_a", "48"}, {"r2_ssa_lod_b", "40"},
            {"r__detail_density", "0.45"},
            {"r2_smap_size", "1536"}, {"r2_sun_far", "80"},
        };
        String[][] balanced = {
            {"r__tf_aniso", "4"}, {"r2_tf_mipbias", "0"},
            {"r__geometry_lod", "0.9"}, {"rs_skeleton_update", "10"},
            {"r2_ssa_lod_a", "64"}, {"r2_ssa_lod_b", "48"},
            {"r__detail_density", "0.6"},
            {"r2_smap_size", "2048"}, {"r2_sun_far", "100"},
        };
        String[][] high = {
            {"r__tf_aniso", "8"}, {"r2_tf_mipbias", "0"},
            {"r__geometry_lod", "1.0"}, {"rs_skeleton_update", "8"},
            {"r2_ssa_lod_a", "64"}, {"r2_ssa_lod_b", "56"},
            {"r__detail_density", "0.75"},
            {"r2_smap_size", "2048"}, {"r2_sun_far", "120"},
        };
        String[][] extreme = {
            {"r__tf_aniso", "16"}, {"r2_tf_mipbias", "0"},
            {"r__geometry_lod", "1.25"}, {"rs_skeleton_update", "4"},
            {"r2_ssa_lod_a", "96"}, {"r2_ssa_lod_b", "64"},
            {"r__detail_density", "0.9"},
            {"r2_smap_size", "3072"}, {"r2_sun_far", "150"},
        };
        m.put("potato", potato);
        m.put("minimum", minimum);
        m.put("low", low);
        m.put("balanced", balanced);
        m.put("high", high);
        m.put("extreme", extreme);
        return m;
    }

    private static boolean isManagedGraphicsKey(String trimmedLine) {
        for (String key : MANAGED_GRAPHICS_KEYS) {
            if (trimmedLine.equals(key)
                    || trimmedLine.startsWith(key + " ")
                    || trimmedLine.startsWith(key + "\t")) {
                return true;
            }
        }
        return false;
    }

    // The launcher rewrites graphics keys in user.ltx ONLY when the
    // launcher-side selection changed since the last launch (or on first
    // run). Otherwise settings tweaked inside the game are left alone —
    // previously every launch wiped them back to the spinner values.
    private static boolean selectionValueChanged(String previousSignature, int field, String value) {
        if (previousSignature == null) return true;
        String[] previous = previousSignature.split("\\|", -1);
        return previous.length != 7 || !value.equals(previous[field]);
    }

    private boolean updateUserLtxSettingsIfChanged(String modePathStr, String gameMode, String diff,
            boolean noShadows, boolean dLights, boolean noSun, boolean fastDxt,
            String graphicsPreset, int resW, int resH) {
        String sig = graphicsPreset + "|" + resW + "x" + resH + "|"
                + noShadows + "|" + dLights + "|" + noSun + "|" + fastDxt + "|" + diff;
        String key = "applied_gfx_signature_" + gameMode;
        String prev = mPrefs.getString(key, null);
        File probe = findUserLtx(modePathStr);
        boolean migrateBloom = !mPrefs.getBoolean(OLR_BLOOM_MIGRATED, false);
        boolean migrateTonemap = !mPrefs.getBoolean(OLR_TONEMAP_MIGRATED, false);
        if (sig.equals(prev) && userLtxExists(probe) && !migrateBloom && !migrateTonemap) {
            AppLog.i("Launcher", "Graphics selection unchanged, keeping in-game user.ltx");
            return true;
        }
        if ("none".equals(graphicsPreset)) {
            // Preset 'none' means the game owns its graphics settings, so no
            // graphics key is written. The resolution is not a preset key though:
            // it has its own spinner and its own saved preference, and returning
            // early here left that spinner dead -- the choice was persisted in
            // prefs but never reached user.ltx, so the game kept rendering at
            // whatever vid_mode was there last.
            AppLog.i("Launcher", "Graphics preset is 'none', preserving graphics and writing resolution: "
                    + resW + "x" + resH);
        }
        // Resolution/difficulty changes are independent of graphics ownership.
        // They must not restore a preset over subsequent in-game quality edits.
        boolean manageGraphics = !"none".equals(graphicsPreset);
        boolean applyPreset = manageGraphics
                && (!userLtxExists(probe) || selectionValueChanged(prev, 0, graphicsPreset));
        boolean applySun = manageGraphics
                && (applyPreset || selectionValueChanged(prev, 4, Boolean.toString(noSun)));
        boolean applyLighting = manageGraphics
                && (applyPreset || selectionValueChanged(prev, 3, Boolean.toString(dLights)));
        boolean applyFastDxt = manageGraphics
                && (applyPreset || selectionValueChanged(prev, 5, Boolean.toString(fastDxt)));
        boolean saved = updateUserLtxSettings(modePathStr, diff, noShadows, dLights, noSun,
                fastDxt, graphicsPreset, resW, resH, applyPreset, applySun, applyLighting,
                applyFastDxt, mCheckFxaa.isChecked(), migrateBloom, migrateTonemap);
        if (saved) {
            mPrefs.edit().putString(key, sig).putBoolean(OLR_BLOOM_MIGRATED, true)
                    .putBoolean(OLR_TONEMAP_MIGRATED, true).apply();
        }
        return saved;
    }

    private boolean updateUserLtxSettings(String pathStr, String diff, boolean noShadows, boolean dLights, boolean noSun, boolean fastDxt, String graphicsPreset, int resW, int resH,
            boolean applyPreset, boolean applySun, boolean applyLighting, boolean applyFastDxt,
            boolean fxaaEnabled, boolean migrateBloom, boolean migrateTonemap) {
        if (pathStr == null || pathStr.isEmpty()) return true;
        try {
            File appDataDir = new File(pathStr, "_appdata_");
            if (!appDataDir.exists()) {
                appDataDir.mkdirs();
            }

            File targetFile = findUserLtx(pathStr);
            boolean hasUserLtx = userLtxExists(targetFile);
            java.util.List<String> currentLines = hasUserLtx ? readUserLtxLines(targetFile)
                    : java.util.Collections.emptyList();

            java.util.Map<String, String> settings = new java.util.LinkedHashMap<>();
            // _preset must come first: it loads rspec_*.ltx, lines below override it.
            // presetTokenForId returns null for "none" -- writing that through would
            // put a literal "_preset null" into user.ltx.
            String presetToken = presetTokenForId(graphicsPreset);
            if (applyPreset && presetToken != null) {
                settings.put("_preset", presetToken);
            }
            // Full-aspect values for this preset (texture quality, draw
            // distance, model quality...). Explicit sun/light checkbox
            // choices below still win over these.
            String[][] aspects = presetAspectSettings().get(graphicsPreset);
            if (applyPreset && aspects != null) {
                for (String[] kv : aspects) {
                    settings.put(kv[0], kv[1]);
                }
            }
            if (diff != null && !diff.isEmpty()) {
                settings.put("g_game_difficulty", diff);
            }
            // Fast CPU DXT decode (bcdec); managed so the checkbox state
            // always matches the engine, like the other graphics keys.
            if (applyFastDxt) settings.put("r__fast_dxt", fastDxt ? "1" : "0");
            if (resW > 0 && resH > 0) {
                settings.put("vid_mode", resW + "x" + resH);
            } else {
                DisplayMetrics metrics = new DisplayMetrics();
                getWindowManager().getDefaultDisplay().getRealMetrics(metrics);
                int screenW = Math.max(metrics.widthPixels, metrics.heightPixels);
                int screenH = Math.min(metrics.widthPixels, metrics.heightPixels);
                if (screenW > 0 && screenH > 0) {
                    settings.put("vid_mode", screenW + "x" + screenH);
                }
            }
            // Only the "No sun" checkbox owns r2_sun. Static lighting deliberately keeps
            // the sun: -sunstatic skips its shadow pass, it does not remove the
            // light, so folding dLights in here used to turn static mode into
            // "no sun" as well. "No shadows" goes through -nodlights now.
            if (applySun) {
                settings.put("r2_sun", noSun ? "off" : "on");
            }
            if (applyLighting) {
                boolean staticLighting = !dLights;
                if (staticLighting) {
                    settings.put("r2_sun_details", "off");
                    settings.put("r__actor_shadow", "off");
                    settings.put("r2_volumetric_lights", "off");
                    // r2_slight_fade accepts 0.2..1, so leave it alone in static mode.
                } else {
                    settings.put("r2_sun_details", "on");
                    settings.put("r__actor_shadow", "on");
                    settings.put("r2_volumetric_lights", "on");
                    settings.put("r2_slight_fade", "0.5");
                }
            }
            if (applyPreset && "potato".equals(graphicsPreset)) {
                for (String[] kv : POTATO_EXTRA_SETTINGS) {
                    // Don't stomp explicit sun/light choices made above.
                    if (!settings.containsKey(kv[0])) {
                        settings.put(kv[0], kv[1]);
                    }
                }
            }
            // The visible FXAA choice survives every preset, including Potato.
            // This is appended after _preset, independently of other quality.
            if (applyPreset) {
                settings.put("r2_aa", fxaaEnabled ? "on" : "off");
                settings.put("r2_aa_detail_mode", Integer.toString(readFxaaDetailSetting(targetFile)));
            }

            // This launcher selects an absolute render resolution. A scale
            // left by another launcher would multiply it a second time and
            // can crop the SoC menu, even after selecting Native resolution.
            if (settings.containsKey("vid_mode")) {
                settings.put("render_scale", "1.");
            }

            if (migrateBloom || !hasUserLtx) {
                String bloomThreshold = lastSettingValue(currentLines, "r2_ls_bloom_threshold");
                // Replace the inherited engine default once. Keep explicit OLR settings.
                if (bloomThreshold == null || "0.00001".equals(bloomThreshold)) {
                    settings.put("r2_ls_bloom_threshold", "0.9");
                }
            }

            if (migrateTonemap || !hasUserLtx) {
                String enabled = lastSettingValue(currentLines, "r2_tonemap");
                boolean inherited = enabled == null || "on".equalsIgnoreCase(enabled) || "1".equals(enabled);
                for (String[] setting : OLR_TONEMAP_DEFAULTS) {
                    inherited &= matchesTonemapDefault(lastSettingValue(currentLines, setting[0]), setting[2]);
                }
                if (inherited) {
                    for (String[] setting : OLR_TONEMAP_DEFAULTS) {
                        settings.put(setting[0], setting[1]);
                    }
                }
            }

            java.util.List<String> lines = new java.util.ArrayList<>();

            if (hasUserLtx) {
                for (String line : currentLines) {
                    String trimmed = line.trim();
                    if (applyPreset && isManagedGraphicsKey(trimmed)) {
                        continue;
                    }
                    boolean replaced = false;
                    for (java.util.Map.Entry<String, String> entry : settings.entrySet()) {
                        if (settingValue(line, entry.getKey()) != null) {
                            // Append all explicit overrides after any existing
                            // _preset line rather than replacing them above it.
                            replaced = true;
                            break;
                        }
                    }
                    if (!replaced) {
                        lines.add(line);
                    }
                }
            } else {
                lines.add("renderer renderer_rgl");
            }

            File[] saveDirs = new File[] {
                new File(appDataDir, "savedgames"),
                new File(pathStr, "savedgames"),
                new File(pathStr, "appdata/savedgames")
            };
            String latestSaveName = null;
            long latestSaveTime = 0;
            for (File sDir : saveDirs) {
                if (sDir.exists() && sDir.isDirectory()) {
                    File[] files = sDir.listFiles();
                    if (files != null) {
                        for (File sf : files) {
                            String fn = sf.getName();
                            if ((fn.endsWith(".sav") || fn.endsWith(".scop")) && sf.lastModified() > latestSaveTime) {
                                latestSaveTime = sf.lastModified();
                                int dot = fn.lastIndexOf('.');
                                latestSaveName = (dot > 0) ? fn.substring(0, dot) : fn;
                            }
                        }
                    }
                }
            }
            if (latestSaveName != null && !latestSaveName.isEmpty()) {
                settings.put("load_last_save", latestSaveName);
            }

            for (java.util.Map.Entry<String, String> entry : settings.entrySet()) {
                lines.add(entry.getKey() + " " + entry.getValue());
            }

            boolean hasQuickSave = false;
            boolean hasQuickLoad = false;
            boolean hasQuit = false;
            boolean hasMedkit = false;
            boolean hasBandage = false;
            boolean hasLookoutL = false;
            boolean hasLookoutR = false;
            boolean hasWpn1 = false, hasWpn2 = false, hasWpn3 = false, hasWpn4 = false, hasWpn5 = false, hasWpn6 = false;

            for (String l : lines) {
                String tl = l.trim();
                if (tl.startsWith("bind quick_save ") || tl.startsWith("bind_sec quick_save ")) hasQuickSave = true;
                if (tl.startsWith("bind quick_load ") || tl.startsWith("bind_sec quick_load ")) hasQuickLoad = true;
                if (tl.startsWith("bind quit ") || tl.startsWith("bind_sec quit ")) hasQuit = true;
                if (tl.startsWith("bind use_medkit ") || tl.startsWith("bind_sec use_medkit ")) hasMedkit = true;
                if (tl.startsWith("bind use_bandage ") || tl.startsWith("bind_sec use_bandage ")) hasBandage = true;
                if (tl.startsWith("bind llookout ") || tl.startsWith("bind_sec llookout ")) hasLookoutL = true;
                if (tl.startsWith("bind rlookout ") || tl.startsWith("bind_sec rlookout ")) hasLookoutR = true;
                if (tl.startsWith("bind wpn_1 ") || tl.startsWith("bind_sec wpn_1 ")) hasWpn1 = true;
                if (tl.startsWith("bind wpn_2 ") || tl.startsWith("bind_sec wpn_2 ")) hasWpn2 = true;
                if (tl.startsWith("bind wpn_3 ") || tl.startsWith("bind_sec wpn_3 ")) hasWpn3 = true;
                if (tl.startsWith("bind wpn_4 ") || tl.startsWith("bind_sec wpn_4 ")) hasWpn4 = true;
                if (tl.startsWith("bind wpn_5 ") || tl.startsWith("bind_sec wpn_5 ")) hasWpn5 = true;
                if (tl.startsWith("bind wpn_6 ") || tl.startsWith("bind_sec wpn_6 ")) hasWpn6 = true;
            }
            if (!hasQuickSave) {
                lines.add("bind quick_save kF6");
                lines.add("bind_sec quick_save kF5");
            }
            if (!hasQuickLoad) {
                lines.add("bind quick_load kF7");
                lines.add("bind_sec quick_load kF9");
            }
            if (!hasQuit) {
                lines.add("bind quit kESCAPE");
            }
            if (!hasMedkit) {
                lines.add("bind use_medkit kLBRACKET");
            }
            if (!hasBandage) {
                lines.add("bind use_bandage kRBRACKET");
            }
            if (!hasLookoutL) {
                lines.add("bind llookout kQ");
            }
            if (!hasLookoutR) {
                lines.add("bind rlookout kE");
            }
            if (!hasWpn1) lines.add("bind wpn_1 k1");
            if (!hasWpn2) lines.add("bind wpn_2 k2");
            if (!hasWpn3) lines.add("bind wpn_3 k3");
            if (!hasWpn4) lines.add("bind wpn_4 k4");
            if (!hasWpn5) lines.add("bind wpn_5 k5");
            if (!hasWpn6) lines.add("bind wpn_6 k6");

            writeUserLtxLines(targetFile, lines);
            return true;
        } catch (Exception e) {
            AppLog.e("Launcher", "Could not save user.ltx settings", e);
            return false;
        }
    }

    private void openFolderPicker() {
        try {
            if (Build.VERSION.SDK_INT >= 30 && !Environment.isExternalStorageManager()) {
                new AlertDialog.Builder(this)
                        .setMessage("Allow file access for OLR to read the archives directly from your chosen folder. Then return and tap Browse again.")
                        .setNegativeButton("Cancel", null)
                        .setPositiveButton("Open settings", (dialog, which) ->
                                startActivity(new Intent(android.provider.Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                                        Uri.parse("package:" + getPackageName()))))
                        .show();
                return;
            }
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            startActivityForResult(intent, BROWSE_FOLDER_REQUEST_CODE);
        } catch (Exception e) {
            Toast.makeText(this, "Failed to open folder picker", Toast.LENGTH_SHORT).show();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == BROWSE_FOLDER_REQUEST_CODE && resultCode == RESULT_OK && data != null) {
            Uri treeUri = data.getData();
            if (treeUri != null) {
                String path = getPathFromTreeUri(treeUri);
                if (path != null && !path.isEmpty()) {
                    File folder = new File(path);
                    File nested = new File(folder, DATA_FOLDER);
                    if (!new File(folder, "gamedata.dbd1").isFile()
                            && new File(nested, "gamedata.dbd1").isFile()) folder = nested;
                    if (!folder.isDirectory() || folder.getAbsolutePath().contains("\"")) {
                        Toast.makeText(this, "Select a local folder with the OLR archives", Toast.LENGTH_LONG).show();
                        return;
                    }
                    mEditGamePath.setText(folder.getAbsolutePath());
                    mPrefs.edit().putString("olr_assets_root", folder.getAbsolutePath()).apply();
                    checkGamePathStatus();
                }
            }
        }
    }

    private String getPathFromTreeUri(Uri treeUri) {
        try {
            String docId = android.provider.DocumentsContract.getTreeDocumentId(treeUri);
            if (docId != null) {
                if (!"com.android.externalstorage.documents".equals(treeUri.getAuthority())) return null;
                String[] split = docId.split(":", 2);
                if (split.length > 0) {
                    String type = split[0];
                    String relativePath = split.length > 1 ? split[1] : "";
                    if ("primary".equalsIgnoreCase(type)) {
                        return Environment.getExternalStorageDirectory() + (relativePath.isEmpty() ? "" : "/" + relativePath);
                    } else {
                        return "/storage/" + type + (relativePath.isEmpty() ? "" : "/" + relativePath);
                    }
                }
            }
        } catch (Exception ignored) {}
        return null;
    }


    public static class ResolutionItem {
        public final String title;
        public final int width;
        public final int height;

        public ResolutionItem(String title, int width, int height) {
            this.title = title;
            this.width = width;
            this.height = height;
        }

        @Override
        public String toString() {
            return title;
        }
    }

    private void setupResolutionSpinner() {
        mResolutionList.clear();

        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getRealMetrics(metrics);
        int screenW = Math.max(metrics.widthPixels, metrics.heightPixels);
        int screenH = Math.min(metrics.widthPixels, metrics.heightPixels);
        if (screenW <= 0) screenW = 1920;
        if (screenH <= 0) screenH = 1080;
        float aspect = (float) screenW / (float) screenH;

        // 1. Native resolution
        mResolutionList.add(new ResolutionItem("Native: " + screenW + "x" + screenH + " (Screen native)", 0, 0));

        // 2. Aspect-ratio matched resolutions (preserving full-width screen without stretching)
        if (Math.abs(aspect - (16.0f / 9.0f)) > 0.05f) {
            int w1080 = Math.round(1080 * aspect);
            int w720 = Math.round(720 * aspect);
            int w540 = Math.round(540 * aspect);
            mResolutionList.add(new ResolutionItem("1080p Screen (" + w1080 + "x1080)", w1080, 1080));
            mResolutionList.add(new ResolutionItem("720p Screen (" + w720 + "x720)", w720, 720));
            mResolutionList.add(new ResolutionItem("540p Screen (" + w540 + "x540)", w540, 540));
        }

        // 3. Standard fixed resolutions
        mResolutionList.add(new ResolutionItem("Full HD (1920x1080, 16:9)", 1920, 1080));
        mResolutionList.add(new ResolutionItem("720p (1280x720, 16:9)", 1280, 720));
        mResolutionList.add(new ResolutionItem("qHD (960x540, 16:9)", 960, 540));
        mResolutionList.add(new ResolutionItem("SVGA (800x600, 4:3)", 800, 600));
        mResolutionList.add(new ResolutionItem("VGA (640x480, 4:3)", 640, 480));

        mResolutionAdapter = new ArrayAdapter<>(
            this,
            R.layout.item_resolution_spinner,
            mResolutionList
        );
        mResolutionAdapter.setDropDownViewResource(R.layout.item_resolution_dropdown);
        mSpinnerResolution.setAdapter(mResolutionAdapter);
    }

    public static class GraphicsPresetItem {
        public final String title;
        public final String id;

        public GraphicsPresetItem(String title, String id) {
            this.title = title;
            this.id = id;
        }

        @Override
        public String toString() {
            return title;
        }
    }

    private void setupGraphicsPresetSpinner() {
        mGraphicsPresetList.clear();
        mGraphicsPresetList.add(new GraphicsPresetItem("None (set in game)", "none"));
        mGraphicsPresetList.add(new GraphicsPresetItem("Potato (max speed)", "potato"));
        mGraphicsPresetList.add(new GraphicsPresetItem("Minimum", "minimum"));
        mGraphicsPresetList.add(new GraphicsPresetItem("Low", "low"));
        mGraphicsPresetList.add(new GraphicsPresetItem("Balanced", "balanced"));
        mGraphicsPresetList.add(new GraphicsPresetItem("High", "high"));
        mGraphicsPresetList.add(new GraphicsPresetItem("Extreme", "extreme"));

        mGraphicsPresetAdapter = new ArrayAdapter<>(
            this,
            R.layout.item_resolution_spinner,
            mGraphicsPresetList
        );
        mGraphicsPresetAdapter.setDropDownViewResource(R.layout.item_resolution_dropdown);
        if (mSpinnerGraphicsPreset != null) {
            mSpinnerGraphicsPreset.setAdapter(mGraphicsPresetAdapter);
        }
    }

    public static class RenderBackendItem {
        public final String title;
        public final String id;

        public RenderBackendItem(String title, String id) {
            this.title = title;
            this.id = id;
        }

        @Override
        public String toString() {
            return title;
        }
    }

    private void setupRenderBackendSpinner() {
        mRenderBackendList.clear();
        mRenderBackendList.add(new RenderBackendItem("Native OpenGL ES", "native"));
        mRenderBackendList.add(new RenderBackendItem("OpenGL ES -> Vulkan (ANGLE)", "angle_vulkan"));
        mRenderBackendList.add(new RenderBackendItem("OpenGL ES -> OpenGL ES (ANGLE)", "angle_gles"));

        mRenderBackendAdapter = new ArrayAdapter<>(
            this,
            R.layout.item_resolution_spinner,
            mRenderBackendList
        );
        mRenderBackendAdapter.setDropDownViewResource(R.layout.item_resolution_dropdown);
        mSpinnerRenderBackend.setAdapter(mRenderBackendAdapter);

        mSpinnerRenderBackend.setOnItemSelectedListener(new android.widget.AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(android.widget.AdapterView<?> parent, View view, int position, long id) {
                updateRenderBackendStatus();
            }
            @Override
            public void onNothingSelected(android.widget.AdapterView<?> parent) {}
        });
    }

    private void updateRenderBackendStatus() {
        if (mTextRenderBackendStatus == null || mSpinnerRenderBackend == null) return;
        Object selected = mSpinnerRenderBackend.getSelectedItem();
        if (selected instanceof RenderBackendItem) {
            RenderBackendItem item = (RenderBackendItem) selected;
            if (RenderBackendConfig.PROCESS.requiresRestart(item.id)) {
                mTextRenderBackendStatus.setVisibility(View.VISIBLE);
                mTextRenderBackendStatus.setText("Force-stop and reopen OpenXRay to change backend. Current process: "
                    + RenderBackendConfig.PROCESS.effectiveBackend() + ".");
                mTextRenderBackendStatus.setTextColor(ContextCompat.getColor(this, R.color.amber_primary));
            } else if (RenderBackendConfig.isAngle(item.id)) {
                mTextRenderBackendStatus.setVisibility(View.VISIBLE);
                RenderBackendConfig.Availability available = RenderBackendConfig.probe(
                    Build.VERSION.SDK_INT, getApplicationInfo().nativeLibraryDir);
                if (available.isAvailable()) {
                    mTextRenderBackendStatus.setText("ANGLE libraries packaged. Graphics backend will be verified at startup.");
                    mTextRenderBackendStatus.setTextColor(ContextCompat.getColor(this, R.color.status_green));
                } else {
                    mTextRenderBackendStatus.setText(available.reason + " Native OpenGL ES will be used.");
                    mTextRenderBackendStatus.setTextColor(ContextCompat.getColor(this, R.color.amber_primary));
                }
            } else {
                mTextRenderBackendStatus.setVisibility(View.GONE);
            }
        }
    }
}
