# 잘못된 Noodoe 펌웨어의 동작과 복구 경로

2026-09-11. 대상은 확보한 AK550 도너 Noodoe의 실제 내부 플래시, resident 부트로더와 APP다. 별도 Android 개발 프로젝트는 이번 분석에서 제외했다. 장치에 연결하거나 잘못된 이미지를 실행하지 않았다.

## 결론과 추론의 범위

**APP만 잘못되고 MCU 전원·디버그 접근·보호 설정이 정상이라면, 현재 가진 SWD와 원본 덤프로 되돌리는 경로가 남는다.** Noodoe의 자체 부트로더에는 이전 APP 자동 복원이 없고, 실패 화면에서 BT 파일을 받는 복구 서비스도 없다.

잘못된 프로그램의 가능한 명령 실행을 무한히 열거할 수는 없다. 여기서는 **설치가 어디까지 진행됐는지, 무엇이 손상됐는지, 어떤 복구 통로가 남는지**를 기준으로 결과가 달라지는 분기를 나눴다. 실제 코드가 확인된 부분, CPU 규칙, 외부 회로·새 APP에 따른 조건부 추론을 구분한다.

## 1. 설치되기 전부터 부트 분기까지

| 경우 | 예상 동작 | 복구·다음 조치 |
|---|---|---|
| 순정 APP가 버전·길이·IGN 조건에서 거절 | 새 APP 설치 요청을 만들지 않음. 기존 APP 유지 | 파일/전송 조건 수정. 플래시 복구가 필요한 상황은 아님 |
| 512 KiB 전체 백업이나 더 큰 다른 계열 이미지를 APP로 보냄 | 순정 APP 및 resident의 APP 상한 448 KiB를 초과하므로 정상 경로에서 거절 | SWD에서 잘못된 시작 주소로 강제 기록한 경우에는 이 검사를 거치지 않으므로 실제 덮인 주소를 별도로 복원 |
| 데이터 누락, 잘못된 전송 CRC, NOR 재판독 불일치 | 정상 파일 완료 상태로 진행하지 않음. NOR에는 일부 byte가 남을 수 있음 | 정상 세션으로 다시 준비. 이전 pending이 없는지와 완료 상태를 구별 |
| 파일을 저장했지만 DONE/IGN OFF 인계를 못 함 | NOR에는 파일이 있어도 내부 APP는 유지 | 저장 존재만으로 자동 설치하지 않음. 전체 전원을 끊으면 RAM 완료 상태도 소실 가능 |
| 잘못된 내용을 보냈지만 그 내용에 맞는 전송 CRC도 보냄 | CRC는 전송 일관성을 확인할 뿐이다. 내용의 실행 가능성은 보증하지 않음 | 순정 수신기의 다른 조건을 통과하면 잘못된 코드를 실제로 설치할 수 있음 |
| SR1 trailer CRC만 잘못됨, 전송 CRC는 일치 | 조사한 정상 수신→resident 복사 경로는 trailer를 별도로 검사하지 않음 | 거절을 기대할 근거 없음. 나머지 APP 전체에 trailer 검사 경로가 없다고 전수 증명한 것은 아님 |
| 내부 요청 CRC가 0 또는 FFFFFFFF | 부트가 비활성으로 취급하여 설치를 건너뜀 | 기존 APP의 MSP gate가 통과하면 그 APP 진입. NOR 파일이 정상이어도 자동 복원 안 함 |
| 요청 CRC가 틀렸지만 0/FFFFFFFF 외 비영 값 | resident는 CRC를 재계산하지 않으므로 복사 분기로 들어갈 수 있음 | 메타 수동 조작으로 검증을 대체하지 않음. NOR가 잘못됐으면 그 데이터를 설치할 수 있음 |
| 요청 slot이 7F80/7F90 외 값이거나 길이>70000 | 정상화하여 요청 후보에서 제외 | 기존 APP가 온전하면 실행 가능. 이미 APP가 손상됐다면 이것만으로 복구되지 않음 |
| 첫 resident version이 현재 E0000과 다름 | 부트가 자신의 값으로 수정하며 요청을 해제하는 메타 재기록 호출 | 새 APP 버전을 첫 word에 잘못 넣으면 설치가 사라질 수 있음. 기록 실패는 별도 분기 |
| APP 요청 대신 BL slot 7F80을 지정 | 별도 BL installer가 선택될 수 있음 | APP 파일로 BL을 대체하면 다음 부트 자체를 파괴할 수 있음. 8000 초과는 BL installer에서 실패하며 APP로 fallback하지 않음 |

