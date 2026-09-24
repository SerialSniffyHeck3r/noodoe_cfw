# 6. 퀵 세팅과 모든 설정 항목

[목차](README.md)

| 퀵 세팅 | 전체 설정 | 이동 감지 |
|---|---|---|
| ![퀵 세팅](images/quick-settings.png) | ![전체 설정](images/settings-root.png) | ![이동 잠금](images/settings-moving.png) |

퀵 세팅에서 UP/DOWN으로 밝기를 빠르게 조절합니다. 수동이면 밝기 %, 자동이면 −2~+2 보정값입니다. O 길게 누르면 전체 설정에 들어갑니다. **유효한 UART 속도가 3km/h 이하로 5초 유지**되어야 합니다.

전체 설정에서 UP/DOWN은 항목 이동, O는 선택입니다. 숫자/날짜/시간은 편집할 칸을 선택하고 값을 바꾼 뒤 `Apply`로 적용합니다. `Back`은 이전 단계입니다. 선택만 하거나 편집 중인 값을 적용하지 않고 나가면 확정하지 않습니다.

차량 이동 또는 속도 미확인으로 설정이 잠기면 화면을 회색 처리하고 30초 카운트가 진행됩니다. 다시 **3km/h 미만으로 3초 유지**하면 재개할 수 있습니다. 제한 시간이 끝나면 주행 화면으로 돌아가며 미확정 값은 버립니다. 반복 이동으로 제한 시간이 계속 연장되지는 않습니다.

## Display

| 표시 이름 | 조절 범위·의미 |
|---|---|
| Brightness mode | Manual / Auto. Auto에는 정상 조도 센서와 공장 보정값 필요 |
| Brightness | 5~100%, 5% 간격. 수동 백라이트 밝기 |
| Auto offset | −2~+2. 본체 자동 밝기 보정 |
| Dashboard light offset | **−5~+5**, 기본 0. 누도→차량 계기판 UART 조도 단계에 더하는 값 |
| Wallpaper | 사진 배경 켜기/끄기 |
| Photo | 일반 배경 슬롯 0/1/2 |
| Key OFF photo | Same as riding / Photo 0 / Photo 1 / Photo 2 |
| Center brightness | 가운데 배경 밝기 0~100%, 5 간격. 백라이트 PWM과 별개 |
| Theme | Dark / Light / Auto. 글자·구분선·아이콘·음영 색을 함께 변경 |
| Auto theme source | 시간 또는 조도 기준 선택 |
| Light starts / Dark starts | 시간 기준 테마 전환 시각, 15분 간격 |
| Dark threshold / Light threshold | 조도 기준 전환 임계값. 두 경계를 두어 반복 전환을 줄임 |
| Ambient sensor | 실제 센서 상태/lux 또는 오류, 읽기 전용 |
| Retry ambient sensor | 센서를 다시 확인하는 동작 |

`Dashboard light offset`은 송신 단계 0~9 범위에서 제한됩니다. 누도 자체 화면의 자동 밝기 보정과는 별개입니다. +값이 특정 차량에서 광학적으로 몇 % 밝아진다는 뜻은 아닙니다. 공장 보정 영역은 읽기만 합니다. 센서 오류 시 자동 밝기가 정상 동작한다고 표시하지 않고 수동 밝기로 대체합니다.

## Date & time

| 항목 | 의미 |
|---|---|
| Date | 2000~2099년의 실제 존재하는 날짜 |
| Time | 현지 시각, 시/분. 날짜와 요일은 달력 규칙에 따라 처리 |
| UTC offset | UTC−12:00~UTC+14:00, 15분 간격 |

화면은 현지 시각이고 내부 RTC는 UTC를 사용합니다. 날짜를 되돌리면 날짜 기준 정비 경과와 TODAY 분류에도 영향을 줍니다. 잘못된 날짜나 범위를 벗어난 변환은 거부됩니다.

## Vehicle

| 항목 | 의미 |
|---|---|
| Distance units | km / mi. 속도 단위도 km/h / mph로 전환 |
| Ring maximum | 20~400km/h, 10 간격의 속도 링 기준 |
| Stop threshold | 0~10km/h, 기본 5. 트립 이동/정차 분류 |
| Vehicle | 계기판에서 받은 차량 정보, 읽기 전용 |

