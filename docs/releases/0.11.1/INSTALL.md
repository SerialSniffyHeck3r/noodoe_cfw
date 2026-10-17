# NewDoe 0.11.1 — round-screen Gate refinement

The startup logo and error skull are 240×240px, up from 160×160px. Startup shows **Welcome!**, then **Device Startup** while checking. The skull screen has no NewDoe heading. Error numbers, countdown, button handling and recovery decisions are unchanged.

When no local restoration is available, the menu says **Oh, shit. Sorry! / We're out of recovery options.** Help provides repair/replacement directions and the GitHub QR without the chip-diagnosis disclaimer. Disabled options remain grey. All Gate text is positioned within the 480px round aperture, including long reasons, footers, progress, confirmations and help pages.

## Installation

Install the matching NewDoe-0.11.1.apk and NewDoe-CFW-0.11.1.zip. **Gate changed again, so 0.11.0 as well as 0.9/0.10 devices need one keep-data stock return and reinstall.** Select Restore stored data in Bootstrap. The original resident bootloader remains unchanged; Product-only updates never overwrite Gate. The existing manual pairing-help route is retained.

Keep permanent module power connected during stock return/reinstallation. The previously accepted old-code stock-return power-loss limitation remains; this visual update does not fix it. IGN OFF is not removal of permanent power.

Confirm the actual CFW UPDATE screen after installation. Healthy B language resources are reused unchanged; the optional NewDoe-LANGUAGE-B-0.11.1.zip has the same content as 0.10.7/0.11.0. Product behavior is unchanged except its displayed version; the APK has the matching version/hash and migration wording. Parking changes from 0.11.0 remain, with no measured standby-current claim.

## Verification

The release includes captures generated from the compiled ARM display commands. They show the circular aperture; ROM-font glyphs are approximated and are not physical LCD photographs. Layout checks use larger preview glyphs, a 3px text-box pad and a 225px safe radius inside the 240px aperture. New Gate initialization, menu/idle/boot checks and APK/ZIP import checks run on software models. See verification.json and SHA256SUMS.txt.

---

# NewDoe 0.11.1 — 원형 화면에 맞춘 Gate

로고와 해골을160×160px에서 **240×240px**로 키웠어요. 처음에는 **Welcome!**, 검사할 때는 **Device Startup**을 표시해요. 해골 화면의 NewDoe 제목은 없앴습니다. 오류 번호·30초 카운트다운·버튼 조작·복구 판단은 그대로예요.

복구할 방법이 없으면 **Oh, shit. Sorry! / We're out of recovery options.**라고 표시해요. 도움말은 수리·교체와 GitHub QR 안내를 남기고 불필요한 칩 진단 설명을 뺐어요. 회색 비활성 항목은 유지하며 사유·하단 안내·진행 표시·확인창·도움말을 모두480px 원형 화면 안쪽으로 배치했어요.

## 설치

NewDoe-0.11.1.apk와 NewDoe-CFW-0.11.1.zip을 함께 사용하세요. **Gate가 다시 바뀌므로0.11.0 사용자도0.9/0.10 사용자와 마찬가지로 데이터 유지 순정 복귀 후 재설치가 한 번 필요해요.** Bootstrap에서는 보관 데이터 복원을 선택하세요. 순정 상주 부트로더와 수동 페어링 복구 절차는 유지하며, Product 업데이트로 Gate를 덮어쓰지 않아요.

순정 복귀·재설치 중에는 상시 전원을 유지하세요. 구버전 순정 복귀 기록 도중 전원 차단 시 자동 복구를 보장하지 못하는 기존 제한은 남아 있어요. IGN OFF는 상시 전원 제거와 달라요.

설치 후 실제 CFW UPDATE 화면을 확인하세요. 정상 B 언어팩은 그대로 재사용하며 NewDoe-LANGUAGE-B-0.11.1.zip 내용도0.10.7/0.11.0과 같아요. Product는 버전 표시 외 동작을 바꾸지 않았고, APK는 대응 버전·해시·전환 안내만 갱신했어요.0.11.0 절전 수정은 유지하지만 대기 전류는 실측하지 않았어요.

## 검증

캡처는 실제 빌드의 ARM 화면 명령을 원형으로 재현한 것으로 실기 사진이 아니에요. ROM 글꼴은 근사 표시합니다. 글자 크기를 여유 있게 잡고 글자 상자에3px를 더해, 반지름240px 화면보다 안쪽인225px 영역에 들어오는지 검사했어요. Gate 초기화·메뉴·절전·부팅과 APK/ZIP 대응도 소프트웨어로 확인합니다. 결과와 파일 해시는 verification.json·SHA256SUMS.txt에 있어요.