마지막 두 행처럼 **설치 요청을 잘못 만든 경우**는 일반 BT 파일 수신 오류와 다르다. 이전 요청이 남아 있다면 이후 NOR 덮어쓰기가 그 요청의 실제 payload까지 바꿀 수 있다.

## 2. 내부 플래시를 쓰는 동안

| 경우 | 예상 동작 | 남는 복구 경로 |
|---|---|---|
| NOR 최초 블록 read 실패 | 내부 APP erase 전에 실패 루프 | 요청·전원·NOR가 정상화되면 다음 실제 부팅에서 재시도. SWD도 별도 사용 가능 |
| 이후 NOR read 또는 APP program 실패 | APP 앞부분이 이미 바뀌었을 수 있음. 현 실행에서는 실패 루프 | 요청과 정상 NOR가 남아 있으면 다음 부트에서 **처음부터 동일 이미지 재복사** |
| APP erase/program 오류를 HAL이 놓침 | 성공 처리·요청 해제 후 불완전 APP로 진입할 수 있음 | SWD readback으로 원본/목표 이미지 대조 후 복구. UI 성공만으로 판정 불가 |
| APP 복사 도중 전체 전원 상실 | 부분 APP가 남을 수 있음 | resident·pending·NOR가 온전하면 다음 부트 재복사. 잘못 만든 파일이면 재복사해도 같은 버그 |
| 복사 완료 후 요청 clear 중단 | 새 APP와 부분 메타가 남음 | 실제 메타 bit에 따라 재복사/APP 진입/정상화가 달라짐. 앞선 byte-prefix 모델 참고 |
| BL 자체 erase/program 중단 | 현재 RAM 코드가 잠시 동작해도 전원 재시작 후 resident가 실행 불가할 수 있음 | SWD 또는 조건이 맞는 ST ROM 부트로더로 원본 resident 복원 |

실패 루프 `0x200045F0`에는 재설치·BT·USB 파일 수신·software reset 호출이 없다. PG13 OFF 조건에서 전원 종료 후보 GPIO를 출력한다. 외부 회로가 실제 전원 재시작을 만들지 않는다면 **IGN을 OFF→ON으로 토글하는 것만으로 그 루프가 설치 main으로 돌아가지는 않는다.**

### 짧은 파일과 길이를 틀린 경우

실제 복사량은 `L = ceil(image_length/0x1000)*0x1000`이다. 마지막 블록의 image_length 밖 byte도 NOR에서 읽어 쓴다. APP 전체를 매번 지우는 구현은 아니다.

| 실제 복사량 L | 지우는 섹터 | 기존 APP가 그대로 남을 수 있는 뒤쪽 |
|---|---|---|
| `1..0x10000` | 4 | `0x08020000–0x0807FFFF` |
| `0x10000<L<=0x30000` | 4, 5 | `0x08040000–0x0807FFFF` |
| `0x30000<L<=0x50000` | 4, 5, 6 | `0x08060000–0x0807FFFF` |
| `0x50000<L<=0x70000` | 4–7 | APP 뒤쪽의 미접촉 섹터 없음 |

erase가 성공했다면 복사 끝부터 마지막으로 지운 섹터 끝까지는 FF다. 그보다 뒤의 **지우지 않은 섹터는 구 코드**다. 잘못된 길이·링커·분기 주소가 이 영역을 참조하면 새 코드/구 코드/FF가 섞인 상태에서 실행할 수 있다. 이는 이전 버전으로 정상 롤백하는 구조가 아니다.

현재 순정 APP의 최소 길이가 `0x50001`이므로 정상 순정 인계는 APP 섹터 4–7에 도달한다. 위의 더 짧은 경우는 커스텀 인계·메타 수동 오류·다른 쓰기 경로까지 포함한 분류다.

## 3. 복사는 끝났지만 잘못된 APP를 실행하는 경우

