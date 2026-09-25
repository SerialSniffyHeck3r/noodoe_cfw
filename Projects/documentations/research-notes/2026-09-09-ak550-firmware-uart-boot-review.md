# AK550 Noodoe: 순정 BIN·APK·문서 대조와 부트로더 백업 연구

작성일: 2026-09-09 KST

이번 분석은 보존된 펌웨어와 순정 APK를 직접 읽고 기존 문서의 주장을 재검증한 것이다.
실물에 SWD/USB/Bluetooth/UART 명령을 보내지 않았으며, 부트로더 덤프나 firmware write는 수행하지 않았다.

## 먼저 읽을 결과

**부트로더 백업은 여전히 SWD 읽기가 1순위다.** 앱에서 사용하는 차량 UART5가
STM32 ROM 부트로더의 USART 경로는 아니며, 순정 앱에서 하위 플래시를 읽어 내보내는
검증된 기능도 찾지 못했다. 접점·전원·보호 상태 확인 후 하위 64KiB와 내부 flash
전체를 보존하는 조건부 절차를 준비했다.

이번에 새로 좁힌 문제:

- 미확인이던 **SR1 계열 OTA 끝 4바이트의 CRC를 재현**했다. 서로 다른 4개 이미지,
  독립 계산 구현 2개, 전체 CRC residue=0, 1-bit 변형 검출로 교차 확인했다.
- **UART5의 바이트 단위 interrupt 수신 경로**를 확인했다. DMA 설정이 있다고
  F5 parser가 DMA ring을 쓴다고 단정한 설명을 보정했다.
- **USART1 baud는 초기 상수 뒤 호출자 값으로 재설정**된다. 3,686,400 하나로
  모든 시점의 HCI baud를 고정할 수 없다. DMA2 S5/S7 Channel4 설정은 재확인했다.
- **하위 flash 갱신 함수가 16KiB 전체를 보존 재기록한다는 설명을 정정**했다.
  16KiB를 RAM에 복사·병합하지만 실제 program loop는 앞 4KiB에서 끝난다.
- APK의 `FILE_QUERY`, `BASIC_FIRMWARE_UPGRADE` enum 이름을 기능 존재의 증거로
  쓰면 안 된다. 현대식 parser/serializer의 해당 분기는 invalid 처리를 한다.

## 1. 대상과 적용 범위

사용자의 실차는 2018 AK550, 구입품은 2017 AK550 계기판·Noodoe 모듈 세트다.
차량 계기판과 Noodoe는 별도 장치이며 **차량 ↔ 계기판 핀아웃만 확보**했다.
계기판 ↔ Noodoe의 물리 핀아웃, donor MCU marking/PCBA/현재 firmware/RDP는 미확인이다.
현재 보유 디버거, Agilent 디지털 스코프 2대, ANENG 멀티미터, 무전기용 전원 공급기를
전제로 하며 새 장비 구매를 백업 계획의 전제로 삼지 않았다.

| 증거 | 이번 확인 |
| --- | --- |
| OTA manifest | 9개 firmware 파일의 크기와 SHA-256 전부 일치 |
| 상세 BIN | SR1.5 V5.16, 458748 bytes, SHA `3b64673054ca84b9cf504f4cc7ebcb2e6615b9fbf59df79a37a92dcf14f037ca` |
| 중복 BIN | 별도 sr1.5-series-1 폴더 복사본과 byte-identical |
| 순정 APK | com.noodoe.sunray 2.1.14, APK SHA `6c9c82aebfcbce221c68530c731f9c469db6d7fdba30439b5549a0e239c79e80` |
| APK 재검증 | DEX 6개 내부 SHA1/Adler 검증; 핵심 5개 class를 classes6.dex에서 새로 decompile |
| 실물 검증 | 이번에는 없음. 기존 실차 기록과 새 donor의 동일성을 자동 인정하지 않음 |

'CV3 이외 모두 같은 모듈'은 아직 하드웨어 동일성 가설이다. 보존 목록에는
SR1/NewAK/SR1.5/EBike와 SR2 Common/CBK/CV3가 있고, series 1에도 SR1.5와
SR2 Common 이미지가 함께 있다. 제품명·시리즈 번호 하나로 세대나 flash 호환성을
결정하지 않는다. 2017 donor의 identity를 확보한 뒤 분석 대상과 대조한다.

## 2. UART: 두 개의 서로 다른 역할

