> 2026-09-21: retired module. Excluded from all firmware profiles; retained only as historical protocol evidence/tests. Runtime and UI no longer reference it.

# ELM prompt 기반 읽기 서비스

`ObdService_Init → SetConnected` 뒤 태스크에서 `TakeCommand`의 nonzero 반환만 즉시 송신한다. 반환 길이는CR을 포함하고 NUL은 제외한다. 수신 조각은 `Feed`, 무수신 동안은 `Poll`에 전달한다. `GetSnapshot`은 값·raw·시간·phase를 복사한다. 하드웨어/BT/RTOS 호출은 없다. 한 context를 한 태스크가 소유해야 한다.

초기화는 `ATI → ATE0 → ATL0 → ATS0 → ATH0 → ATM0 → ATTP0 → 0100`이다. ATM0는 성공 protocol의 자동 저장을 끄며 ATSP/ATZ/EEPROM 저장·DTC 삭제·ECU 변경 명령을 발행하지 않는다. Mode01 PID0C/0D/05/0F/11/04/0B/10만 지원 bitmap에 따라 순환한다.0100 NO DATA는 지원 미확인으로 계속한다. 이는 adapter 설정도 전혀 변경하지 않는 수동 sniff가 아니라, 세션 설정 후 ECU의 표준 현재값을 질의하는 방식이다. [ELM327 공식 DS](https://www.elmelectronics.com/wp-content/uploads/2017/01/ELM327DS.pdf), AT descriptions/prompt/OBD sections를 대조했다.

values index 순서의 단위는 RPM×4(raw AB), km/h, coolant°C, intake°C, throttle‰, load‰, MAP kPa, MAF mg/s다. raw_values와 각 value_ms를 보존한다. Headers-off 정상 응답이 여러 ECU에서 오면 첫 값을 선택하고 multiple_replies를 올린다. ECU identity 선택, CAN header 및 ISO-TP 여러 frame 응답은 지원하지 않는다. 요청 PID·길이·hex 전체 줄이 맞아야 하며 garbage 혼합은 MALFORMED다.

기본 timeout2000ms/stale5000ms/poll200ms다. 자동 protocol 탐색이 느린 adapter에는 caller가 더 긴 timeout을 설정해야 한다. deadline은 TakeCommand에서 시작한다. timeout 뒤 DRAINING으로 가며 실제 `>`가 올 때까지 새 명령을 막는다. 초기화 중 실패는 재연결까지 FAILED다. PID 실패는 그 필드의 유효성만 내리고 다음 query를 허용한다. prompt가 영영 오지 않으면 transport가 재연결해야 하며 타임아웃만으로 새 명령을 덮어 보내지 않는다.

실제AK550 ECU/ELM clone의 지원 PID·배선·protocol은 아직 검증하지 않았다. 테스트 통과가 실차 통신 성공을 뜻하지 않는다.