| 잘못된 부분 | 예상 증상과 조건 | 복구 |
|---|---|---|
| 초기 MSP가 부트 mask를 통과하지 못함 | APP로 점프하지 않고 실패 화면/전원 출력 루프 | SWD로 올바른 APP 복원. 요청이 이미 0이면 NOR만 남아 있어도 재설치 안 함 |
| MSP가 mask는 통과하지만 실제 스택 주소로 부적합 | 함수 진입·push·예외 stacking에서 fault 가능 | SWD로 벡터/링커 수정. mask 통과는 RAM 유효성 증명이 아님 |
| Reset vector bit0=0 | Thumb 상태 위반으로 fault 가능 | SWD 복원. CPU fault는 FLASH가 영구 손상됐다는 뜻이 아님 |
| Reset 주소가 존재하지 않는 곳·실행 불가 영역 | instruction fetch fault 등 가능 | fault 상태/PC 확인 후 SWD 복원 |
| Reset이 엉뚱하지만 유효한 코드 주소 | 즉시 fault하지 않고 엉뚱한 코드 실행 가능. 예: resident Reset `0x080004C5` 재진입·반복 초기화 | 잘못된 link base/vector를 고쳐 재기록 |
| VTOR를 08010000으로 옮기지 않음 | IRQ/fault가 boot RAM 벡터를 사용. RAM 벡터와 handler가 온전하면 boot fault loop로 갈 수 있음 | SWD로 startup 수정. 다음 행의 RAM 파괴와 구별 |
| `.data/.bss` 초기화로 boot RAM 벡터·handler까지 덮음 | VTOR가 그대로면 잘못된 예외 주소 실행, fault escalation/lockup 가능 | reset 또는 debugger로 CPU를 멈추고 APP 복원 |
| 순정 APP fault handler를 유지한 패치가 fault | 해당 handler가 보드 정리 후 SYSRESETREQ를 호출하므로 반복 reset 가능 | 실행 전에 Under Reset으로 잡아 복구 |
| 완전 신규 APP의 fault handler/무한루프 | 자신의 handler에 따라 정지·reset·로그·기타 동작. 일률적으로 단정 불가 | 디버그 접근 가능 시 halt/Under Reset 후 복구 |
| APP Reset handler가 반환 | BLX 다음의 부트 코드로 돌아갈 수 있지만 MSP·RAM이 이미 바뀌어 정상 반환을 보장하지 않음 | Reset handler가 반환하지 않게 수정하고 SWD 복원 |
| 클록·FPU·SDRAM·인터럽트·런타임 초기화 오류 | 초기 단계 정지, 표시 이상, IRQ 직후 crash, BT/USB 미인식 등 | APP 초기화 수정 후 SWD 복구 |
| 화면/리소스 코드만 실패 | 검은 화면·흰 화면·깨진 화면이어도 CPU/BT는 동작할 수 있음 | BT updater가 실제로 살아 있으면 정상 이미지 수신 가능. 아니면 SWD |
| BT 코드를 누락·비활성화하거나 수신 정책을 잘못 구현 | 나머지 기능이 살아도 다음 BT 업데이트를 못 받을 수 있음 | SWD로 updater를 포함한 APP 복원 |
| SWD 핀을 GPIO 등으로 재설정하거나 너무 일찍 저전력 진입 | 일반 접속이 실패하거나 타이밍에 따라 붙었다 끊김 | NRST 연결을 사용한 Under Reset 경로. 전원/보호 조건 정상이라는 전제 |
| watchdog 또는 잘못된 전원 GPIO 때문에 반복 reset/꺼짐 | 접속 창이 짧거나 MCU rail 자체가 사라질 수 있음 | 코드 실행 전 debug 확보 및 전원 상태 확인. rail이 없으면 SWD 설정만으로 해결되지 않음 |
| 다른 보드용인데 크기·벡터·CRC는 적합한 이미지 | 부팅할 수도 있지만 pin/peripheral/resource 설정 불일치로 오동작 가능 | 해당 도너 원본으로 복원. 같은 MCU 계열·크기가 보드 호환을 보장하지 않음 |

