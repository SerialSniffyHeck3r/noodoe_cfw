# AK550 차량 계기판의 TOOL0 / RESET 단서

2026-09-10 KST. 대상은 사용자가 구입한2017 AK550 **차량 계기판**이다. Noodoe 모듈과 구분한다.

## 새 관찰과 출처

사용자가 차량 계기판 하네스에 `/RESET`와 `TOOL0`가 있다고 보고했다. 이번 작업에서 하네스 사진·해당 배선도 원문·MCU 마킹을 직접 확인하지 않았다. 로컬 텍스트/기존DOCX와PDF파일명 검색에서도 TOOL0를 계기판 MCU 형번에 연결하는 추가 직접 근거는 발견하지 못했다.

## 판단

**Renesas RL78 또는 구형 NEC/Renesas 78K0R 계열의 프로그래밍/온칩디버그 접점일 가능성이 높다.** TOOL0는 이들 제조사 문서에서 실제 사용하는 핀명이다. 다만 동일 명칭이 여러 계열에 쓰이므로 RL78이나 특정 R5F 부품으로 확정하지 않는다.

- Renesas의 RL78용 E1/E20/E2/E2 Lite 연결 매뉴얼은 TOOL0를 명령·데이터 전송용 양방향 신호로 명시하고 RESET 연결을 함께 다룬다. VDD/EMVDD/GND와 연결 회로 조건은 장치별 확인 대상이다. [공식 RL78 연결 매뉴얼 R20UT1994, Rev9.40](https://www.renesas.com/en/document/mat/e1e20e2-emulator-e2-emulator-lite-additional-document-users-manual-notes-connection-rl78)
- RL78 serial programming 문서에는 TOOL0를 쓰는 단선 UART와 reset을 이용한 모드 진입이 나온다. 따라서 TOOL0는 일반 차량 통신 UART의 TX 하나로 취급하지 않는다. 프로토콜 A/B 등 변종·통신 속도·전기 조건은 정확한 MCU가 결정한다. [RL78 Protocol A](https://www.renesas.com/en/document/apn/rl78-family-rl78-microcontroller-rl78-protocol-serial-programming-guide), [Protocol B](https://www.renesas.com/en/document/apn/rl78-family-rl78-microcontroller-rl78-protocol-b-serial-programming-guide)
- 구형78K0R/Kx3도 TOOL0와 RESET을 사용하는 플래시 프로그래밍 경로가 있다. 이름만으로RL78에 고정하지 않는 근거다. [78K0R/Kx3 Flash Memory Programming, U18433](https://www.renesas.com/en/document/apn/78k0rkx3-flash-memory-programming-programmer)

하네스로 외부에 나와 있다면 조립 후 생산·검사·서비스용 접속을 의도했을 가능성이 있다. 이는 신호명과 접근 위치에 근거한 추론이며 KYMCO가 용도를 확인한 사실은 아니다. `/RESET`는 관례상active-low reset 표기이지만 실제 극성·전압·중간 버퍼 존재는 회로/측정으로 확인해야 한다.

## 백업 목표와 관련한 중요한 차이

프로그래밍 접점을 찾았다고 기존 플래시 전체 읽기가 보장되지는 않는다. Renesas는 RL78에서 RFP의 serial programming으로 메모리 덤프를 할 수 없고, 조건이 충족된 debug 연결을 별도로 사용해야 한다고 설명한다. 같은 공식 문서에는 debugger 연결 과정에서 일부 OCD 영역이 덮어써질 수 있다는 주의도 있다. 따라서 칩 식별 전의 디버거 자동접속을 무변경 읽기로 취급하지 않는다. [Renesas: Memory Dump using CS+](https://en-support.renesas.com/knowledgeBase/21153484)

원본 보존 목적의 다음 순서는 다음과 같다.

1. **차량 계기판** MCU의 전체 마킹과 패키지를 확인한다. 이전의 Noodoe 대형 IC 관찰을 여기로 옮겨 적용하지 않는다.
2. 전원을 끈 상태에서 하네스 TOOL0와 /RESET이 해당 MCU 또는 주변 저항·버퍼·리셋IC의 어디로 이어지는지 추적한다.
3. 정상 전원 조건에서 MCU 전원과 각 신호의 대기 전압·리셋 파형을 기록한다. 하네스 위치만 보고12V나3.3V 로직을 가정하지 않는다.
4. 정확한 칩의 프로그래밍·OCD·보안 설정과 보유 디버거 지원 여부를 대조한 뒤 연결 방법을 결정한다. 이 단계에서 물리 신호를 넣거나 flash erase/unlock을 실행하지 않았다.

계기판 MCU를 읽을 수 있게 된다면 앞서 Noodoe에서 확인한 F5 UART의 **상대측 구현**과 차량 입력 변환을 연구할 수 있다. 현재 TOOL0 발견만으로 그 읽기가 가능하다고 확정한 것은 아니다. Noodoe의 STM32 계열/SWD 과제는 별도로 남는다.