| 구분 | UART5 | USART1 |
| --- | --- | --- |
| 앱에서 상대 역할 | 차량 계기판 쪽 데이터 | TI Bluetooth controller HCI |
| 분석된 설정 | 115200, 8N1, no flow | RTS/CTS, DMA2 S5/S7 Ch4; baud 재설정 존재 |
| 확인한 수신/처리 | RXNE byte interrupt → F5 parser | HCI transport 초기화/제어 |
| 신호 후보 | PC12/PD2 | PA9/PA10/PA11/PA12 |
| 아직 모르는 것 | donor 커넥터 번호·실물 전압·기동 조건 | 실제 controller·시점별 baud·donor 물리 배선 |

신호 후보는 기존 MCU 추론과 대조할 대상이다. 이번 감사에서 동적 GPIO 테이블과
보드 revision 선택 전체를 복원하지 않았으므로 물리 핀아웃 확정으로 사용하지 않는다.

UART5에서 추적한 경로는 다음과 같다.

```text
UART5 IRQ 0x08070E68
  -> UART IRQ handler 0x0804C25C
  -> RXNE handler 0x0804C55C
  -> receive-complete callback 0x08043C08
  -> dispatcher 0x0804E7C2
  -> F5 parser 0x0804EA14
  -> telemetry/profile/status 소비
```

F5 수신기는 CMD `21/22/41/42`를 허용하며 LEN=0을 거절한다.
`0x41=250`, `0x42=71`은 consumer의 고정 복사 크기이지 parser의 정확한 LEN
검증이 아니다. 짧은 payload 처리의 정적 결함 후보는 발견했으나 실물 시험,
코드 실행, dump 가능성을 검증한 것이 아니며 백업 방법으로 제안하지 않는다.
이 앱의 네 명령 분기에서 명시적인 flash-read/update/boot 명령은 확인되지 않았다.
OTA에 없는 resident loader가 UART5를 사용하는지는 별도 미확인이다.

[원시 명령어와 21개 anchor 검증](../analysis/2026-09-09-ak550-uart-audit/README.md).

## 3. 업데이트: 수신과 설치를 구분

순정 modern SPP update는 file-transfer 경로를 사용한다.

```text
순정 Android APK
  -> Bluetooth Classic SPP
  -> negotiate 0x0A / control 0x0B / data 0x0D
  -> firmware location ID 0x0800, FILE type 2
  -> 실행 중 SR1.5 application의 command handler
  -> internal event와 등록 callback
  -> [미확인] staging 위치 / install marker / 실제 loader 검증·복사
```

`location 0x0800`은 protocol 식별값이며 flash 주소가 아니다.
APK가 파일을 보냈다는 것, 전송 ACK/DONE을 받았다는 것, 실제 flash에 설치돼
재부팅했다는 것은 서로 다른 검증 단계다.

새 APK decompile은 기존 AK tuple `protocol 0.0 / HW0 / firmware5.16`의 VERSION_1_5
분류를 지지한다. protocol major >2도 VERSION_2_0으로 분류하므로 과거 문서의
'2.x만'이라는 요약은 엄밀하지 않다. firmware에는 modern 경로와 legacy 경로가
혼재하므로 legacy 명령을 AK donor의 백업 API로 가정하지 않는다.

`FILE_QUERY`/`BASIC_FIRMWARE_UPGRADE`라는 enum 이름은 남아 있지만 modern serializer와
parser의 대응 분기는 invalid 처리한다. diagnostic 앱의 UART 화면은 외부 UART
시험기를 이용한 검사 결과를 수동 기록하는 화면이며, UART byte 송수신 또는 MCU
flash read를 구현했다고 볼 수 없다.

[APK 원문·fresh decompile·전송 상태기계 근거](../analysis/2026-09-09-ak550-apk-update-audit/README.md).

## 4. OTA 이미지 CRC와 하위 flash

SR1 계열 CRC는 `file[:-4]`를 little-endian 32-bit word로 읽고 polynomial
`0x04C11DB7`, initial `0xFFFFFFFF`, no reflection, no final XOR로 처리하면 재현된다.

| 파일 | 마지막 word |
| --- | --- |
| SR1 V1.09 | 0x93A080E2 |
| NewAK V2.07 | 0xD108D5F1 |
| SR1.5 V5.16 | 0x70067874 |
| EBike V5.16 | 0x74733DD4 |

이 결과는 기존 '끝 word 알고리즘 미확인'에서 진전이다. **CRC를 알았다고 custom image가
설치된다는 뜻은 아니다.** loader가 검사하는 다른 조건, 서명 유무, 버전·series·길이
정책은 하위 flash가 없어서 미확인이다. Android 전송용 padded CRC32와도 다르다.