CPU 규칙과 동작 근거: Cortex-M4는 Thumb 전용이며, HardFault/NMI 처리 중의 추가 hard fault는 lockup을 일으킬 수 있다. lockup은 reset 또는 debugger halt 등으로 벗어날 수 있는 CPU 상태다. [ST PM0214 Rev10, pp23·47](https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortex-m4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf).

실물 부트의 HardFault handler는 `0x2000659A`의 `b .`이지만, 순정 APP의 HardFault handler `0x08070D56`은 reset 경로를 호출한다. 따라서 **잘못된 APP면 항상 무한루프다**, 또는 **항상 재부팅한다**는 설명은 둘 다 틀리다. 새 이미지의 벡터·handler·RAM 상태에 따라 달라진다.

## 4. APP 이외의 것을 실제로 망가뜨린 경우

단순 이미지 설치가 모든 아래 손상을 자동으로 만드는 것은 아니다. 새 코드가 해당 영역에 기록하거나, 프로그래머에서 잘못된 대상·설정을 조작했을 때의 추가 분기다.

| 손상 범위 | 결과 | 복구 범위 |
|---|---|---|
| resident BL `0x08000000–0x08007FFF` | 전원을 켜도 Noodoe 부트 코드 실행 불가 가능 | SWD 접근이 되면 원본 BL 복원. ST 공장 ROM은 별도 영역이므로 조건부 대체 경로 |
| 메타 섹터 2 | 설치 무시, 반복 설치, 잘못된 BL/APP 슬롯 선택 가능 | 정상 원본 메타 섹터와 올바른 APP를 함께 확인·복원 |
| 데이터 섹터 3 | 식별·설정 등의 손상으로 코드가 정상이어도 비정상 동작 가능 | 같은 도너의 원본 섹터 3 복원. 다른 기기의 데이터로 대체하지 않음 |
| 외부 NOR staging만 손상 | pending이 있으면 잘못된 파일 설치 또는 반복 실패 | pending 해소와 APP 복원. staging 잔류만으로 재설치하지는 않음 |
| 외부 NOR 파일시스템/리소스 손상 | 내부 APP가 정상이어도 화면·개인화·리소스 기능 이상 가능 | 내부 FLASH 복원과 별도로 외부 저장소 복구 필요 |
| RDP level 1 | 기존 FLASH 읽기 제한. 보호를 되돌리는 과정에 전체 FLASH 소거가 수반될 수 있음 | 공식 regression 조건에 따라 내부 원본 전체를 복원하는 별도 절차. 보존된 백업이 중요 |
| RDP level 2 | F429의 디버그·system-memory 부팅 복구가 영구 제한됨 | RDP2 자체를 되돌릴 수 없음. 실행 가능한 APP의 자체 갱신은 별개지만, 그것까지 죽으면 일반 외부 복구가 막혀 MCU/보드 교체 범주 |
| OTP program/lock | 일부 식별·키 등 일회성 데이터는 일반 flash처럼 원상복구할 수 없음 | 내부 FLASH BIN 재기록으로 해결되지 않음. 변경 bit·lock과 용도에 따라 교체/기능 손실 |
| WRP/PCROP/기타 option 변경 | 쓰기/읽기 차단, 부팅·watchdog 동작 변화 등 | 실제 옵션과 칩별 공식 해제 조건을 확인. RDP2와 일반 쓰기 보호를 같은 상태로 취급하지 않음 |
| 전기적 손상 | MCU/전원/버스 자체가 동작하지 않음 | 펌웨어 백업만으로 수리 불가. 하드웨어 수리/교체 |

