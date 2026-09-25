# 2026-08-30 Noodoe Tools Initial Analysis

## Identity

The launcher-visible `Noodoe Tools` app is package `noodoe.com.navigations`.

- Launcher activity: `noodoe.com.navigation.main.NoodoeToolsActivity`
- Version name: `1.0`
- Version code: `1811131504`
- Install source: package installer, initiated from Chrome
- APK preserved under `evidence/apk/noodoe.com.navigations_1.0/`

## Permissions

Requested permissions include:

- Bluetooth: `BLUETOOTH`, `BLUETOOTH_ADMIN`, `BLUETOOTH_CONNECT`, `BLUETOOTH_SCAN`, `BLUETOOTH_ADVERTISE`
- Location: `ACCESS_FINE_LOCATION`, `ACCESS_COARSE_LOCATION`, `ACCESS_BACKGROUND_LOCATION`
- Network: `INTERNET`, `ACCESS_NETWORK_STATE`, `ACCESS_WIFI_STATE`, `CHANGE_WIFI_STATE`
- Media/camera/storage permissions

## Static Findings

The APK contains 62 dex files and appears to include a large embedded Noodoe/Sunray control stack.

Notable class/name hits:

- `noodoe/com/navigation/main/NoodoeToolsActivity`
- `noodoe/com/navigation/uart/UartFragment`
- `noodoe/com/navigation/noodoereplacement/NoodoeReplacementFragment`
- `noodoe/com/navigation/parameteradjustment/NoodoeAdjustmentFragment`
- `noodoe/com/navigation/mfi/MFIFragment`
- `noodoe/com/navigation/cloudapis/ApiClient`
- `noodoe/com/navigation/cloudapis/FireBaseApiClient`
- `com/noodoe/sunray/cmu/service/connection/SunrayBTService`
- `com/noodoe/sunray/cmu/service/connection/handler/spp/command/PacketSequenceProcessor`
- `com/noodoe/sunray/cmu/service/task/InputCommandParser`
- `com/noodoe/sunray/cmu/service/task/OutputCommandProcessor`
- `com/noodoe/sunray/cmu/service/task/SunrayControlCommandHandler`

The app has the same kind of Bluetooth stack found in the official Noodoe app:

- `BluetoothSocket`: present
- `createRfcommSocketToServiceRecord`: present
- `connectGatt`: present
- `00001101`: present

## URL / Server Findings

Extracted URL candidates:

- `http://iotc.kymco.com/`
- `https://sunrayoqctool.firebaseio.com/`
- Mixpanel and Crashlytics endpoints

Working interpretation: `Noodoe Tools` is likely a service/OQC/diagnostic tool that talks to a Kymco/Firebase backend while also containing local Sunray/Noodoe Bluetooth command infrastructure.

## Why This Matters

This APK may be more valuable than the public Noodoe app for reverse engineering because it exposes service-oriented feature names such as UART, parameter adjustment, MFI, Noodoe replacement, and command processors.

Treat it as a privileged reference sample. Do not publish credentials, tokens, link keys, backend secrets, or owner-specific vehicle identifiers from this app or its data.

