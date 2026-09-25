# 2026-09-01 AK550 실차 시험 분석

## 범위와 증거

- 대상: 한국향 Galaxy Tab Active3 `SM-T575N`, OpenNoodoe `0.3.1`, AK550 Noodoe
- 주 캡처: `captures/20260901-field-nav-gallery-theme-oqc/20260901-021421-794/protocol.log`
- 사용자 관찰: 내비게이션 화면 표시 성공, 갤러리 표시 실패, 테마 실패,
  OQC 실행 중 계기판 밝기 변화
- 순정 런타임 표본: `analysis/2026-09-01-official-app-runtime/`
- 판정 기준: 시퀀스 ACK, 명령 REPLY/status, 화면 관찰을 분리한다.

## 내비게이션과 파일 전송

내비게이션 이미지 협상과 파일 START는 모두 `status=0`이었다. 계기판은 9,371바이트
JPEG 데이터에 대해 `0x0D` REPLY를 보내 task 1, transfer 1, received 9,371,
total 9,371을 보고했다. 사용자가 내비게이션 화면 표시도 확인했으므로 이미지와
메타데이터 경로는 실차에서 동작한다.

그러나 `0.3.1`은 데이터 REPLY를 처리하기 전에 FILE FINISH를 보냈다.

```text
02:14:53.019 RX 0x0D reply
02:14:53.029 ACTION FILE FINISH
02:14:53.031 TX ACK for the 0x0D reply
02:14:53.101 FILE_CONTROL status=8 ERROR_INVALID_DATA
```

계기판 응답에 대한 ACK보다 종료 제어가 먼저 끼어든 순서 오류다. `0.3.2`는 각
`0x0D` 명령 REPLY와 그 수신 프레임 ACK가 끝날 때까지 기다리고, task/transfer ID와
누적 수신 길이를 확인한 후에만 FINISH를 보낸다.

## 갤러리

갤러리 BEGIN은 task 3 및 7-12에서 모두 `status=5 ERROR_INVALID_STATE`였다.
앞선 내비게이션 task가 FINISH 실패로 닫히지 않은 상태였으므로, 이 결과는 갤러리
JPEG 형식의 거부가 아니다. 갤러리 데이터는 아직 계기판에 전달되지도 않았다.

순정 앱 런타임에서 회수한 갤러리 JPEG는 다음 정적 구현과 일치한다.

- 480x480 JPEG
- JPEG EOI `FF D9` 뒤에 슬롯 바이트 하나를 추가
- 슬롯은 0-5
- 추가된 슬롯 바이트까지 포함한 MD5를 파일명으로 사용

회수 표본 `854e6947d702a8af204cc4577c6fcd5c.jpg`는 5,793바이트이며 마지막 바이트가
슬롯 0이고, 전체 파일 MD5가 파일명과 같다. 따라서 갤러리는 `0.3.2`에서 독립
세션으로 재검증해야 한다.

## 생성 콘텐츠와 테마

실차 실패 요청은 모두 위치 `0x000 DEFAULT_DASHBOARD`였고 BEGIN 단계에서
`status=8 ERROR_INVALID_DATA`를 받았다. 순정 앱에서 기본 대시보드는 내장 항목이며
생성 번들 설치 대상이 아니다. 실제 생성 콘텐츠 위치는 시계 `0x200`, 날씨 `0x300`,
속도계 `0x400`, POI `0x500`, 그룹 `0x700`, 음악 `0xA00`, 오디오 `0xB00`이다.
OpenNoodoe `0.3.2`는 잘못된 기본 대시보드 선택지를 제거하고 서비스에서도 막는다.

루트 권한으로 순정 앱의 실제 설치 번들을 보존했다.

| 디렉터리 | 종류 | cfg | 크기 | MD5/파일명 일치 |
| --- | --- | --- | ---: | --- |
| `installed/2.bundle` | 시계 | `6e38e4321acb5e37f09167eb66af8558.cfg` | 2,190 | 예 |
| `installed/4.bundle` | 속도계 | `70a3e3c9975f606d45a9c4d039af3194.cfg` | 2,008 | 예 |
| `installed/6.bundle` | 그룹 | `d7f12a5b64d83a98419dc14bf258f4e4.cfg` | 237 | 예 |

시계 cfg에는 `BackgroundWidget`, `ClockDigitWidget`, `DateWidget`이, 속도계 cfg에는
`BackgroundWidget`, `SpeedBarWidget`, `SpeedDigitWidget`, `OdometerWidget`이 있다.
OpenNoodoe의 최소 `BackgroundWidget` cfg는 이 스키마의 유효한 부분집합이지만,
올바른 위치에서의 실차 수락 여부는 아직 검증되지 않았다.

## OQC의 실제 의미

`OQC_TEST START/STOP`은 둘 다 `status=0`이었다. `0xC4` 5바이트 NOTIFY는 밝기
명령이 아니라 공장 검사 입력과 센서 상태다.

| 비트 | 공식 필드 | 실차 관찰 |
| ---: | --- | --- |
| `0x01` | Power ON pressed | 계속 감지 |
| `0x02` | Power OFF pressed | 미감지 |
| `0x04` | UP pressed | `0x25`에서 감지 |
| `0x08` | ENTER pressed | `0x29`에서 감지 |
| `0x10` | DOWN pressed | `0x31`에서 감지 |
| `0x20` | MFi chip enabled | 계속 감지 |

조도 센서 값은 2에서 1로 변했다. Noodoe Tools의 Buttons 화면은 UP/ENTER/DOWN을
누적 체크하고, LightSensor 화면은 수치를 표시하며, MFI 화면은 bit 5를 합격 여부로
사용한다. Backlight 화면도 같은 OQC 모드를 열고 사용자가 합격/실패를 기록한다.
따라서 관찰된 밝기 변화는 OQC 모드의 백라이트 검사 동작이고, 동시에 버튼·조도·MFi
데이터도 정상 수신된 것이다. `0.3.2`는 이 값을 이름과 누적 체크 상태로 표시한다.

## 다음 실차 시험

1. 계기판을 재시작하거나 SPP를 완전히 끊어 이전 task 상태를 초기화한다.
2. OpenNoodoe `0.3.2`에서 새 캡처 세션을 만든다.
3. 내비 이미지 전송을 한 번만 실행한다. `0x0D status=0`, 수신 길이 일치,
   수신 프레임 ACK, FILE FINISH `status=0`, NAV DONE `status=0` 순서를 확인한다.
4. 새 캡처 세션에서 갤러리 슬롯 1 한 장만 전송한다. BEGIN/START/DATA/FINISH/DONE이
   모두 `status=0`인지와 실제 사진 표시를 각각 기록한다.
5. 새 캡처 세션에서 생성 콘텐츠의 첫 항목인 시계 `0x200`을 한 번 전송한다.
   BEGIN이 수락되면 모든 파일 및 DONE까지 확인하고, 화면 전환 후 배경을 확인한다.
6. OQC START 후 UP, ENTER, DOWN을 한 번씩 누르고 조도센서를 가렸다가 연다.
   앱의 누적 입력과 조도 범위를 확인한 뒤 STOP한다.

갤러리와 시계 전송은 같은 캡처에서 연속 실행하지 않는다. 하나가 실패하면 연결을
초기화한 뒤 다음 기능으로 넘어가야 후속 `INVALID_STATE`를 원인으로 오판하지 않는다.
