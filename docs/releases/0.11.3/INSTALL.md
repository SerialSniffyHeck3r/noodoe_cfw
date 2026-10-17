# NewDoe 0.11.3 beta

## 0.11.3 language stability update

From 0.11.2, install **the 0.11.3 APK and NewDoe-CFW-0.11.3.zip** through CFW update. Gate is byte-for-byte identical to 0.11.2, so this path does not require stock return. Older Gates retain the existing keep-data stock-return migration requirement.

- Reuse **NewDoe-LANGUAGE-B-0.11.2.zip**. Its older filename is intentional: resource contents are unchanged. A matching verified pack is checked without retransmission.
- An older installed pack gets an explicit upgrade offer. Later preserves the existing pack and language preference.
- Transfer, verification/saving, automatic restart and actual menu activation are separate statuses. A lost final reply triggers fresh device readback, not a firmware reinstallation or blind transfer replay.
- Turn English only off, choose the device language, then accept the restart prompt on the stopped device. Follow app applies the app language at the next boot; a manual choice is not overwritten on reconnection.
- Transient read-only storage errors get two additional attempts. Published text and glyph caches remain usable during a recheck. Persistent errors fall back to English and retain preferences; fonts are not automatically repaired.

Hardware check: choose Korean, accept restart, and verify the Korean settings without tapping the app. Repeat with Japanese, then cycle power and verify the saved choice. With the current pack already installed, Install/resume resources should show the active language without retransmitting. If errors persist, retain diagnostics and use Install/resume resources to recheck. Firmware completion and language resource failure remain separate.

Keep the vehicle stopped and the module powered. Ignition ON/OFF does not cancel resource installation. Confirmed font damage or insufficient storage retains the existing explicit recovery/English-only choices.

## 0.11.3 언어 안정성 업데이트

0.11.2를 사용 중이면 **0.11.3 APK + NewDoe-CFW-0.11.3.zip**으로 CFW 업데이트하세요. Gate는 0.11.2와 동일하므로 이 경로에서는 순정 복귀가 필요하지 않습니다. 더 오래된 Gate는 기존의 데이터 유지 순정 복귀 안내를 따릅니다.

- 언어팩은 **NewDoe-LANGUAGE-B-0.11.2.zip**을 그대로 사용합니다. 파일명이 이전 버전이어도 맞습니다. 이미 설치된 팩의 실제 해시가 같으면 다시 보내지 않습니다.
- 이전 팩이 있으면 갱신 여부를 따로 묻습니다. 나중에를 선택하면 현재 팩과 언어 선호도를 유지합니다.
- 전송 완료 → 검증·저장 → 자동 재시작 → 실제 메뉴 언어 확인을 구분합니다. 저장 뒤 마지막 응답이 끊겨도 다시 연결해 기기 상태를 확인하며, 펌웨어를 다시 설치하지 않습니다.
- 언어를 바꿀 때는 영어 전용을 끄고 원하는 언어를 선택한 뒤, 정차 상태에서 기기의 재시작 안내를 확인하세요. 앱 따르기는 앱의 언어를 다음 부팅에 적용하고, 직접 선택한 언어는 재연결로 바뀌지 않습니다.
- 부팅 중 일시적 읽기 오류는 두 차례 더 읽어봅니다. 검증을 통과한 번역·글자 캐시는 재검사 때문에 갑자기 영어로 바뀌지 않습니다. 지속적인 오류는 영어로 대체하고 선호도를 보존합니다. 자동 폰트 복구는 하지 않습니다.

확인 순서: 한국어 선택 → 기기의 재시작 확인 → 앱을 누르지 않고 한국어 설정 화면 확인 → 일본어로 같은 과정 반복 → 전원을 다시 켜도 선택 언어가 유지되는지 확인. 이미 현재 팩이 설치돼 있다면 언어 자원 설치/이어가기에서 재전송 없이 실제 적용 언어가 표시돼야 합니다. 읽기 오류가 계속되면 진단을 보존하고 언어 자원 설치/이어가기로 다시 확인하세요. 펌웨어 완료와 언어 자원 오류는 별개입니다.

주행 중에는 설치하지 마세요. IGN ON/OFF는 설치를 취소하지 않지만 상시 전원은 유지해야 합니다. 실제 저장 공간이 부족하거나 폰트 손상이 확정되면 기존 선택 안내를 따릅니다.

Software-verified beta; physical LCD, radio and storage behavior still need the checks above. The previously accepted old-code power-loss limitation during stock-return migration remains. Gate, Bootstrap, pairing and maps are unchanged.
