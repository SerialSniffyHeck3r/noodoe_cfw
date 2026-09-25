# 계기판 하위 상태와 배치 변경

2026-09-13 최종 결과. 사용자 정정에 따라 **ODO/footer는 호 끝 중심선 아래쪽,
원형 내부 Y404..467**이다. install-01/verify-01의 위쪽 배치는 폐기된 시안이다.

- 상위 UiState 아래 UiDashboardState를 둔다. 중앙 모드·footer·카드 내부
  selection을 분리하고 부모 메뉴/modal/IGN/경고가 우선한다.
- ENTER 짧게 중앙 카드 순회, Trip computer에서 UP/DOWN 하단 선택.
  원격/알림/음악은 오프라인 상태를 표시한다. TRIP F는 기존 RESV 강제 모드다.
- 시계48px/baseline80, 3개 선분의 열린 사다리꼴 separator.
- footer 제목20px/x142/baseline424, 숫자32px/오른쪽 끝x271/baseline459,
  단위20px/x282/baseline459. 고정 폭 숫자와 고정 단위 위치를 유지한다.
- 개발용 외곽0x4A5952. 원형 입력/표시 영역은 동일하고 외곽 content는 없다.
- ProductUI_PublishDistances는 nonblocking snapshot API이며 거리 공급/계산/
  저장 서비스는 후속이다. 현재 UART ODO만 실측 값, 다른 거리는 명시적 미확인.

## 검증

- 실제 ARM C O0/Os 각각 상태90,203 + 속도416 assertions 통과.
- 폰트·모든 고정 label 영역/문구·충돌 검사 통과. Debug/Release ELF의
  좌표 테이블을 직접 해독하여 현재 source와 동일함도 확인했다.
- EVE 명령 스트림/상태 복원 O0/Os 각각223 assertions 통과.
- sync2회: Core/IOC/CDT37개 파일 byte 동일. 이번에 GUI 재생성을 수행한 것은 아니다.
- Debug APP425,384bytes, 여유33,368bytes. Release APP367,264bytes,
  여유91,488bytes. 실제 CubeIDE 빌드 성공.
- install-02에서 programmer 내부 검증이0x08016DCD에서1byte 불일치를 보고했다.
  verify-failure-inspection의100/100/50kHz 독립 구간 읽기는 모두 기대값과 일치했다.
  recovery-02에서 APP 전체/하위64KiB 비교와 순정BL 경유2회 재부팅을 통과했다.
  다시 쓰지 않았으며 verify-02의 추가 APP 전체 hash도 일치했다.
  실패 시 남았던 DBGMCU_APB1_FZ는 최초 값0으로 복원하고 읽어 확인했다.
- COM11 벤치 UART0/73/170/230/120km/h에서 링 비율/ODO36475km 일치.
  FPS **29.8~30.6**, CPU **53.6~54.7%**.
  중단 시 회색 stale/각도 고정, 추가 UART 오류 없음, GPU SPI 실패 없음.
  COM11 시험 송신은 종료했다.
- EVE CMD_SNAPSHOT2 실제480×480 raster/CRC/세대 검증. 물리 패널 사진이 아니다.
  반경242 밖의 모든 pixel이 동일 녹회색인 것을 검사했다(AA 경계 제외).

![최종 EVE 캡처](verify-02/capture/display.png)

UI 메뉴 전이는 ARM 모델 시험으로 검증했다. 이번 턴에 사용자가 물리 버튼을
눌러 모든 카드를 확인한 것으로 보고하지 않는다. BT·정비 거리 계산·경고 도메인
연결·실제 절전 완료를 이 UI 변경만으로 주장하지 않는다.
