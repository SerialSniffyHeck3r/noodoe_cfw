# NewDoe0.11.6 beta

## 0.11.6 Bootstrap display and optional re-pairing

**From0.11.4 or0.11.5, use normal CFW update.** Gate and language resources are unchanged. Older Gates still require keep-data stock return and reinstallation using the matching APK/ZIP.

The nine initial file checks use a horizontal loading bar without numbers. During installation, the pale mint upper bar is overall progress; the mint lower bar is the current device operation. The overall bar combines completed stages with the current operation’s fraction, instead of counting that stage as already complete. Percentages are hidden on the Noodoe. The phone uses the same snapshot, subject to polling delay.

Choosing **Install CFW** opens **Reset Bluetooth?**. **Yes is recommended** and clears all saved phone pairings; it is never automatically confirmed. **No** keeps the current pairing. After choosing Yes:

1. Wait for **PAIR YOUR PHONE** on the Noodoe.
2. In Android Bluetooth settings, forget this Noodoe.
3. Pair the same Noodoe again.
4. Return to the app, choose **Check current device → Continue from here**. Do not resend Bootstrap.

The existing connection closes during reset. If CFW configuration storage exists, cleared keys are saved before Bluetooth restarts. On a first installation without that storage, re-pairing is held in the running Bootstrap and the new keys are saved before final installation. Persistence across a power restart before installation is not claimed in that case. Reset/save failures are not reported as success.

The final **INSTALL CFW? / Everything is ready.** screen shows **Back / Install CFW** side by side. Its30-second countdown approves installation through the existing path. UP/DOWN stops the countdown and changes the selection; O executes it. Back cancels; Install CFW approves immediately. Connection loss stops automatic approval.

**Developer’s message** uses the entire circular screen for contiguous text. **DOWN advances one screen, UP goes back one screen**, either end wraps, and O returns to the menu. Original text and font size are preserved; the fixed heading/footer no longer consume reading space.

Bootstrap runs during installation from stock. Existing CFW users need not return to stock merely to see the changed menus. To run this new Bootstrap, use this ZIP during stock installation; normal Product update replaces Product only. Use the matching0.11.6 APK and ZIP.

## 0.11.6 Bootstrap 화면과 선택형 재페어링

**0.11.4·0.11.5에서는 일반 CFW 업데이트를 사용합니다.** Gate와 언어 자원은 같습니다. 더 오래된 Gate에서는 기존 안내대로 CFW 데이터를 유지하여 순정 복귀 후 최신 APK·ZIP으로 재설치합니다.

최초 9개 파일 검사는 숫자 없는 가로 로딩바로 표시합니다. 설치 중 위쪽 연한 민트색 막대는 전체 진행률, 아래쪽 민트색 막대는 현재 기기 작업입니다. 전체 막대는 완료한 단계와 현재 작업의 진행분을 합쳐 채웁니다. 누도에는 퍼센트를 표시하지 않습니다. 앱은 같은 상태값으로 계산하며 조회 주기만큼 표시 시점이 다를 수 있습니다.

메뉴에서 **Install CFW → Reset Bluetooth?**가 나옵니다. **Yes를 권장**하며 저장된 모든 휴대폰 페어링을 초기화합니다. 자동으로 Yes를 선택하지 않습니다. **No**는 초기화 없이 기존 연결을 사용합니다. Yes를 선택했다면:

1. 누도의 **PAIR YOUR PHONE** 안내를 기다립니다.
2. Android Bluetooth 설정에서 이 Noodoe를 등록 해제합니다.
3. 같은 Noodoe를 새로 페어링합니다.
4. 앱으로 돌아와 **현재 기기 확인 → 여기서부터 계속하기**로 설치를 이어갑니다. Bootstrap을 다시 전송하지 않습니다.

초기화 도중 기존 연결이 끊기는 것은 정상입니다. 기존 CFW 설정 저장소가 있으면 삭제한 키를 저장한 뒤 Bluetooth를 다시 시작합니다. 첫 설치로 저장소가 아직 없으면 이번 Bootstrap에서 재페어링하고 최종 설치 전에 새 키를 저장합니다. 이 경우 설치 전 전원 재시작으로 초기화 상태가 보존된다고 보장하지 않습니다. 실패·저장 오류는 성공으로 표시하지 않습니다.

최종 **INSTALL CFW? / Everything is ready.** 화면은 **Back / Install CFW**를 좌우로 표시합니다. 30초 카운트다운이 끝나면 기존 절차로 설치를 승인합니다. UP/DOWN은 카운트다운을 멈추고 선택을 바꾸며, O는 선택한 항목을 실행합니다. Back은 취소, Install CFW는 즉시 승인입니다. 연결이 끊기면 자동 승인은 중단됩니다.

**Developer’s message**는 원문 전체를 큰 원형 화면에 연속 배치합니다. **DOWN은 다음 화면, UP은 이전 화면**, 끝에서는 반대 끝으로 순환하고 O는 메뉴로 돌아갑니다. 원문·글씨 크기는 그대로이며 고정 제목과 하단 안내가 차지하던 영역까지 본문을 표시합니다.

Bootstrap은 순정에서 설치할 때 실행됩니다. 이미 CFW를 쓰는 사용자가 메뉴 변경만 보려고 순정으로 돌아갈 필요는 없습니다. 새 Bootstrap을 실제로 실행하려면 순정 설치 과정에 이번 ZIP을 사용해야 하며, 일반 Product 업데이트는 실행 중인 Product만 교체합니다. 이번 APK와 ZIP은 0.11.6을 함께 사용하세요.

## Verification / 검증 범위

Software verification only; physical LCD, Bluetooth and installation tests remain to be run on a device. The original bootloader and the previously accepted old-firmware stock-return power-loss limitation are unchanged. Keep permanent power connected during stock return and installation.

소프트웨어 검증 결과입니다. 이번 버전의 실물 LCD·Bluetooth·설치는 아직 테스트하지 않았습니다. 구형 코드의 순정 복귀 중 전원 차단 제한은 그대로이며, 순정 복귀·설치 중 상시 전원을 유지해야 합니다.
