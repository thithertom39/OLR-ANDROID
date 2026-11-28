package org.openxray;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.LightingColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Typeface;

/**
 * Shared in-game and editor skin. Reuses the already packaged original X-Ray
 * ui_common3 and ui_button_02 artwork (converted launcher drawables).
 * Textures are decoded and composited once per View, never on the draw path.
 * No native renderer or input mapping changes.
 */
final class XRayTouchSkin {
    private static final int PLATE_SIZE = 128;
    private static final int DISC_SIZE = 192;
    private final Bitmap plate;
    private final Bitmap disc;
    private final Paint imagePaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Paint linePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF dst = new RectF();
    private final LightingColorFilter pressedFilter = new LightingColorFilter(0xffd4c9ae, 0x00272313);

    XRayTouchSkin(Context context) {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inScaled = false;
        Bitmap panel = BitmapFactory.decodeResource(context.getResources(),
                R.drawable.launcher_panel_touch_common3, opts);
        Bitmap strip = BitmapFactory.decodeResource(context.getResources(),
                R.drawable.launcher_ui_button_02, opts);
        Paint prep = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        ColorMatrix cm = new ColorMatrix();
        cm.setSaturation(0.30f);
        prep.setColorFilter(new ColorMatrixColorFilter(cm));

        plate = Bitmap.createBitmap(PLATE_SIZE, PLATE_SIZE, Bitmap.Config.ARGB_8888);
        Canvas plateCanvas = new Canvas(plate);
        if (panel != null) {
            plateCanvas.drawBitmap(panel, null, new RectF(0, 0, PLATE_SIZE, PLATE_SIZE), prep);
        } else {
            prep.setColorFilter(null);
            prep.setColor(Color.rgb(29, 31, 28));
            plateCanvas.drawRoundRect(2, 2, PLATE_SIZE - 2, PLATE_SIZE - 2, 6, 6, prep);
        }
        if (strip != null) {
            prep.setAlpha(150);
            plateCanvas.drawBitmap(strip, null, new RectF(13, 106, 115, 122), prep);
            prep.setAlpha(255);
        }

        disc = Bitmap.createBitmap(DISC_SIZE, DISC_SIZE, Bitmap.Config.ARGB_8888);
        Canvas discCanvas = new Canvas(disc);
        Path mask = new Path();
        mask.addCircle(DISC_SIZE / 2f, DISC_SIZE / 2f, DISC_SIZE / 2f - 1f, Path.Direction.CW);
        discCanvas.save();
        discCanvas.clipPath(mask);
        if (panel != null) {
            prep.setColorFilter(new ColorMatrixColorFilter(cm));
            Rect source = new Rect(panel.getWidth() / 8, panel.getHeight() / 8,
                    panel.getWidth() * 7 / 8, panel.getHeight() * 7 / 8);
            discCanvas.drawBitmap(panel, source, new RectF(0, 0, DISC_SIZE, DISC_SIZE), prep);
        } else {
            discCanvas.drawColor(Color.rgb(28, 30, 27));
        }
        discCanvas.restore();
        prep.setColorFilter(null);
        prep.setColor(Color.rgb(116, 117, 102));
        prep.setStyle(Paint.Style.STROKE);
        prep.setStrokeWidth(2f);
        discCanvas.drawCircle(DISC_SIZE / 2f, DISC_SIZE / 2f, DISC_SIZE / 2f - 2f, prep);
        // The source Bitmaps are not retained by this skin. Do not recycle
        // resource bitmaps that might be shared by Android's drawable cache.
        linePaint.setStyle(Paint.Style.STROKE);
        textPaint.setTextAlign(Paint.Align.CENTER);
        textPaint.setTypeface(Typeface.create("sans-serif-condensed", Typeface.BOLD));
    }

    private static int alpha(float opacity, int maximum) {
        return Math.round(Math.max(0f, Math.min(1f, opacity)) * maximum);
    }

    void drawButton(Canvas canvas, RectF bounds, String label, float opacity, boolean pressed) {
        imagePaint.setAlpha(alpha(opacity, pressed ? 235 : 175));
        imagePaint.setColorFilter(pressed ? pressedFilter : null);
        canvas.drawBitmap(plate, null, bounds, imagePaint);
        imagePaint.setColorFilter(null);
        if (pressed) {
            float inset = bounds.width() * 0.045f;
            dst.set(bounds.left + inset, bounds.top + inset, bounds.right - inset, bounds.bottom - inset);
            linePaint.setStrokeWidth(Math.max(1f, bounds.width() * 0.022f));
            linePaint.setColor(Color.argb(alpha(opacity, 205), 151, 137, 98));
            canvas.drawRect(dst, linePaint);
        }
        float size = bounds.height() * (label.length() > 6 ? 0.25f :
                label.length() > 4 ? 0.30f : label.length() > 2 ? 0.34f : 0.40f);
        textPaint.setTextSize(size);
        float width = textPaint.measureText(label);
        if (width > bounds.width() * 0.75f) {
            textPaint.setTextSize(size * bounds.width() * 0.75f / width);
        }
        textPaint.setColor(Color.argb(alpha(opacity, 255), 218, 214, 195));
        textPaint.setShadowLayer(Math.max(1f, bounds.height() * 0.018f), 0f, 1f,
                Color.argb(alpha(opacity, 230), 0, 0, 0));
        canvas.drawText(label, bounds.centerX(),
                bounds.centerY() - (textPaint.ascent() + textPaint.descent()) * 0.5f
                        - bounds.height() * 0.025f, textPaint);
    }

    private void drawDisc(Canvas canvas, float x, float y, float radius, float opacity, int maxAlpha) {
        dst.set(x - radius, y - radius, x + radius, y + radius);
        imagePaint.setColorFilter(null);
        imagePaint.setAlpha(alpha(opacity, maxAlpha));
        canvas.drawBitmap(disc, null, dst, imagePaint);
    }

    void drawJoystick(Canvas canvas, float baseX, float baseY, float thumbX, float thumbY,
                      float radius, float opacity, boolean active) {
        drawDisc(canvas, baseX, baseY, radius, opacity, 112);
        linePaint.setStrokeWidth(Math.max(1f, radius * 0.017f));
        linePaint.setColor(Color.argb(alpha(opacity, 145), 137, 139, 121));
        canvas.drawCircle(baseX, baseY, radius * 0.91f, linePaint);
        float a = radius * 0.82f, b = radius * 0.92f;
        canvas.drawLine(baseX - a, baseY, baseX - b, baseY, linePaint);
        canvas.drawLine(baseX + a, baseY, baseX + b, baseY, linePaint);
        canvas.drawLine(baseX, baseY - a, baseX, baseY - b, linePaint);
        canvas.drawLine(baseX, baseY + a, baseX, baseY + b, linePaint);
        drawDisc(canvas, thumbX, thumbY, radius * 0.45f, opacity, active ? 220 : 175);
        linePaint.setColor(Color.argb(alpha(opacity, 185), 159, 154, 127));
        canvas.drawCircle(thumbX, thumbY, radius * 0.29f, linePaint);
    }

    void drawWheelBase(Canvas canvas, float x, float y, float radius, float opacity) {
        drawDisc(canvas, x, y, radius, opacity, 240);
    }
}
