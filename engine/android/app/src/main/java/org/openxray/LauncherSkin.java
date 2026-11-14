package org.openxray;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.ClipDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.os.Build;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.SeekBar;
import android.widget.TextView;
import androidx.appcompat.widget.AppCompatButton;
import androidx.core.content.res.ResourcesCompat;

/** Presentation only: no preferences, launch arguments or gameplay state. */
final class LauncherSkin {
    private static final int AMBER = 0xFFCCAC77;
    private static final int AQUA = 0xFF76BDB7;

    static void apply(Activity activity) {
        View root = activity.findViewById(R.id.launcher_root);
        Typeface body = ResourcesCompat.getFont(activity, R.font.launcher_stalker_menu);
        Typeface heading = Build.VERSION.SDK_INT >= 28 ?
                Typeface.create(body, 600, false) : Typeface.create(body, Typeface.BOLD);
        Resources res = activity.getResources();
        Bitmap[] frame = new Bitmap[] {
                BitmapFactory.decodeResource(res, R.drawable.launcher_ui_frame_lt),
                BitmapFactory.decodeResource(res, R.drawable.launcher_ui_frame_t),
                BitmapFactory.decodeResource(res, R.drawable.launcher_ui_frame_rt),
                BitmapFactory.decodeResource(res, R.drawable.launcher_ui_frame_l),
                BitmapFactory.decodeResource(res, R.drawable.launcher_ui_frame_back),
                BitmapFactory.decodeResource(res, R.drawable.launcher_ui_frame_r),
                BitmapFactory.decodeResource(res, R.drawable.launcher_ui_frame_lb),
                BitmapFactory.decodeResource(res, R.drawable.launcher_ui_frame_b),
                BitmapFactory.decodeResource(res, R.drawable.launcher_ui_frame_rb) };
        Bitmap compact = BitmapFactory.decodeResource(res, R.drawable.launcher_dds_btn_mm_e);
        Bitmap mainButton = BitmapFactory.decodeResource(res, R.drawable.launcher_ui_btn_triple_center_d);
        Bitmap backButton = BitmapFactory.decodeResource(res, R.drawable.launcher_ui_btn_triple_left_d);
        Bitmap actionButton = BitmapFactory.decodeResource(res, R.drawable.launcher_ui_button_02);
        Bitmap headerButton = BitmapFactory.decodeResource(res, R.drawable.launcher_ui_btn_mm_h);
        Bitmap track = BitmapFactory.decodeResource(res, R.drawable.launcher_dds_scb_back);
        Bitmap knob = BitmapFactory.decodeResource(res, R.drawable.launcher_ui_btn_numpad_okcancel_h);
        Bitmap checkbox = BitmapFactory.decodeResource(res, R.drawable.launcher_dds_frame_01_back);
        Bitmap mainPanel = BitmapFactory.decodeResource(res, R.drawable.launcher_panel_main_common3);
        Bitmap touchPanel = BitmapFactory.decodeResource(res, R.drawable.launcher_panel_touch_common3);
        Bitmap optionsPanel = BitmapFactory.decodeResource(res, R.drawable.launcher_panel_game_options);
        Bitmap pathPanel = BitmapFactory.decodeResource(res, R.drawable.launcher_panel_game_path_frame);
        decorate(root, body, heading, frame, compact, mainButton, backButton,
                actionButton, headerButton, track, knob, checkbox,
                mainPanel, touchPanel, optionsPanel, pathPanel);
        if (activity.getResources().getConfiguration().smallestScreenWidthDp < 600) {
            activity.findViewById(R.id.launcher_hero).setVisibility(View.GONE);
            setWeight(activity.findViewById(R.id.launcher_page_container), 1f);
            // Stack settings cards on narrow screens instead of shrinking them
            // into unreadable side-by-side columns.
            LinearLayout content = activity.findViewById(R.id.launcher_settings_content);
            content.setOrientation(LinearLayout.VERTICAL);
            useFullWidth(activity.findViewById(R.id.launcher_settings_column));
            useFullWidth(activity.findViewById(R.id.launcher_touch_column));
        }
    }

