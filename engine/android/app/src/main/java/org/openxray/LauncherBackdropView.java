package org.openxray;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.View;

/** Only the two unmodified photos supplied by the user are used as backdrops. */
public final class LauncherBackdropView extends View {
    private final Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.DITHER_FLAG);
    private final RectF target = new RectF();
    private final Bitmap homeImage;
    private final Bitmap settingsImage;
    private boolean settingsPage;

    public LauncherBackdropView(Context context, AttributeSet attrs) {
        super(context, attrs);
        homeImage = BitmapFactory.decodeResource(getResources(), R.drawable.launcher_user_main);
        settingsImage = BitmapFactory.decodeResource(getResources(), R.drawable.launcher_user_atp);
    }

    public void setSettingsPage(boolean settingsPage) {
        if (this.settingsPage != settingsPage) {
            this.settingsPage = settingsPage;
            invalidate();
        }
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        Bitmap background = settingsPage ? settingsImage : homeImage;
        if (background == null || getWidth() == 0 || getHeight() == 0) return;
        float scale = Math.max((float) getWidth() / background.getWidth(),
                (float) getHeight() / background.getHeight());
        float width = background.getWidth() * scale;
        float height = background.getHeight() * scale;
        float left = (getWidth() - width) * 0.5f;
        float top = (getHeight() - height) * 0.5f;
        target.set(left, top, left + width, top + height);
        canvas.drawBitmap(background, null, target, paint);
        if (getResources().getConfiguration().smallestScreenWidthDp < 600) {
            canvas.drawColor(0x600B0A08);
        }
    }
}
