package org.openxray;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.KeyEvent;

/**
 * Low-contrast, touch-driven R1/R2-independent X-Ray quick selector.
 * Does not query inventory: it dispatches the exact existing slot/quick-item keys.
 * No game archive textures or native renderer changes are required.
 */
final class RadialWeaponWheel {
    private static final int SECTORS = 8;
    private static final float START_ANGLE = -112.5f;
    private static final String[] LABELS = {
        "НОЖ", "ПИСТОЛЕТ", "ОСНОВНОЕ", "ГРАНАТА",
        "БИНОКЛЬ", "БОЛТ", "АПТЕЧКА", "БИНТ"
    };
    private static final String[] HINTS = {
        "1", "2", "3", "4", "5", "6", "[", "]"
    };
    private static final int[] KEYS = {
        KeyEvent.KEYCODE_1, KeyEvent.KEYCODE_2, KeyEvent.KEYCODE_3,
        KeyEvent.KEYCODE_4, KeyEvent.KEYCODE_5, KeyEvent.KEYCODE_6,
        KeyEvent.KEYCODE_LEFT_BRACKET, KeyEvent.KEYCODE_RIGHT_BRACKET
    };

    private RadialWeaponWheel() {}

    static float radius(float width, float height) {
        return Math.min(height * 0.272f, width * 0.232f);
    }

    static int selection(float x, float y, float cx, float cy, float radius) {
        final float dx = x - cx;
        final float dy = y - cy;
        final float d = (float) Math.hypot(dx, dy);
        if (d < radius * 0.25f || d > radius * 1.18f) return -1;
        float angle = (float) Math.toDegrees(Math.atan2(dy, dx));
        float fromTop = (angle + 90f + 22.5f + 360f) % 360f;
        return ((int) (fromTop / 45f)) % SECTORS;
    }

    static int keyFor(int index) {
        return index >= 0 && index < KEYS.length ? KEYS[index] : KeyEvent.KEYCODE_UNKNOWN;
    }

    static void draw(Canvas canvas, float width, float height, float cx, float cy,
                     int selected, float overlayAlpha) {
        final float radius = radius(width, height);
        final float inner = radius * 0.25f;
        final float dim = Math.max(0.24f, Math.min(1.0f, overlayAlpha));
        final RectF outer = new RectF(cx - radius, cy - radius, cx + radius, cy + radius);
        final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
        fill.setStyle(Paint.Style.FILL);
        final Paint line = new Paint(Paint.ANTI_ALIAS_FLAG);
        line.setStyle(Paint.Style.STROKE);
        line.setStrokeWidth(Math.max(1.0f, height * 0.0016f));
        final Paint label = new Paint(Paint.ANTI_ALIAS_FLAG);
        label.setStyle(Paint.Style.FILL);
        label.setTextAlign(Paint.Align.CENTER);

        for (int i = 0; i < SECTORS; i++) {
            boolean active = i == selected;
            fill.setColor(Color.argb((int) ((active ? 162f : 140f) * dim),
                    active ? 58 : 22, active ? 53 : 26, active ? 39 : 25));
            canvas.drawArc(outer, START_ANGLE + i * 45f, 45f, true, fill);
        }

        fill.setColor(Color.argb((int) (220 * dim), 12, 15, 14));
        canvas.drawCircle(cx, cy, inner, fill);
        line.setColor(Color.argb((int) (160 * dim), 126, 129, 112));
        canvas.drawCircle(cx, cy, radius, line);
        canvas.drawCircle(cx, cy, inner, line);

        for (int i = 0; i < SECTORS; i++) {
            final double edge = Math.toRadians(START_ANGLE + i * 45.0);
            final float cos = (float) Math.cos(edge);
            final float sin = (float) Math.sin(edge);
            line.setColor(Color.argb((int) (125 * dim), 112, 116, 104));
            canvas.drawLine(cx + cos * inner, cy + sin * inner,
                    cx + cos * radius, cy + sin * radius, line);

            if (i == selected) {
                line.setColor(Color.argb((int) (178 * dim), 152, 133, 88));
                line.setStrokeWidth(Math.max(1.5f, height * 0.0028f));
                canvas.drawArc(outer, START_ANGLE + i * 45f + 0.6f, 43.8f, false, line);
                line.setStrokeWidth(Math.max(1.0f, height * 0.0016f));
            }

            final double mid = Math.toRadians(-90.0 + i * 45.0);
            final float tx = cx + (float) Math.cos(mid) * radius * 0.69f;
            final float ty = cy + (float) Math.sin(mid) * radius * 0.69f;
            label.setColor(Color.argb((int) ((i == selected ? 232 : 176) * dim),
                    i == selected ? 205 : 178, i == selected ? 190 : 182,
                    i == selected ? 145 : 164));
            label.setTextSize(radius * (LABELS[i].length() > 6 ? 0.079f : 0.100f));
            canvas.drawText(LABELS[i], tx, ty - radius * 0.010f, label);
            label.setColor(Color.argb((int) (147 * dim), 150, 149, 137));
            label.setTextSize(radius * 0.068f);
            canvas.drawText(HINTS[i], tx, ty + radius * 0.092f, label);
        }

        label.setColor(Color.argb((int) (188 * dim), 178, 175, 157));
        label.setTextSize(radius * 0.110f);
        canvas.drawText("×", cx, cy - radius * 0.018f, label);
        label.setTextSize(radius * 0.060f);
        canvas.drawText("ОТМЕНА", cx, cy + radius * 0.108f, label);
    }
}