    private static void setWeight(View view, float weight) {
        LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) view.getLayoutParams();
        params.weight = weight;
        view.setLayoutParams(params);
    }

    private static void useFullWidth(View view) {
        LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) view.getLayoutParams();
        params.width = ViewGroup.LayoutParams.MATCH_PARENT;
        params.weight = 0f;
        params.leftMargin = 0;
        params.rightMargin = 0;
        view.setLayoutParams(params);
    }

    private static int dp(View view, float value) {
        return Math.round(value * view.getResources().getDisplayMetrics().density);
    }

    private static void decorate(View view, Typeface body, Typeface heading,
            Bitmap[] frame, Bitmap compact, Bitmap mainButton, Bitmap backButton,
            Bitmap actionButton, Bitmap headerButton, Bitmap track, Bitmap knob, Bitmap checkbox,
            Bitmap mainPanel, Bitmap touchPanel, Bitmap optionsPanel, Bitmap pathPanel) {
        if ("zone_panel".equals(view.getTag())) {
            int panelId = view.getId();
            Bitmap atlas;
            int leftSlice, topSlice, rightSlice, bottomSlice;
            if (panelId == R.id.launcher_path_panel) {
                atlas = pathPanel;
                leftSlice = 18; topSlice = 18; rightSlice = 18; bottomSlice = 18;
                view.setBackground(new DdsAtlasPanel(atlas, view.getResources(),
                        leftSlice, topSlice, rightSlice, bottomSlice));
            } else {
                if (panelId == R.id.launcher_options_panel) {
                    atlas = optionsPanel;
                    leftSlice = 20; topSlice = 34; rightSlice = 11; bottomSlice = 16;
                } else {
                    atlas = panelId == R.id.launcher_touch_panel ? touchPanel : mainPanel;
                    leftSlice = 28; topSlice = 45; rightSlice = 17; bottomSlice = 16;
                }
                view.setBackground(new DdsAtlasPanel(atlas, view.getResources(),
                        leftSlice, topSlice, rightSlice, bottomSlice));
            }
            // Label sits on the real DDS top header, not on an extra colored bar.
            if (panelId == R.id.launcher_launch_panel) {
                view.setPadding(dp(view, 25), dp(view, 33), dp(view, 11), dp(view, 15));
            } else {
                view.setPadding(dp(view, 17), dp(view, 5), dp(view, 17), dp(view, 17));
            }
        }
        if (view instanceof TextView) {
            TextView text = (TextView) view;
            text.setTypeface(body);
            text.setIncludeFontPadding(false);
            if ("zone_heading".equals(view.getTag())) {
                text.setTypeface(heading);
                text.setTextColor(AMBER);
                text.setLetterSpacing(.025f);
                text.setShadowLayer(dp(view, 1), 0, dp(view, 1), 0xFF16120C);
                // Header labels are printed directly over the thick DDS beam.
                // The page title stays unframed above the settings columns.
                text.setBackgroundColor(Color.TRANSPARENT);
                text.setGravity(Gravity.CENTER);
                // The source DDS title beam is on the LEFT part of the upper edge,
                // not in the geometrical center of these stretched atlas panels.
                ViewGroup parent = view.getParent() instanceof ViewGroup ?
                        (ViewGroup) view.getParent() : null;
                if (parent != null) {
                    int parentId = parent.getId();
                    // Path: text centered across the ui_frame panel.
                    // Touch: beam occupies the LEFT portion of ui_common3.
                    if (parentId == R.id.launcher_path_panel ||
                            parentId == R.id.launcher_touch_panel) {
                        ViewGroup.LayoutParams params = text.getLayoutParams();
                        params.width = ViewGroup.LayoutParams.MATCH_PARENT;
                        text.setLayoutParams(params);
                        text.setTranslationX(parentId == R.id.launcher_touch_panel ?
                                -dp(view, 32) : 0f);
                    } else if (parentId == R.id.launcher_options_panel) {
                        ViewGroup.LayoutParams params = text.getLayoutParams();
                        params.width = dp(view, 220);
                        text.setLayoutParams(params);
                        text.setTranslationX(dp(view, 43));
                    }
                }
            }
        }
        if (view instanceof EditText) {
            view.setBackgroundTintList(null);
        }
        if (view instanceof CompoundButton) {
            CompoundButton button = (CompoundButton) view;
            button.setButtonTintList(null);
            button.setButtonDrawable(new Toggle(view.getResources(), view instanceof RadioButton, checkbox));
            button.setMinimumHeight(dp(view, 26));
            button.setCompoundDrawablePadding(dp(view, 3));
        }
        if (view instanceof Button && !(view instanceof CompoundButton)) {
            Button button = (Button) view;
            int id = view.getId();
            int kind = (id == R.id.btn_open_settings || id == R.id.btn_start_game) ? 1 :
                    id == R.id.btn_settings_back ? 2 :
                    (id == R.id.btn_browse_path || id == R.id.btn_check_path ||
                     id == R.id.btn_view_logs || id == R.id.btn_report_bug) ? 3 : 4;
            Bitmap texture = kind == 1 ? mainButton : kind == 2 ? backButton :
                    kind == 3 ? actionButton : compact;
            if (button instanceof AppCompatButton) {
                ((AppCompatButton) button).setSupportBackgroundTintList(null);
            }
            button.setBackgroundTintList(null);
            button.setBackground(new DdsButton(texture, view.getResources()));
            button.setTypeface(heading);
            button.setAllCaps(false);
            button.setMinimumHeight(0);
            button.setMinimumWidth(0);
            button.setLetterSpacing(.015f);
            button.setTextColor(id == R.id.btn_view_logs || id == R.id.btn_edit_touch_layout ?
                    AMBER : 0xFFE7E4D8);
            button.setShadowLayer(dp(view, 1), 0, dp(view, 1), 0xFF18120E);
        }
        if (view instanceof SeekBar) {
            SeekBar seek = (SeekBar) view;
            LayerDrawable layers = new LayerDrawable(new Drawable[] {
                    new DdsTrack(track, false),
                    new ClipDrawable(new DdsTrack(track, true), Gravity.START, ClipDrawable.HORIZONTAL) });
            layers.setId(0, android.R.id.background);
            layers.setId(1, android.R.id.progress);
            layers.setLayerGravity(0, Gravity.CENTER_VERTICAL | Gravity.FILL_HORIZONTAL);
            layers.setLayerGravity(1, Gravity.CENTER_VERTICAL | Gravity.FILL_HORIZONTAL);
            layers.setLayerHeight(0, dp(view, 5));
            layers.setLayerHeight(1, dp(view, 5));
            seek.setProgressDrawable(layers);
            seek.setProgressTintList(null);
            seek.setProgressBackgroundTintList(null);
            seek.setThumb(new DdsThumb(knob, view.getResources()));
            seek.setThumbTintList(null);
            seek.setSplitTrack(false);
        }
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) {
                decorate(group.getChildAt(i), body, heading, frame, compact, mainButton,
                        backButton, actionButton, headerButton, track, knob, checkbox,
                        mainPanel, touchPanel, optionsPanel, pathPanel);
            }
        }
    }

    private static final class Toggle extends Drawable {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Path tick = new Path();
        private final boolean radio;
        private final Bitmap texture;
        private final RectF checkboxRect = new RectF();
        private final float density;
        private final int size;
        private boolean checked;

        Toggle(Resources resources, boolean radio, Bitmap texture) {
            this.radio = radio;
            this.texture = texture;
            density = resources.getDisplayMetrics().density;
            size = Math.round(20 * density);
        }
        @Override public int getIntrinsicWidth() { return size; }
        @Override public int getIntrinsicHeight() { return size; }
        @Override public boolean isStateful() { return true; }
        @Override protected boolean onStateChange(int[] states) {
            boolean value = false;
            for (int state : states) if (state == android.R.attr.state_checked) value = true;
            if (value != checked) { checked = value; invalidateSelf(); return true; }
            return false;
        }
        @Override public void draw(Canvas canvas) {
            Rect bounds = getBounds();
            float x = bounds.exactCenterX(), y = bounds.exactCenterY(), r = 7.7f * density;
            paint.setColor(checked ? AMBER : 0xFF9D9280);
            paint.setStrokeWidth(1.4f * density);
            paint.setStyle(Paint.Style.STROKE);
            if (radio) {
                canvas.drawCircle(x, y, r, paint);
                if (checked) {
                    paint.setStyle(Paint.Style.FILL);
                    canvas.drawCircle(x, y, 4.3f * density, paint);
                }
            } else {
                checkboxRect.set(x-r, y-r, x+r, y+r);
                if (texture != null) {
                    paint.setStyle(Paint.Style.FILL);
                    canvas.drawBitmap(texture, null, checkboxRect, paint);
                }
                paint.setStyle(Paint.Style.STROKE);
                canvas.drawRect(x-r, y-r, x+r, y+r, paint);
                if (!checked) return;
                paint.setStyle(Paint.Style.STROKE);
                paint.setColor(0xFFE3C88F);
                paint.setStrokeWidth(1.8f * density);
                paint.setStrokeCap(Paint.Cap.ROUND);
                tick.reset();
                tick.moveTo(x - 4.1f * density, y);
                tick.lineTo(x - 1.1f * density, y + 3.2f * density);
                tick.lineTo(x + 4.6f * density, y - 3.5f * density);
                canvas.drawPath(tick, paint);
            }
        }
        @Override public void setAlpha(int alpha) { paint.setAlpha(alpha); invalidateSelf(); }
        @Override public void setColorFilter(ColorFilter filter) { paint.setColorFilter(filter); invalidateSelf(); }
        @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
    }

    /** Nine-slice original DDS atlas panel. Every source pixel, including its
     *  native alpha, is preserved. No artificial translucent backing is drawn.
     */
    private static final class DdsAtlasPanel extends Drawable {
        private final Bitmap atlas;
        private final Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.DITHER_FLAG);
        private final Rect src = new Rect();
        private final RectF dst = new RectF();
        private final int left, top, right, bottom;
        private final float density;

        DdsAtlasPanel(Bitmap atlas, Resources resources,
                int left, int top, int right, int bottom) {
            this.atlas = atlas;
            this.left = left; this.top = top; this.right = right; this.bottom = bottom;
            this.density = resources.getDisplayMetrics().density;
        }
        @Override public void draw(Canvas canvas) {
            if (atlas == null) return;
            Rect b = getBounds();
            if (b.width() <= 0 || b.height() <= 0) return;
            int w = atlas.getWidth(), h = atlas.getHeight();
            float l = Math.min(left * density, b.width() * .25f);
            float r = Math.min(right * density, b.width() * .25f);
            float t = Math.min(top * density, b.height() * .30f);
            float bot = Math.min(bottom * density, b.height() * .22f);
            int[] xs = {0, left, w - right, w};
            int[] ys = {0, top, h - bottom, h};
            float[] dx = {b.left, b.left + l, b.right - r, b.right};
            float[] dy = {b.top, b.top + t, b.bottom - bot, b.bottom};
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 3; col++) {
                    src.set(xs[col], ys[row], xs[col + 1], ys[row + 1]);
                    dst.set(dx[col], dy[row], dx[col + 1], dy[row + 1]);
                    canvas.drawBitmap(atlas, src, dst, paint);
                }
            }
        }
        @Override public void setAlpha(int alpha) { paint.setAlpha(alpha); invalidateSelf(); }
        @Override public void setColorFilter(ColorFilter filter) {
            paint.setColorFilter(filter); invalidateSelf();
        }
        @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
    }

    /** Full ui_frame_* DDS frame for the Game Path card. Unlike a sliced
     *  inventory atlas it keeps each corner intact and avoids distorted edges.
     */
    private static final class DdsNineFramePanel extends Drawable {
        private final Bitmap[] pieces;
        private final Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.DITHER_FLAG);
        private final RectF dst = new RectF();
        private final float density;
        DdsNineFramePanel(Bitmap[] pieces, Resources res) {
            this.pieces = pieces;
            this.density = res.getDisplayMetrics().density;
        }
        @Override public void draw(Canvas canvas) {
            Rect b = getBounds();
            if (b.isEmpty()) return;
            float left = Math.min(22f * density, b.width() / 4f);
            float right = left;
            float top = Math.min(30f * density, b.height() / 3f);
            float bottom = Math.min(16f * density, b.height() / 3f);
            float[] xs = { b.left, b.left + left, b.right - right, b.right };
            float[] ys = { b.top, b.top + top, b.bottom - bottom, b.bottom };
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 3; col++) {
                    Bitmap piece = pieces[row * 3 + col];
                    if (piece == null) continue;
                    dst.set(xs[col], ys[row], xs[col + 1], ys[row + 1]);
                    canvas.drawBitmap(piece, null, dst, paint);
                }
            }
        }
        @Override public void setAlpha(int alpha) { paint.setAlpha(alpha); invalidateSelf(); }
        @Override public void setColorFilter(ColorFilter value) { paint.setColorFilter(value); invalidateSelf(); }
        @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
    }

    /** A header strip from the supplied ui_btn_mm_h.dds, without a green overlay. */
    private static final class DdsHeader extends Drawable {
        private final DdsButton image;
        DdsHeader(Bitmap bitmap) { image = new DdsButton(bitmap, null); }
        @Override public void draw(Canvas canvas) { image.setBounds(getBounds()); image.draw(canvas); }
        @Override public void setAlpha(int value) { image.setAlpha(value); invalidateSelf(); }
        @Override public void setColorFilter(ColorFilter value) { image.setColorFilter(value); invalidateSelf(); }
        @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
    }

    /** Horizontal 3-slice of an original supplied DDS button; no colored fill blocks. */
    private static final class DdsButton extends Drawable {
        private final Bitmap texture;
        private final Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.DITHER_FLAG);
        private final Paint highlight = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Rect src = new Rect();
        private final RectF dst = new RectF();
        private final float inset;
        private boolean pressed;
        DdsButton(Bitmap texture, Resources resources) {
            this.texture = texture;
            inset = 19f * (resources == null ? 1f : resources.getDisplayMetrics().density);
        }
        @Override public boolean isStateful() { return true; }
        @Override protected boolean onStateChange(int[] states) {
            boolean next = false;
            for (int state : states) if (state == android.R.attr.state_pressed ||
                    state == android.R.attr.state_focused) next = true;
            if (pressed == next) return false;
            pressed = next; invalidateSelf(); return true;
        }
        @Override public void draw(Canvas canvas) {
            if (texture == null) return;
            Rect b = getBounds();
            if (b.width() <= 0 || b.height() <= 0) return;
            int w = texture.getWidth();
            int cap = Math.min(19, Math.max(1, w / 5));
            float s = Math.min(inset, b.width() / 4f);
            float[] xx = {b.left, b.left + s, b.right - s, b.right};
            int[] sx = {0, cap, w - cap, w};
            for (int col = 0; col < 3; col++) {
                src.set(sx[col], 0, sx[col + 1], texture.getHeight());
                dst.set(xx[col], b.top, xx[col + 1], b.bottom);
                canvas.drawBitmap(texture, src, dst, paint);
            }
            if (pressed) {
                highlight.setColor(0x24FFFFFF);
                dst.set(b.left + s, b.top + 3, b.right - s, b.bottom - 3);
                canvas.drawRect(dst, highlight);
            }
        }
        @Override public void setAlpha(int alpha) { paint.setAlpha(alpha); invalidateSelf(); }
        @Override public void setColorFilter(ColorFilter value) {paint.setColorFilter(value);invalidateSelf();}
        @Override public int getOpacity() {return PixelFormat.TRANSLUCENT;}
    }

    /** Real ui_scb_back DDS texture, not a platform Material slider. */
    private static final class DdsTrack extends Drawable {
        private final Bitmap image;
        private final Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.DITHER_FLAG);
        private final Rect src = new Rect();
        private final RectF dst = new RectF();
        DdsTrack(Bitmap image, boolean active) {
            this.image = image;
            if (active) paint.setColorFilter(new PorterDuffColorFilter(0xFFB39D75, PorterDuff.Mode.MULTIPLY));
        }
        @Override public void draw(Canvas canvas) {
            if (image==null) return;
            Rect b=getBounds();
            src.set(0, 4, image.getWidth(), Math.min(image.getHeight(), 12));
            dst.set(b.left,b.top,b.right,b.bottom);
            canvas.drawBitmap(image,src,dst,paint);
        }
        @Override public void setAlpha(int alpha) {paint.setAlpha(alpha);invalidateSelf();}
        @Override public void setColorFilter(ColorFilter value) {paint.setColorFilter(value);invalidateSelf();}
        @Override public int getOpacity() {return PixelFormat.TRANSLUCENT;}
    }

    /** Supplied ui_scb_scroll_box DDS as a scalable, touch-sized knob. */
    private static final class DdsThumb extends Drawable {
        private final Bitmap image;
        private final Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.DITHER_FLAG);
        private final RectF dst = new RectF();
        private final int size;
        DdsThumb(Bitmap image, Resources resources) {
            this.image=image;
            size=Math.round(40f*resources.getDisplayMetrics().density);
        }
        @Override public int getIntrinsicWidth() {return size;}
        @Override public int getIntrinsicHeight() {return Math.round(size * 25f / 56f);}
        @Override public void draw(Canvas canvas) {
            if (image==null) return;
            Rect b=getBounds(); dst.set(b.left,b.top,b.right,b.bottom);
            canvas.drawBitmap(image,null,dst,paint);
        }
        @Override public void setAlpha(int alpha) {paint.setAlpha(alpha);invalidateSelf();}
        @Override public void setColorFilter(ColorFilter value) {paint.setColorFilter(value);invalidateSelf();}
        @Override public int getOpacity() {return PixelFormat.TRANSLUCENT;}
    }
}
