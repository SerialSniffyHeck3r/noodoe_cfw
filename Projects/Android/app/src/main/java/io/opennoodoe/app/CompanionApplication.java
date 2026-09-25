package io.opennoodoe.app;
/** Locale setup does not recreate or own the Bluetooth service. */
public final class CompanionApplication extends android.app.Application {
 @Override public void onCreate(){super.onCreate();UiText.initialize(this);}
}