V5.16 `SystemInit`은 VTOR를 `0x08010000`으로 설정한다. 실제 OTA 표현 범위는
`0x08010000..0x0807FFFB`이므로 하위 64KiB는 보존 파일로 대체할 수 없다.

또한 `0x080426D4`의 하위 flash 함수는 sector 2/3을 선택하고 16KiB shadow를
만들지만 `0x080427C4`의 raw `17 f5 80 50`가 나타내는 program loop 상한은
**sector base+0x1000 (4KiB)**다. 독립 분석으로 재확인했다. 나머지 영역의 실제
사용 여부·호출 조건이 없으므로 실물 데이터 손실을 단정하지 않지만, 이 함수를
안전한 전체-sector 보존 갱신 구현으로 복제할 수는 없다.

일반 reset은 `0x08043C50 → 0x080366FE → 0x080366A6`에서 AIRCR SYSRESETREQ까지
추적했다. firmware DONE에서 이 경로로 이어지는 호출은 확인하지 못했다.

[바이너리 보고서·독립 CRC 계산·부팅 경로](../analysis/2026-09-09-ak550-boot-update-audit/README.md).

## 5. 실물에서 부트로더를 백업할 순서

1. MCU marking/package와 SWDIO/SWCLK/NRST 접점을 확인하고 정상 전원을 확보한다.
2. 가진 디버거의 모델에 맞는 도구로 저속 SWD 연결, IDCODE·option bytes를 기록한다.
3. RDP0 및 PCROP 등 읽기 조건을 확인한다. RDP regression/Read Unprotect를 하지 않는다.
4. 실제 flash 용량을 `0x1FFF7A22`의 16-bit KiB 값과 MCU 사양으로 대조한다.
5. 코어가 멈춘 상태에서 **하위 64KiB 두 번**, **실제 용량의 전체 flash 두 번**을 읽는다.
6. 길이·SHA·전체 dump의 앞 64KiB 일치·오류 로그·벡터 타당성을 함께 확인한다.
7. 옵션/UID/사진/로그를 같이 보존하고, 이후 NOR 원본과 물리 복구 경로를 확보한다.

read-only 예시 명령과 합격 기준은 [부트로더 백업 계획](2026-09-09-ak550-bootloader-backup-plan.md)에 있다.
명령은 실행되지 않았으며, 디버거 모델과 설치 도구 버전을 확인한 뒤 적용하는 조건부 예시다.

STM32F42/43 ROM의 USART는 USART1/USART3이며 앱의 UART5가 아니다. ROM USB는
OTG_FS PA11/PA12이므로, 앱의 OTG_HS와 실제 USB-B 배선을 대조해야 한다.
RDP1은 ROM flash-read도 차단하므로 UART/USB로 바꾸는 것이 보호 우회 방법이 되지 않는다.
[ST AN2606](https://www.st.com/resource/en/application_note/an2606-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf),
[ST RM0090](https://www.st.com/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf).

## 6. 이번 문서 대조에서 바뀐 판단

| 이전 표현 | 이번 판단 |
| --- | --- |
| SR1 trailer 알고리즘 미확인 | 4개 이미지에서 STM32 word CRC 관계 재현; loader 정책은 계속 미확인 |
| 하위 sector 전체 보존 재기록 | 16KiB shadow/erase와 4KiB program을 구분해야 함 |
| UART5 F5가 DMA ring 수신 | DMA 설정과 별도로 byte-IRQ 직접 수신 경로 확인 |
| USART1 3,686,400 고정 | HAL 초기 상수 뒤 인자 기반 baud 재설정 있음 |
| USART1 DMA channel 미확인 | V5.16 초기화의 Channel4 상수 확인 |
| 250/71을 순정이 정확히 검증 | 소비 크기이며 parser/dispatcher에 해당 길이 검증은 없음 |
| boot/file-query enum이면 기능 존재 | modern APK에서 invalid 처리하는 항목 존재 |
| 차량 UART 또는 USB-B로 ROM backup | 지원 peripheral·실제 배선·boot 선택·RDP를 별도 확인 |

기존 실행 기준 문서에는 이 보고서를 우선 읽도록 정정 안내를 추가한다.
역사적 분석 원문과 기계 판독 hardware 계약의 모든 항목을 이번에 재작성한 것은 아니다.
새 firmware 구현에서 충돌하면 이번 raw-byte 재검증과 실물 측정을 우선한다.