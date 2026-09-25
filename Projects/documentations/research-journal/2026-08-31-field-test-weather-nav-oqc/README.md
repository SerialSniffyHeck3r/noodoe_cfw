# OpenNoodoe 0.3.0 실차 시험 분석

시험 캡처: `20260831-055442-628`

회수 자료:

- `captures/20260831-field-weather-nav-oqc/20260831-055442-628/protocol.log`
- `captures/20260831-field-weather-nav-oqc/opennoodoe-full.log`
- `captures/20260831-field-weather-nav-oqc/logcat.txt`
- `captures/20260831-field-weather-nav-oqc/meminfo.txt`

## 판정

| 기능 | 프로토콜 결과 | 실차 관찰 | 판정 |
| --- | --- | --- | --- |
| 날씨 `0x09` | 두 번 모두 외부 sequence ACK, 명령 REPLY 없음 | 날씨 테마 미설치로 화면 변화 없음 | 전송 경로만 확인, 기능 성공 아님 |
| 내비 메타데이터 `0x08` | 두 번 모두 외부 sequence ACK | 화면 변화 없음 | 프레임 수신만 확인 |
| 내비 이미지 협상 `0x0A` | task 1, 3, 4 모두 status 6 | 이미지 없음 | 앱의 payload 길이 오류 |
| 생성 대시보드 협상 `0x0A` | task 2, 5 모두 status 6 | 변화 없음 | 같은 payload 길이 오류 |
| OQC read `0x11` | status 0, 139바이트 | 전체 생산정보 수신 | 성공 |
| OQC START/STOP `0x12` | START 2회, STOP 2회 모두 status 0 | 약 1초 주기 밝기 순환 | 성공, 공장 검사 동작 |

## 파일 협상 오류

계기판의 status `6`은 순정 enum의 `ERROR_INVALID_LENGTH`이다. 0.3.0은
협상 payload를 다음과 같이 26바이트로 만들었다.

```text
u16 task_id
u8  transfer_type       # 오류
u16 location
u32 total_size
u8  attribute
16 bytes content_id
```

순정 `OutputCommandProcessor`는 `transfer_type`에도 `GattUtils` format 34,
즉 `u16le`를 사용한다. 올바른 협상 payload는 27바이트다. 파일 control의
operation 역시 `u16le`이므로 0.3.1에서 두 필드를 함께 수정했다.

## OQC와 렉

첫 START 뒤 26.3초 동안 `C4` 이벤트 233개가 수신되었다. 대략 초당 9개다.
0.3.0은 각 샘플마다 원시 RX, command, OQC 상태를 파일에 쓰고 각 로그마다
TextView 전체 문자열을 다시 만들었으며 Snapshot도 갱신했다. ANR이나 crash는
없지만 이 구현은 고빈도 OQC 스트림에서 명확히 UI jank를 만든다.

0.3.1 변경:

- 원시 TX/RX는 캡처 파일에만 기록
- 화면 로그는 250 ms 단위로 배치
- 동일 OQC 샘플은 최대 초당 1회만 UI/Snapshot 갱신
- OQC 자동 STOP을 5분에서 30초로 단축

## 다음 시험

1. 0.3.1에서 내비 이미지 하나만 전송하고 `0x0A status=0`을 확인한다.
2. 성공한 경우에만 `0x0B START`, `0x0D` 데이터, `0x0B TERMINATE`, DONE을 추적한다.
3. 날씨는 순정 날씨 creation이 설치된 상태에서 같은 고정값으로 다시 시험한다.
4. OQC는 START 한 번만 누르고 30초 자동 STOP과 UI 응답성을 확인한다.
