# NewDoe 0.11.17 beta

UART 수신 중 숫자가 사라지는 처리와 키 OFF 정차 사용 중 ODO 유지를 수정했습니다.

- 잠깐의 수신 오류에서는 직전 정상값을 원래의 유효시간까지 유지합니다. 오류가 반복되어도 유효시간을 연장하지 않으며, 키 ON에서 1.5초 넘게 정상 차량 데이터가 없으면 알 수 없음으로 표시합니다.
- 키 OFF 후 10분 사용 모드에서도 마지막 ODO를 표시합니다. RAM에만 보관하고, 재부팅하면 새 계기판 값을 기다립니다. 트립·정비 기록과 계산은 유지합니다.
- UART 오류가 붙은 바이트는 버리고 수신을 복구합니다. CMD22의 정상 공통 데이터가 있으면 선택적인 추가 필드가 없어도 속도·ODO를 읽습니다.
- 조사한 F4 순정 이미지 네 종류는 모두 UART5 115200bps / 8N1을 사용합니다. **2021 Xciting 400의 완전한 호환성은 아직 확인되지 않았습니다.** 해당 차량의 원본 이미지나 UART 기록이 없어 연식별 차이를 확정할 수 없습니다.

## 설치 / Installation

**0.11.16 → 0.11.17은 일반 CFW 업데이트입니다. 순정 복귀나 언어팩 재설치는 필요하지 않습니다.**

1. `NewDoe-0.11.17.apk`를 기존 앱 위에 설치합니다.
2. 정차한 상태에서 기기 페이지에 본체와 `NewDoe-CFW-0.11.17.zip`을 선택합니다.
3. CFW 업데이트를 누르고 안전 안내를 확인합니다. 배터리를 분리하거나 스타터를 누르지 마세요. 설치 중 키 OFF에서 UP과 O를 함께 누르지 마세요.
4. 전송·설치·재부팅 후 누도의 `CFW UPDATE` 표시를 확인하고 앱에서 완료 확인을 누릅니다.
5. 기존 B 0.11.2 언어 자원은 유지합니다. 정상 설치돼 있으면 다시 보낼 필요가 없습니다.

0.11.16보다 오래된 Gate에서는 앱이 **순정 복귀 → CFW 데이터 보관 → 재설치 → 보관 데이터 복원** 경로를 안내합니다. 이전 코드가 실행하는 순정 복귀 구간에서는 전원을 유지해야 합니다. 그 구간의 전원 차단 후 자동 복구는 보장하지 않습니다.

[한국어 그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md) · [English illustrated guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)

## English

This update preserves a recently accepted UART reading across a brief receive error without extending its original 1.5-second freshness deadline. It discards error-marked UART bytes before rearming reception and accepts the complete common telemetry prefix of CMD22 independently of its optional extension.

The last received ODO now remains visible during the ten-minute key-OFF mode. This is RAM-only display retention; trips and maintenance counters remain separate, and a reboot requires a new cluster reading.

**From 0.11.16, install the matching APK and ZIP with the normal CFW update. No stock return or language-pack reinstall is required.** Gate is unchanged. Older Gate installations follow the app’s keep-data stock-return/reinstall guidance. Keep power connected during the legacy stock-return interval; automatic recovery from a power cut there is not guaranteed.

All four inspected F4 stock images initialize UART5 at 115200bps, 8N1. This does not establish complete compatibility with the reported 2021 Xciting 400; its firmware dump and live UART data are unavailable.
