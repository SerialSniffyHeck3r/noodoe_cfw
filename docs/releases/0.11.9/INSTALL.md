# NewDoe 0.11.9 beta — phone dashboard

Swipe between **Device · Home · Personalize**. Home shows an offline map, speed and ODO, four swipeable trip readouts, and Oil/Belt/Service distances with interval bars. The top play/pause button controls the existing riding connection. Device keeps installation, language resources and recovery; Personalize groups device settings, maps, phone features and wallpapers. Existing installation approvals and pairing behavior are unchanged.

The phone receives existing counters on the same Bluetooth connection, about every 2 seconds on Home, 5 seconds during visual transfers, and 10 seconds elsewhere. It calculates service reminders and posts Android notifications once per service cycle. Oil means distance since an oil-service baseline, not measured oil quantity. Offline values show their last receipt time; unavailable speed/counters show a dash.

## Install

1. Install **NewDoe-0.11.9.apk** over the existing app. Its signing identity and app data are preserved.
2. In **Device**, choose **NewDoe-CFW-0.11.9.zip** and follow the existing update wizard. The new counter response requires the matching firmware; installing the APK alone does not add it to Noodoe.
3. From **0.11.4–0.11.8**, use normal CFW update. Gate and Bootstrap are byte-identical to 0.11.8. For an older Gate, follow the installer's existing stock-return/reinstall guidance. Do not bypass the device/Gate checks.
4. Working **NewDoe-LANGUAGE-B-0.11.2.zip** resources remain valid and do not need resending. Confirm the Product screen when requested, then use the top play button for riding.
5. Set service intervals/baselines through the existing device settings. Allow Android notifications for maintenance reminders. Choose an offline map under Personalize → Riding & maps.

This changes no Gate, Bootstrap, stock bootloader, boot-trial or pairing state machine. The optional Bootstrap Bluetooth-reset question still defaults to **No**. Existing data preservation and recovery remain in place.

## 한국어

**기기 · 홈 · 꾸미기**를 좌우로 넘깁니다. 홈에는 지도, 속도·ODO, 옆으로 넘기는 네 가지 트립, 오일·벨트·정기 점검 거리와 주기 막대가 표시됩니다. 상단 재생/일시정지는 기존 주행 연동을 조작합니다. 설치·언어팩·복구는 기기 페이지, 지도·휴대전화·배경 설정은 꾸미기 페이지에 모았습니다.

**APK와 같은 0.11.9 펌웨어 ZIP을 함께 사용하세요.** APK만 설치하면 누도에 새 데이터 응답 기능이 추가되지는 않습니다. 0.11.4~0.11.8은 일반 CFW 업데이트를 사용합니다. 더 오래된 Gate는 기존 설치 안내의 순정 복귀·재설치 경로를 따릅니다. 정상 언어 자원은 그대로 사용하며 재전송할 필요가 없습니다. 업데이트 확인 및 페어링 승인 절차는 바꾸지 않았습니다.

누도는 기존 카운터만 전송합니다. 정비 주기 해석과 알림은 휴대폰이 담당하며, 같은 정비 주기에 알림을 반복하지 않습니다. 오일 표시는 실제 오일 잔량 센서가 아니라 교환 기준점 이후 거리입니다. 연결이 끊기면 마지막 수신 시각을 표시합니다.

## Validation and limits

Both Product builds retain **592 bytes free**, above the 512-byte minimum; compiler flags, memory reservations and device visuals are unchanged. APK signature, matching firmware contract, installation regressions, counter behavior and native Android UI are checked. Physical phone APK installation/UI checks are recorded separately from software tests. No new firmware flashing, vehicle counter or RF endurance qualification was performed on Noodoe.

The accepted limitation in already-installed old stock-return code remains: a power interruption in that old-code transition is not guaranteed to recover automatically. Keep module power stable during installation. See verification.json, SHA256SUMS.txt and the release validation report.