RDP·OTP·ROM의 공식 근거와 조건은 [ROM 복구 조사](../analysis/2026-09-11-bad-image-recovery/rom-recovery/README.md) 및 [ST RM0090](https://www.st.com/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)에 정리한다. 일반적인 APP crash나 SWD pin 재설정 자체가 RDP2 또는 OTP 변경과 같은 것은 아니다.

## 5. 현재 장비로 하는 복구 판단 순서

### 5.1 우선은 SWD

1. 지금 정상 동작한 ST-LINK, SWDIO/SWCLK/GND와 타깃 전원을 사용한다. 전압 감지 입력/출력을 혼동하지 않는다. MCU rail이 실제 유지되는지 확인한다.
2. 접속 가능하면 재시작 전에 PC/MSP/VTOR·fault 상태와 `0x08008000`의 메타, APP 첫 벡터를 보존해 원인을 구분한다. 화면 상태만으로 FLASH 손상을 판정하지 않는다.
3. 일반 접속이 실패하면 **확인된 NRST를 연결한 Connect Under Reset + Hardware reset**을 사용한다. 코드가 SWD 핀을 변경하기 전에 잡기 위한 경로다. NRST가 연결되지 않거나 reset 중 전원 rail이 꺼지면 같은 조건이 성립하지 않는다. ST 공식 설명: [STM32CubeProgrammer ST-LINK 설정](https://dev.st.com/stm32cube-docs/prog/2.23.0/en/docs/markup/CubeProg_UserManual/Main_window.html).
4. 디버그 접근과 FLASH 쓰기 권한이 정상임을 확인한 뒤 손상 범위에 맞게 원본을 복원한다. 실제 기록 전 현재 오류 로그·보호 상태를 확인한다. 단순 접속 실패를 해결하려고 무조건 RDP 해제/전체 erase를 먼저 실행하는 절차는 아니다.

Under Reset은 실제 명령 실행 전에 잡는 기능이다. `Normal` 또는 단순 reset pulse는 새 APP가 먼저 실행되는 창이 생길 수 있다. [ST 명령행 문서](https://dev.st.com/stm32cube-docs/prog/2.23.0/en/docs/markup/CubeProg_Command_Lines.html).

### 5.2 무엇을 복원할 것인가

- **APP만 잘못됨, BL·데이터·메타 정상:** 원본 APP 영역 `0x08010000–0x0807FFFF`를 복원한다.
- **pending이 남았거나 메타가 불명확:** APP 복원과 함께 섹터 2를 정상 상태로 복원하거나 정확히 pending을 해소한다. **APP를 고친 뒤 CPU를 먼저 풀어 주면 NOR의 나쁜 파일을 다시 덮어쓸 수 있다.** 두 영역과 요청 상태를 확인할 때까지 실행을 재개하지 않는다.
- **BL도 손상:** 원본 BL까지 복원한다. 손상 범위가 불명확한 내부 FLASH 전체라면 이 도너의 512 KiB 원본을 기준으로 복구한다. 내부 전체 복구는 외부 NOR/옵션/OTP까지 복원하는 작업이 아니다.
- **외부 NOR도 손상:** 내부 APP 복구 후 접근 가능한 저장소 인터페이스나 보드에 맞는 외부 loader로 별도 복구한다. 다른 ST 평가보드용 외부 loader가 이 Noodoe의 SPI5 배선·칩·전원 제어와 호환된다고 가정하지 않는다.

복원 후는 단순 성공 메시지에 의존하지 않고 해당 범위를 다시 읽어 준비한 원본과 비교한다. 이후 정상 boot 설정에서 재시작하여 기능을 확인한다. 이 문서 작성 중에는 위 쓰기 작업을 실행하지 않았다.

### 5.3 ST ROM 부트로더는 두 번째 통로

Noodoe resident FLASH가 망가져도 STM32 공장 system memory ROM 자체는 별도다. 다만 **보드에서 ROM 부팅 조건과 해당 인터페이스에 실제 접근할 수 있어야 한다.** BOOT0/BOOT1·옵션, USB/USART 배선과 clock 조건, RDP 상태를 충족해야 한다.

F429/439 V9.x의 USB DFU는 USB_OTG_FS 경로다. 이 Noodoe의 USB-B가 정상 APP에서 MSC 저장장치로 보였다는 사실만으로 ROM DFU가 되는 것은 확정되지 않는다. DFU mode를 실제로 열고 인식하는 시험은 아직 하지 않았다. 지원 핀·HSE 조건·BOOT pattern은 [ST AN2606 기반 조사](../analysis/2026-09-11-bad-image-recovery/rom-recovery/README.md)를 따른다.

오히려 기존 V5.16 정적 분석은 **OTG_HS core `0x40040000`의 내장 FS PHY** 사용을 확인했다. ROM DFU가 사용하는 OTG_FS의 PA11/PA12와 다른 USB 경로일 수 있다. USB 링크가 Full Speed라는 말과 OTG_FS 컨트롤러에 연결됐다는 말은 다르다. 현재 USB-B가 곧 복구 포트라고 기대하기 전에 PCB 배선을 대조해야 한다. [기존 USB 하드웨어 분석](../analysis/2026-09-01-sr15-hardware-crosscheck/README.md).

ROM USART 후보도 계기판 통신 UART5와 다르다. 차량 UART 핀과 115200 8N1을 그대로 쓰면 ROM에 연결된다는 근거는 없다. 실제 ROM 핀과 프로토콜은 별도 확인 대상이다.

ROM DFU/UART의 존재를 resident 실패 화면의 BT 복구와 혼동하지 않는다. 또한 USB 저장장치에 BIN 파일을 복사하면 내부 FLASH를 복구한다는 경로도 이번 분석에서 확인되지 않았다.

## 6. 준비한 복구 참고 파일

A/B 원본의 크기·전체 일치와 SHA-256을 확인한 뒤, **내용을 변경하지 않고 영역별로 분리한 BIN**을 준비했다. 네 파일을 순서대로 이어 붙이면 원본 A 전체와 정확히 일치한다.

| 참고 파일 | 실제 MCU 주소 | 크기 |
|---|---|---:|
| [원본 APP](../analysis/2026-09-11-bad-image-recovery/recovery-reference/app_original_08010000_00070000.bin) | `0x08010000` | 448 KiB |
| [원본 메타 섹터 2](../analysis/2026-09-11-bad-image-recovery/recovery-reference/metadata_sector2_original_08008000_00004000.bin) | `0x08008000` | 16 KiB |
| [원본 BL](../analysis/2026-09-11-bad-image-recovery/recovery-reference/boot_original_08000000_00008000.bin) | `0x08000000` | 32 KiB |
| [원본 데이터 섹터 3](../analysis/2026-09-11-bad-image-recovery/recovery-reference/data_sector3_original_0800C000_00004000.bin) | `0x0800C000` | 16 KiB |

APP 참고 BIN은 원본의 마지막 FF 네 바이트까지 포함한 **SWD 복구용 영역 사본**이다. 순정 OTA 전송 파일 `0x6FFFC`와 길이가 다르다. 메타 원본의 첫 다섯 word는 `E0000,0,0,0,0`으로 미처리 설치 요청이 없다. 저장 위치·해시는 [manifest](../analysis/2026-09-11-bad-image-recovery/recovery-reference/manifest.json)에 있다.

외부 저장소의 기존 백업은 [USB 보존 기록](2026-09-09-noodoe-usb-acquisition.md)에 있다. USB 노출 127.5 MiB를 보존한 것이며 숨겨진 마지막 512 KiB, 옵션 바이트, OTP는 내부 A/B나 그 USB 이미지에 포함되지 않는다. 당시 이미지의 파일시스템 오류까지 그대로 보존한 것이므로 공장 초기 정상 리소스 세트라는 뜻도 아니다.

실험 전에 확보하면 복구 범위가 넓어지는 것은 **실제 NRST 연결 확인, 옵션 바이트·OTP/잠금 상태의 읽기 기록, ROM 진입 핀 접근성 확인, 필요하면 USB 비노출 NOR 영역의 읽기 백업**이다. 이들은 현재 가진 내부 FLASH 사본이 대신해 주지 못하는 정보다.

## 증거

- [분석 인덱스](../analysis/2026-09-11-bad-image-recovery/README.md)
- [부트 메타·길이·잘못된 이미지 시나리오](../analysis/2026-09-11-bad-image-recovery/boot-scenarios/README.md)
- [APP 벡터·fault·초기화·디버그 시나리오](../analysis/2026-09-11-bad-image-recovery/app-scenarios/README.md)
- [공식 ROM·보호 조건](../analysis/2026-09-11-bad-image-recovery/rom-recovery/README.md)
- [원본 분리 재현 코드](../analysis/2026-09-11-bad-image-recovery/prepare_reference.py)

모델은 부트 메타 13개와 복사 길이 11개의 경계 사례를 검증했고, APP/boot fault 벡터는 원본과 복원 RAM의 실제 주소로 대조했다. 실물 crash·전원 차단·보호 변경·복구 쓰기는 시험하지 않았다.
