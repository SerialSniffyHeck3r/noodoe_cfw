# 속도 링 보간 및 트립 레이아웃,2026-09-14

사용자 요청의5개 항목을 구현하고 Release APP를 실기기에 설치했다.
COM11의115200 8N1 속도1→200 반복 송신은 그대로 유지한다.
송신 PID와 중지 파일은 ../2026-09-13-xmb-trip/uart-ramp-234632/launch.json에 있다.
중지할 때만 해당 디렉터리에 STOP 파일을 만들면 마지막0km/h 프레임을 보내고 종료한다.

## 변경

- 기존150ms 속도 모델 보간을 유지하면서 LVGL/EVE 정수 각도 변환을 없앴다.
  Graphics_SubpixelArc는 태그한 arc task만 평가·디스패치해0.01도 입력을
  EVE1/16px 정점의 둥근 line strip으로 렌더링한다. 실제 속도/주행 계산과
  속도에 따른 조작 제한은 원시 UART값을 사용한다. 다른 LVGL arc 및 vendor
  소스는 변경하지 않았다.
- Partial / telemetry gap / Waiting 등 트립 상태 문구를 그리지 않는다.
  내부 validity/gap 플래그는 그대로다. 중앙 viewport는Y384까지9px 확장했고
  모든 모서리도 링 안에 들어오는지 검사했다. 시계/링/거리 footer/구분선은 유지했다.
- 주행시간 D-DIN40, 거리·최고·평균속도 D-DIN36, Lato 글자·단위20을 사용한다.
  실측 잉크 하단은 시간/Moving/Stopped Y215, Dist./거리/단위 Y313,
  최고·평균속도/단위 Y376이다.112처럼 둥근 숫자가 없는 조합의1px 차이도
  고려해 실제 잉크 하단을 고정한다. 숫자 폭/열 위치와 leading-zero 정책은 유지한다.
- 실제 캡처 중 UART가 ORE 뒤 반복 재시작에 갇히는 기존 문제를 확인했다.
  HAL_F4 AbortReceive는 SR/DR 오류를 지우지 않아, RX 초기화·복구에
  SR→DR 플래그 제거를 추가했다. TX DMA 소유권·실패 latch는 보존했다.

## 검증

- Release405876bytes, 여유52876bytes, SHA256
  `9f63b908b4540debd96e685513115826d3566fd9a5e846457dc9adca3ac99b3c`.
- Debug458332bytes, 여유420bytes. 빌드 오류0, 기존 RWX LOAD 경고1.
  Debug 여유가 작으므로 이후 기능 추가 시 실제 빌드 용량을 계속 확인해야 한다.
- install-final/summary.json: APP 전체 readback 일치, 두 reset chain의
  HAL/RTOS/heartbeat 정상 진행. 순정 하위64KiB SHA256 전후 동일:
  `f38ed839c4fbce74d4d8227fcf13764194f405c4860cda75c628fce66064b574`.
- 실제 ARM O0/Os 모델: 속도2434개 assertion씩, UI 모델110605개씩 통과.
  새 정점 경로는 각 설정에서1만 입력,9999개 고유 endpoint를 검증했다.
  삼각함수 기준 endpoint 최대오차0.1392px. 이는 소프트웨어 정점 검사이며
  패널 광학 성능의 측정은 아니다.
- 실제 ARM UART 복구 테스트88개씩 통과. SR→DR 읽기의 레지스터 부작용과
  기존 IRQ 재진입·DMA abort 실패 보존을 포함한다.
- Cube 설정 복원 fixture 통과: 상대경로·프로필·링커 복원 멱등성,
  Core/IOC byte 보존. 이번에 GUI 재생성을 새로 수행한 것은 아니다.
- trip-final/capture/display.png: 실제 EVE RGB565 캡처. 테스트 트립값과
  실제 UART 속도를 조합했다. capture-check.json에서10개 텍스트 영역의
  아랫선을 픽셀 단위로 대조했다. 단어 경계의 희미한 AA 픽셀도 포함한다.
- 해당 캡처 진단29.9FPS, CPU 비유휴율70.3%, frame slot 누락0,
  렌더링 경고/오류/SPI 실패0. UART speed/ODO 유효, ODO36475km.
  초기화·디버그 중 UART 오류 이력47회와 복구47회는 남아 있다.
  오류가 한 번도 없었다고 주장하지 않으며 마지막 HAL 오류는0이었다.

## 보존 및 한계

캡처용 데이터와 프리뷰는 검증 후 해제한다(live-final 기록).
속도 송신은 사용자가 계속 유지하라고 요청했으므로 종료하지 않는다.
캡처는 EVE framebuffer이며 LCD 유리/백라이트를 촬영한 사진은 아니다.
BT/광센서/MFi/NOR 포맷/OTA/RTC 설정은 이번 변경 범위에 포함하지 않는다.
첫 후보에서 태그 task가 upstream evaluator에 거부돼 링이 빠지는 것을
실측했으며 최종 코드에서 평가 단계를 등록하고 경고0/링 표시를 재검증했다.
trip-candidate는 실패 단계의 증거이고 최종 결과는 trip-final이다.

현재 소스 snapshot은 sources/, 해시는 sources.json,
설치한 두 구성의 manifest는 release-manifest.json/debug-manifest.json에 있다.