## Maintenance → Oil / Belt / Service

각 정비에 아래 항목이 있습니다.

| 항목 | 범위·의미 |
|---|---|
| Distance interval | 0~100,000km, 100 간격 |
| Key ON hours | 0~10,000시간 |
| Day interval | 0~3,650일 |
| Service completed | 현재 ODO·키 ON 누계·날짜를 이 정비의 새 기준점으로 확정 |

주기 0은 그 기준을 사용하지 않는다는 뜻입니다. 활성 기준 중 하나가 도래하면 정비가 필요합니다. 엔진을 실제로 돌렸는지는 측정하지 않으므로 Key ON hours를 엔진 운전 시간이라고 해석하지 마세요. 기준값이 없거나 시간을 되돌려 계산할 수 없는 항목은 미확인으로 취급합니다.

## Connections

| 항목 | 동작 |
|---|---|
| Phone / Bluetooth | 실제 연결/드라이버 상태 읽기 |
| Pair phone | 페어링 창 열기 |
| Close pairing | 페어링 창 닫기 |
| Disconnect phone | 현재 폰 연결 종료 |
| Re-pair phone | 확인 후 저장된 폰 인증정보 삭제·저장·재시작·페어링 |
| Restart Bluetooth | Bluetooth 서비스를 재시작 |

`Re-pair phone`을 사용했다면 Android 쪽의 이전 Noodoe 등록도 지우고 다시 페어링해야 합니다. 단순 연결 지연마다 양쪽 기록을 지우는 것은 권장 동작이 아닙니다. 연결 재시작과 페어링 정보 삭제는 서로 다른 작업입니다.

## System / Power sequence

Firmware와 Language는 본체 버전/고정 UI 언어를 표시하는 읽기 항목입니다. 폰 앱의 언어 선택과 혼동하지 마세요. `Reset display`는 화면 환경설정을 기본값으로 되돌리는 확인 동작입니다.

Power sequence에는 **Screen hold, Screen duration, BT only, BT-only duration, All off (STOP), Wake condition**이 있습니다. 두 유지 시간은 각각 0~1440분입니다. 각 단계와 마지막 단계 유지 조건은 [절전 설명](07-Power-Warnings.md)을 참고하세요.

## Debug

읽기 전용 상태로 Bluetooth/HCI·연결, 조도 센서, 보정/송신 밝기, 입력 스위치, UART 및 관련 런타임 상태를 확인합니다. 값이 보인다는 이유만으로 실제 무선 연결·배선·광학 밝기가 정상인 것은 아닙니다. 심층 진단은 별도 Diagnostic 펌웨어 경로입니다.

## 폰에서 모든 기기 설정 변경하기

주행 연동으로 연결한 뒤 꾸미기/기기 설정을 엽니다. 앱은 기기에서 실제 설정 목록을 읽고 숫자·선택·날짜·시간·동작에 맞는 입력을 제공합니다. 라이더 이름, ODO 보호 선택도 여기서 다룹니다.

**접수 → 적용 → 영구 저장**은 다른 상태입니다. 저장 완료를 확인하기 전에는 전원을 끄지 않는 편이 좋습니다. 설정은 기기가 기준이며 재연결했다고 폰의 예전 값으로 덮어쓰지 않습니다. 움직이거나 IGN OFF/설치 중이면 변경을 거부할 수 있습니다. 연결이 끊긴 오래된 창의 동작은 새 기기에 적용되지 않습니다.

## 사진 3슬롯

앱에서 슬롯을 고르고 사진을 전송한 뒤 저장 완료를 기다립니다. 본체 지원 형식은 480×480 이하, 최대 128KiB JPEG이며 앱이 맞춰 준비합니다. 일반 사진은 CFW 사진 저장소에 유지되고 음악 앨범아트는 별도 RAM 표시입니다. 빈 슬롯은 지정해도 그림을 만들어내지 않습니다. 순정 앨범 파일을 직접 편집하지 않습니다.
