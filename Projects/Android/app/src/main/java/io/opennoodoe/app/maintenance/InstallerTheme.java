package io.opennoodoe.app.maintenance;

import android.app.Activity;
import android.os.Build;
import android.view.View;
import io.opennoodoe.app.R;
import io.opennoodoe.app.UiThemeSettings;

/** UI-only palette. Does not touch companion renderers or any transfer service. */
final class InstallerTheme {
    private InstallerTheme() {}
    static void apply(Activity activity) {
        boolean dark=UiThemeSettings.isDark(activity);
        activity.setTheme(dark?R.style.AppThemeDark:R.style.AppThemeLight);
        activity.getWindow().setBackgroundDrawable(new android.graphics.drawable.ColorDrawable(background(activity)));
        int flags=activity.getWindow().getDecorView().getSystemUiVisibility();
        flags=dark?flags&~View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR:flags|View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
        if(Build.VERSION.SDK_INT>=26)flags=dark?flags&~View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR:flags|View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
        activity.getWindow().getDecorView().setSystemUiVisibility(flags);
    }
    static int error(Activity a) { return UiThemeSettings.isDark(a)?0xffff8a80:0xffb3261e; }
    static int warning(Activity a) { return UiThemeSettings.isDark(a)?0xffffcf70:0xff795500; }
    static int background(Activity a) { return UiThemeSettings.isDark(a)?0xff111413:0xfff3f6f4; }
}
