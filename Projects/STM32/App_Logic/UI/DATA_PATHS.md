# UI 데이터 경로

차량 속도·ODO는 `UART5 → Vehicle_Service → NoodoeRuntime → ProductUI` 경로를
유지한다. 원격 데이터의 개발 모드를 켜도 이 경로를 대체하지 않는다.
개발용 속도는 PC에서 실제115200 8N1 프레임을 보내 넣는다.

## 앞으로의 BT 생산자

프로토콜 어댑터는 수신 패킷을 검증/디코딩한 뒤 `ProductUI_PhoneToken(slot)`으로
현재 연결 세대 토큰을 얻고 `ProductUI_PublishPhone/PublishMusic`에 전달한다.
두 실제 PhoneContent 슬롯만 갱신하며 개발 주입과 섞이지 않는다. OBD/GPS는
기존 Runtime의 읽기 전용 캐시를 사용한다. 이 인터페이스가 실제 무선 연결이나
companion 디코더 완성을 뜻하지는 않는다.

## 개발용 생산자

전역 설정은 `App_Logic/Config/inc/App_DataConfig.h`의 `DATA_DEBUG` 한 곳에서
정한다. 현재는1이다.1이면 부팅 직후부터 미리 정의한 표시용 데이터를 계속
사용하며 별도 SWD 주입이나 TTL 연장이 필요 없다.0이면 실제 공급자만 사용하고
개발용 주입 API/메일박스는 거부한다. 속도·ODO·IGN·RTC는 두 설정 모두 실제
UART/보드 경로를 유지한다. `DATA_DEBUG_START_CARD=3`은 디버그 빌드의 초기
음악 페이지 선택이며, 강제 화면 고정이 아니므로 버튼 탐색은 그대로 동작한다.

`Development_Data.c/h`가 별도 샘플을 검증하고 표시용 facts를 선택한다.
`ProductUI_InjectDevelopmentData(&sample, ttl_ms)`는 대기 없이 요청 접수만 한다.
DATA_DEBUG=1 및 GRAPHICS_DEV_VIEWPORT=1일 때만 임시 샘플을 허용하고,
1000..180000ms 뒤 전역 설정의 기본 플레이스홀더로 복귀한다.
`ProductUI_InjectDevelopmentData(NULL,0)`도 임시 샘플을 해제해 기본값으로 복귀한다.
이미 요청 처리 중이거나 입력이 잘못되면0을 반환한다. 그래픽 viewport 디버그를
끄더라도 DATA_DEBUG의 기본 소스 선택은 유지되며, 임시 주입만 거부한다.

- `DEVELOPMENT_REMOTE`: 표시용 폰 상태/알림/음악/RGB565 아트/폰GPS/OBD만 대체.
- `DEVELOPMENT_TRIPS`: 표시용 A/B/오늘/주유 후 기록만 대체.
- 기본 샘플은 DEVELOPMENT_REMOTE만 켠다. 트립은 UART 누계 실데이터를 표시하므로 A/B 리셋 결과가 즉시 보인다. 명시적 시험 샘플에서만 DEVELOPMENT_TRIPS를 추가할 수 있다. 실제 UART 트립 계산은 개발 표시 중에도 계속된다.
- Vehicle speed/ODO/IGN/RTC, 실제 링크 상태, 송신 커맨드, NOR/설정 필드는 없다.
- UI 태스크의 임시 `DevelopmentScratch`는 렌더링까지 유효하다. 별도 힙 할당이나
  영구 도메인 캐시를 추가하지 않는다. 실제 BT 캐시를 작업 공간으로 사용하지 않는다.
- 출처는 g_product_data.active_mask로 확인한다. 사용자 요청에 따라 TEST DATA 같은 진단 문구는 화면에서 표시하지 않는다. 원격 placeholder는 실물 BT 연결의 증거가 아니다.

디버거는 `g_product_data` version2/392bytes mailbox를 사용한다. command1=기본
샘플 임시 적용,2=sample 적용,3=전역 설정의 기본 소스로 복귀. `command/ttl_ms/sample`을 먼저 쓰고 request_id를
마지막에 쓴 뒤 ack_id/result를 확인한다. 이전 요청 ack 전에는 다음 필드를 쓰지
않는다. 공개 API와 디버거 주입을 동시에 사용하지 않는다. 유효하지 않은 요청은
result2로 거부하고 기본 소스로 복귀한다. active_mask로 현재 출처를 확인한다.
기본 플레이스홀더의 expires_ms는0이며 임시 override만 유한 TTL을 갖는다.
명령3의 의미가 바뀌었으므로 기존 version1 도구는 version 확인 후 중단해야 한다.

`g_page_preview` version2는 데이터 생산자가 아니다. card/selection/방향/시간
정지만 선택하며 old flags1은 거부한다. flags2는120ms에서 멈추고 flags4는UP을
선택한다. 실제 버튼은 이 화면 프리뷰만 취소하므로 개발 데이터를 둔 채 버튼
탐색이 가능하다. 임시 샘플은 TTL 또는 명시적 기본값 복귀로 해제한다.

## 물리 UART로 속도 주입

`tools/dashboard_uart.py --port COM11 --speed 73 --seconds 30 --execute`는
확인된 우노↔누도 전용 배선에10Hz로 프레임을 보낸다. --execute가 없으면 프레임만
출력한다. `--speed-file speed.json`을 주면 파일의 `{"speed":73}` 값을100ms마다
읽어 실시간 변경한다. 속도는0..255km/h, 기본ODO36475km다. 알려지지 않은 상태
바이트는 기존 캡처를 유지하고 XOR를 다시 계산한다. 종료 때0km/h를 보내고 닫는다.
프레임과 응답은 JSON에 저장한다. 이는 PC의 계기판 에뮬레이터이며 실제 주행값은 아니다.

## 연속 주행 시나리오

`tools/dashboard_drive.py --port COM11 --output-directory <새 로그 폴더> --execute`
는 종료 시각 없이150초 시나리오를 반복한다. 정차 → 0..50 가속 → 50 순항 →
50..90 가속/순항 → 90..160..200 고속 가속 → 200 유지 → 60으로 감속/유지 →
0으로 감속 → 신호대기 순서다. 115200 8N1/10Hz이며 기존 `dashboard_uart.frame`
인코더를 공유한다. 전송한 속도를 시간 적분해ODO를36,475km부터 정수km로
증가시키고, 반복 경계에서ODO를 초기화하지 않는다. PC가0.5초 넘게 멈춘 구간의
거리는 추산하지 않고 timing_gaps에 기록한다.

로그 폴더의 `stop` 파일을 생성하면 마지막0km/h 프레임을 보낸 뒤 포트를 닫는다.
`--seconds N`을 주면 유한 시간 시험도 가능하다. UART 오류 시 오류를 기록하고
종료하며, 임의 재연결이나 다른 COM 포트 탐색을 하지 않는다. `status.json`에는
PID/속도/ODO/단계/누적송수신량, `uart.json`에는최근6000개 TX/RX 기록을 유지한다.
무한 로그 증가를 막되 `tx_count`/`rx_bytes`/반복 횟수는 누적한다. PC 송신 완료는
누도 수신 성공을 뜻하지 않는다. `g_dash_service.rx_frames`, `published_vehicle`,
`g_product_ui`와 대조하고, 계기판 UART가 분리된 기존 벤치 배선에서만 사용한다.
