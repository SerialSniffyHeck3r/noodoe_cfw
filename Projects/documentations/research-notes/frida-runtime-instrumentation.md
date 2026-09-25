# Frida runtime instrumentation

Date: 2026-08-30 KST

## Confirmed setup

Target device:

- Samsung Galaxy Tab Active3
- Model: SM-T575N
- Product: gtactive3kx
- ABI: arm64-v8a
- Root: Magisk `uid=0(root)`

Test app:

- Package: `noodoe.com.navigations`
- App label: Noodoe Tools
- APK: `evidence/apk/noodoe.com.navigations_1.0/base.apk`

Frida:

- Server: `.tools/frida-server-17.17.0-android-arm64`
- Client tools: `.tools/python/bin/frida.exe`
- Required local environment in this workspace:

```powershell
$env:PYTHONPATH=(Resolve-Path .tools\python)
```

Codex's non-interactive Windows console needs TTY mode for Frida CLI tools.
In a normal PowerShell window this should not matter.

## ADB and Frida startup

The tablet was discovered over wireless ADB with:

```powershell
adb mdns services
adb connect 192.168.0.102:44447
adb devices -l
```

Frida server was installed and started with:

```powershell
adb push .tools\frida-server-17.17.0-android-arm64 /data/local/tmp/frida-server
adb shell su -c "chmod 755 /data/local/tmp/frida-server"
adb shell su -c "/data/local/tmp/frida-server >/dev/null 2>&1 &"
```

Verification:

```powershell
adb shell su -c "pidof frida-server"
.\.tools\python\bin\frida-ps.exe -Uai
```

## Login bypass result

The Noodoe Tools server login gate was bypassed at runtime with:

```powershell
$env:PYTHONPATH=(Resolve-Path .tools\python)
.\.tools\python\bin\frida.exe -U -f noodoe.com.navigations -l tools\frida\noodoe-tools-login-bypass.js
```

Runtime evidence:

- The app spawned successfully under Frida.
- `NoodoeToolsActivity.onGetStartedClicked()` was intercepted.
- `NoodoeToolsActivity.initialView(false)` was forced to `true`.
- `NoodoeToolsPresenter.getUserName()` returned `offline`.
- The UI entered the Noodoe Tools main menu.

Screenshot evidence:

- `evidence/tablet-noodoe-tools-login-bypass.png`
- `evidence/tablet-noodoe-tools-combined-hook.png`

This confirms that the first login wall is a local UI/state gate, not a hard
requirement for loading the tool UI.

## Combined hook

For later bike-side testing, use the combined login bypass and Bluetooth socket
logger:

```powershell
$env:PYTHONPATH=(Resolve-Path .tools\python)
adb shell am force-stop noodoe.com.navigations
.\.tools\python\bin\frida.exe -U -f noodoe.com.navigations -l tools\frida\noodoe-tools-offline-and-btsocket.js
```

Confirmed load messages:

- `Noodoe Tools offline-login bypass loaded`
- `using output stream class android.bluetooth.BluetoothOutputStream`
- `using input stream class android.bluetooth.BluetoothInputStream`
- `Noodoe BluetoothSocket hook loaded`
- `Noodoe Tools offline + Bluetooth hook loaded`

On this Android build, these stream overloads exist:

- `write(int)`
- `write(byte[], int, int)`
- `read()`
- `read(byte[], int, int)`

The simple `write(byte[])` and `read(byte[])` overloads were absent.

## Meaning

This is a strong validation of the revised strategy:

1. Use Frida to bypass server/UI gates.
2. Use the test app as a rich hardware-control oracle.
3. Observe high-level API calls and Bluetooth bytes from inside Android.
4. Use the motorcycle only for short validation sessions.

## Safety

The tool UI exposes manufacturing and service functions. Do not casually run:

- Factory reset
- Firmware/resource update
- OQC data write
- Replacement/write operations against a real meter
- Any function that changes ECU/meter identity fields

Near the bike, start with read-only or low-risk paths:

1. Bluetooth connect/disconnect observation.
2. Device info read.
3. Mobile status/time.
4. Harmless notification.
