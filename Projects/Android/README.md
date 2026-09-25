# FuckNudo Android companion

This is the Gradle/Android Studio project for the CFW companion and installer. It connects to a compatible Noodoe through Classic Bluetooth SPP, supplies media, notifications, calls and phone GPS, and manages the explicit install/update/recovery workflows. The app is not the original KYMCO app; the original OpenNoodoe source tree is not mirrored here.

Open this directory in Android Studio or run `./gradlew :app:assembleDebug` with a local Android SDK. Local `local.properties`, Gradle caches, signing keys and all APK/build outputs are intentionally excluded. The in-tree unit tests can be run with `./gradlew :app:testDebugUnitTest`; that does not replace testing Bluetooth and Android background-service behavior on a real phone.

`tools/` contains package/release helpers. A complete firmware package additionally needs the matching STM32 outputs, an approved exact stock input and device-profile evidence. See [installer documentation](../documentations/installer.md). The source includes third-party fonts and libraries under their own licenses; no repository-wide commercial-use license is granted.
