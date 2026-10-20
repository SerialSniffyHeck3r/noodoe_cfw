# NewDoe 0.11.18 beta

주유 감지를 개선하고 Android 앱의 불필요한 반복 작업을 줄였습니다.

- SINCE REFUEL은 이전보다 두 칸 이상 높은 연료 상태가 3초 동안 유지되면 새 주유로 등록합니다. 예를 들어 2칸에서 주유한 뒤 4·5칸 사이를 오가도 감지합니다. 주유 전 기준 연료량의 저장도 요청합니다.
- 연료 감지는 화면 그리기와 별개로 처리하고, 부팅 중 이전 기록을 읽은 뒤 시작합니다. TRIP A/B·오늘·정비 기록은 유지합니다.
- 같은 위치·축척의 지도 준비 계산을 재사용합니다. 좌표나 축척이 바뀌면 바로 다시 계산하며 지도 미리 가져오기와 전송은 유지합니다.
- 변화 없는 앱 지도를 반복해서 다시 그리지 않습니다. 이미지 준비 대기는 완료 알림으로 깨우고, 기기 이미지 검증 상태를 지나치게 자주 묻지 않습니다.
- 음악 세션·휴대폰 알림은 변경 알림을 사용하고, 전체 상태를 다시 읽는 보조 조회를 줄였습니다. GPS 수신 주기, 화면이 꺼진 상태의 주행 연동과 실제 이미지 전송 속도는 유지합니다.

## 설치 / Installation

**0.11.16 또는 0.11.17에서는 일반 CFW 업데이트로 올리세요. 순정 복귀나 언어팩 재설치는 필요하지 않습니다.**

1. `NewDoe-0.11.18.apk`를 기존 앱 위에 설치합니다.
2. 정차한 상태에서 기기 페이지에 본체와 `NewDoe-CFW-0.11.18.zip`을 선택합니다.
3. CFW 업데이트를 누르고 안전 안내를 확인합니다. 설치 중 배터리 분리·스타터 조작·키 OFF에서 UP+O 동시 누르기를 하지 마세요.
4. 전송·설치·재부팅 후 누도의 `CFW UPDATE` 화면을 확인하고 앱에서 완료 확인을 누릅니다.

Gate·Bootstrap과 B 0.11.2 언어 자원은 그대로입니다. 0.11.16보다 오래된 Gate에서는 앱의 **순정 복귀 → CFW 데이터 보관 → 재설치 → 보관 데이터 복원** 안내를 따릅니다. 이전 코드가 수행하는 순정 복귀 구간에서는 전원을 유지해야 하며, 그 구간의 전원 차단 후 자동 복구는 보장하지 않습니다.

[한국어 그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md) · [English illustrated guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)

## English

SINCE REFUEL now accepts a sustained rise of at least two bars, including a 2-bar tank refilled to alternating 4/5-bar readings. It requests a checkpoint when the learned baseline changes, waits for saved history at boot, and samples fuel independently of display composition. Trip and maintenance history is preserved.

Android reuses the map plan for an unchanged fix/scale, avoids unchanged-map redraws, and reduces polling while artwork is being prepared or verified. Media and notification callbacks stay immediate, with less frequent fallback reconciliation. Live GPS cadence, background riding, map prefetch and DATA transfer throughput are retained.

**From 0.11.16 or 0.11.17, use the matching APK and ZIP with the normal CFW update. No stock return or language reinstall is required.** Gate, Bootstrap and the B 0.11.2 resources are unchanged. Older Gate installations follow the app's keep-data stock-return/reinstall route; keep power connected through that legacy interval.

## 확인 범위 / Verification scope

보고된 삼성의 30분당 10% 소모는 같은 조건의 실기 측정이 없어 절감률을 확정할 수 없습니다. 2021 Xciting의 주유 전후 UART 원본도 아직 없어 차종별 연료 필드 차이까지 확인한 것은 아닙니다. 이번 수정은 재현된 주유 판정·저장 시점 문제와 불필요한 작업 반복을 대상으로 합니다.

The reported Samsung battery percentage has not been measured under matching conditions, so no battery-saving percentage is promised. Raw pre/post-refill UART data from the 2021 Xciting is still unavailable; a model-specific fuel encoding difference is not established.
