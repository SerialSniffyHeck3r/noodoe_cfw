<!-- NOODOE_LATEST_DOWNLOADS_BEGIN -->


Noodoe CFW

An independent custom firmware for the KYMCO Noodoe motorcycle dashboard.

This project replaces the original dashboard application firmware with a fully custom implementation designed to run on the stock KYMCO hardware.

It is not a simulator, companion app, or external display.
The firmware runs directly on the original dashboard installed on the motorcycle.

What does it do?

Noodoe CFW turns the factory dashboard into a programmable platform while preserving its role as a usable motorcycle instrument cluster.

Current features include:

Custom speedometer and dashboard UI
Vehicle and trip information
Bluetooth SPP communication
Media information display
Media control using the factory handlebar buttons
Persistent settings and user configuration
Custom asset and resource loading
Installation and recovery mechanisms designed to reduce the risk of permanently bricking the dashboard

The firmware has been tested on real hardware and during actual road use.

Why does this exist?

The original Noodoe platform is tightly coupled to KYMCO's software ecosystem and cloud services.

Rather than letting otherwise functional dashboard hardware become increasingly dependent on discontinued or unavailable services, this project explores what the hardware can do as a fully independent embedded platform.

The goal is not merely to modify the appearance of the original firmware.

The goal is to understand the hardware, document it, and build a usable replacement firmware from the ground up.

Project status

This is an active reverse-engineering and embedded firmware project.

It is already capable of running on real motorcycle hardware, but hardware compatibility, installation procedures, and recovery requirements should be read carefully before attempting to install it.

Do not flash firmware unless your exact hardware revision is known to be supported.

Documentation

Detailed documentation is available in the docs/ directory, including:

Hardware and memory layout
Firmware architecture
Installation and recovery
Resource and asset format
Bluetooth communication
Vehicle integration
Reverse-engineering notes
Known limitations


# 최신 다운로드

**Companion 6.11.4 — Compact two-line notifications**

- **[Android APK 다운로드](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/download/companion-v6.11.4-phone-compact-two-lines/NoodoeCompanion-6.11.4.apk)**
- **[설치 ZIP 다운로드](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/download/companion-v6.11.4-phone-compact-two-lines/NoodoeInstaller-CFW-6.11.4.zip)**
- [항상 최신 릴리스 보기](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest)

현재 버전: `companion-v6.11.4-phone-compact-two-lines`. 위 APK와 ZIP을 한 쌍으로 사용하세요.
Bootstrap 수정이 포함된 업데이트는 해당 릴리스의 설치 순서를 확인하세요.
검증 범위와 알려진 제한은 릴리스 설명에 기록되어 있습니다.
<!-- NOODOE_LATEST_DOWNLOADS_END -->

# noodoe_cfw
Noodoe CFW companion APK downloads

## 사용 설명서

[전체 기능·설치·업데이트·복구 설명서](docs/manual/README.md) — 페이지별 본체 캡처와 앱 단계별 화면.
