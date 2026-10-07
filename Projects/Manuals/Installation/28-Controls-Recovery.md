# FuckNudo 0.9.61 — gray status pulses and manual ODO checks

- **GPS/Bluetooth:** 비활성 구간을 사라짐·흰색 대신 회색으로 표시합니다.
  GPS는120ms 회색 펄스, Bluetooth는 저전량 빨강/회색 및 충전 중 초록/회색
  교대입니다. 배터리 경계값과 점멸 주기는 그대로입니다.
- **설치 정지 안내:** 기존 Paused. See phone help. 조건에서 아래 문구를
  표시하고 Stage·Code/phase를 유지합니다.

```text
Please manually reset.
Hold UP + O together
for 3 seconds.
```

- **ODO 확인:** 자동 팝업을 없앴습니다. **SETTINGS → Vehicle → ODO check**
  또는 폰의 기기 설정 ODO 항목에서 확인합니다. 두 값 적용에는 IGN ON과
  유효한 정차 속도5초 확인이 필요합니다. Back 또는 O 길게는 적용이 막혀
  있어도 변경 없이 돌아갑니다. 적용 후 O로 설정에 복귀합니다.
- **입력 수정:** 공통 입력 처리에서 승인한 짧은 누름을 ODO 창이 다시
  시간 기준으로 버리던 처리를 제거했습니다. 이전에는 약3초 뒤 팝업이 열려도
 5초 적용 조건이 충족되지 않아 Ask me later만 동작할 수 있었습니다.
  이 상태를 시험으로 재현했고, 준비 완료 후 두 선택의 적용을 검증했습니다.
- **설명서:** 한국어·영어 설명서와 앱의6개 언어 도움말에 새 조작을 반영했습니다.

주행 연동을 멈추고 **0.9.61 APK와 ZIP을 함께** 업데이트하세요. 일반 Product
업데이트는 기존 설정·사진·주행 기록을 보존합니다. Gate를 다시 설치할 필요는
없으며 Product 이외의 모든 이미지가0.9.60과 같습니다.

새 오류 문구는 수동 재시작 안내이며 모든 설치 오류의 원인 해결이나 설치 성공
판정은 아닙니다. 재시작 후 버튼을 놓고 같은 기기·작업으로 재연결하세요.
구버전이 첫 재시작을 수행하는 동안에는 구버전 문구가 계속 표시됩니다.

[한국어 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/User/23-Controls-Recovery.md)
· [English manual](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/User/23-Controls-Recovery.en.md)

Android554개 시험, ARM ODO·아이콘·입력 시험과 설치/ODO 화면9개를 검증했습니다.
Debug 여유1,388B, Release33,456B입니다. **실기 시험은 하지 않았습니다.**
[검증 보고서](../../documentations/controls-recovery-0.9.61.md)를 참고하세요.

## English

GPS now pulses gray for120ms; Bluetooth battery animation alternates with gray
instead of white. Existing thresholds and timing remain. The installation error
screen gives the three-line UP+O3s instruction above and retains Stage/Code/phase.
This is manual recovery guidance, not proof that an underlying failure is fixed.

ODO checks open only from Settings > Vehicle > ODO check or phone Device settings.
The old prompt could open at3s before the5s stationary apply gate. Back or long O
now always returns without modifying a value; both value choices work when ready.
Redundant rejection of an already normalized short gesture is removed. Use the
matching0.9.61 APK and ZIP.554 Android tests and ARM/software-rendered frames pass;
physical vehicle behavior remains untested.
