# Summary 직후 절전 / BT 연결 유지

## 설치 결과

- `candidate.elf`의 Release APP을 `0x08010000`에 설치했다. APP 372,328 bytes 전체 읽기 대조, 순정 BL 하위64KiB 전후 동일 확인. NOR/옵션 바이트는 쓰지 않았다.
- APP SHA256: `651f29a854308537bb0ea421c1f6525c8f7bf75d2d1d3069531b2e4e3ed3bb44`
- 복원 기준: `../2026-09-18-power-wake/install-final/app.bin` 및 `install/restore-app.bin`.
- Release flash 여유86,424B, Debug70,988B, 일반 SRAM35,744B, CCM16,320B. 힙/스택/큐 크기 유지. 생성 Core/IOC/vendor 변경 없음. Debug 링크의 기존 RWX LOAD warning은 남아 있다.

## 원인과 변경

이전 AWAKE는25% 백라이트를 유지하며 배경 로딩/전환을 지속했고, DISPLAY_SLEEP에서도 일반 WFI만 사용하여1ms 틱마다 CPU가 깨어났다. 또한 이후의 BT 유지 시간이 만료되면 controller STOP을 요청했다. 최신 사용자 요구인 Summary 직후 소등·대기 절전·BT 연결 유지를 만족하지 못했다.

1. 기존 IGN OFF 첫1초 완전 유지와5초 Summary는 보존한다. Summary 종료 시 즉시 PWM0으로 만들고 시계/ODO만 있는 진입 프레임을 한 번 제출한다. 실제 DLSWAP 완료 후 패널/EVE SLEEP에 들어간다.
2. AWAKE에서는 유효한 시계의 분이 달라질 때만 디스플레이를 깨워 프레임 하나를 제출하고 다시 잔다. 깨우기/갱신/scanout 동안도 PWM0이다. 배경 전환·사진 업로드는 보류하며 대기 시간을 연장하지 않는다. 아직 RTC 날짜가 설정되지 않은 실제 기기는 `--:--`가 유지되어 불필요한 분 갱신을 하지 않는다.
3. 설정된 Clock refresh time(기존 대기 시간, 기본60분) 이후 SLEEPING에서는 이 분 갱신도 중단한다. **양쪽 OFF 상태 모두 BT를 계속 유지**한다. 과거 BT 시간 만료에 의한 DEEP 요청은 앱 정책에서 제거했다. 기존 설정 키/메모리 배치는 보존하고 Bluetooth standby는 읽기 전용 `Keep connected`로 표시한다.
4. BT H4 UART의 수신을 보존하기 위해 CPU는 tickless Sleep/WFI를 사용한다. HSE/PLL, UART/DMA 및 SDRAM 동작 클록은 유지한다. SysTick/TIM6 tick interrupt는 쉬는 동안 중지하고 LSE RTC로 경과 시간을 보정한다. UART/DMA·IGN IRQ와 예정된 타이머가 CPU를 깨운다. 이는 MCU STOP/Standby가 아니며, BT 절전 핸드셰이크를 구현했다고 주장하지 않는다.
5. inherited IWDG 설정은 바꾸지 않고 다음 태스크 deadline·최대500ms·보수적으로 계산한 watchdog 반주기 중 가장 짧은 시간 이내에서 깨어나 reload한다. 실물 PR0/RLR4095이므로 watchdog 상한은136ms이다. CPU가60초 내내 연속해서 자는 설계가 아니다.
6. OFF에서 계기판 UART는 중지하고, 조도 센서 폴링과 새로운 OBD 질의는 하지 않는다. BT 수신·프로토콜/큐 처리는 유지한다. BT 앱 housekeeping은 초기화 때10ms, 안정된 OFF에서는100ms이며 native BTstack 타이머/수신 callback은 그대로 즉시 처리한다. I/O는100ms, 유휴 저장소/화면은1초 대기로 줄인다. 명시적 저장소/업데이트/SWD 복구 요청은 처리한다.
7. raw IGN edge가 UI를 깨운 직후 아직 디바운스가 안 끝난 경우를 위해, 확정 IGN snapshot 게시 후 UI에 태스크 알림을 보낸다. 분 경계 snapshot도 같은 방식으로 알린다. dark standby 복귀는 새 Welcome 프레임 scanout 후만 점등하고, Home 속도 링 sweep/최종0 이후 새 UART 패킷 조건을 보존한다.
8. 수면 중 새 화면 캡처는 오류로 응답하며 EVE를 깨우지 않는다. 완료된 SDRAM 캡처 다운로드/취소는 가능하다. 캡처 중 수면 전환은 미완성 결과를 폐기한다.

## 검증

- Product Release/Debug 빌드 및 메모리 예산 통과.
- ARM power UI O0/Os 각각1,151 assertions:100회 interrupted wake,100회 minute wake/single frame, delayed scanout, PWM0 유지, minute wake 도중 IGN ON.
- ARM power/RTC/display O0/Os 각각445 assertions: RX DMA active 상태의 retained Sleep은 STOP에 진입하지 않고 클록/FMC/DMA 보존, RTC 시간 보정, IGN 알림, 기존100회 STOP 경로 회귀. RTC shadow 잠금0.
- UI 상태머신 O0/Os/Oz: 각116,348 assertions. Settings O0/Os 각181.
- 캡처 O0/Os 각12,330 assertions: full/stripe CRC, sleeping GPU snapshot 거부, SDRAM download 유지, 중간 수면 취소.
- BT transport O0/Os 기존6개 시나리오 및 retained-link 시나리오403 assertions 추가. 두 폰+OBD 세션100회 housekeeping에서 연결 식별자 유지, HCI power-off/USART reset0, credit/send 요청 유지. 실제 무선 시험을 의미하지 않는다.
- Cube 재생성 fixture에서 Product/Integrated/Graphics 소스/링커 연결 복원, 반복 동기화 동일성, Core/IOC 불변 통과. 실제 Cube GUI 재생성은 수행하지 않았다.

## 실물에서 확인한 범위 / 남은 검증

- 실제 정상 IGN ON 부팅, Welcome/Home sweep1회 완료, 이후 새 실제 UART packet 수신. UART speed108km/h, ODO36731km 관측. Graphics29.9FPS / CPU59.2%, 그래픽 오류·SPI 실패0.
- 마지막 관측은 IGN ON, 물리 edge0, PWM25, power error0, RTC ready1/error0, 유효 fault record 없음(magic0)이다. RTC counter는 진행하지만 날짜 valid0이며 날짜를 임의 설정하지 않았다.
- OFF 요청은 사용자에게 전달했으나 마지막 읽기 시 아직 ON이었다. **실제 Summary 후 PWM0·panel sleep·CPU Sleep 누적/소비전류 및 물리 IGN 복귀는 미확인**이다. `observe.py`와 `observe_ui.py`로 후속 기록 가능.
- 실제 두 폰+OBD 무선 유지 검증은 정상 BT 기판에서 필요하다. 모의 시험을 RF 성공으로 해석하지 않는다.

관측 파일: `after-install`, `ui-after-install`, `final-power`. 설치 로그/바이너리: `install`.
