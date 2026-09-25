package io.opennoodoe.app;

import android.content.Context;
import android.content.res.Configuration;

public final class UiThemeSettings {
    public static final int AUTO = 0;
    public static final int LIGHT = 1;
    public static final int DARK = 2;
    private static final String PREF_MODE = "ui_theme_mode";

    private UiThemeSettings() {
    }

    public static int mode(Context context) {
        return context.getSharedPreferences(NoodoeService.PREFS, Context.MODE_PRIVATE)
                .getInt(PREF_MODE, AUTO);
    }

    public static void setMode(Context context, int mode) {
        int safe = mode == LIGHT || mode == DARK ? mode : AUTO;
        context.getSharedPreferences(NoodoeService.PREFS, Context.MODE_PRIVATE)
                .edit().putInt(PREF_MODE, safe).apply();
    }

    public static boolean isDark(Context context) {
        int mode = mode(context);
        if (mode == DARK) return true;
        if (mode == LIGHT) return false;
        return (context.getResources().getConfiguration().uiMode
                & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
    }
}
