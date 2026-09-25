# AK550 Noodoe: 순정 부트로더 비파괴 백업 계획

작성: 2026-09-09 KST. 대상: 소유자의 2017 AK550 계기판 + 별도 Noodoe 세트.

이 문서는 로컬 연구와 ST 공식 자료를 대조한 **향후 실험 계획**이다. 이번 작업에서 보드에 접속하거나 메모리를 읽고 쓰지 않았다. 실물 MCU 마킹, SWD 위치, 정상 전원, 디버거 모델, 보호 상태는 미확인이다. STM32F429 계열이라는 전제는 보존된 SR1.5 V5.16 이미지의 분석이며 실물 확정이 아니다.

## 1. 보존 대상

| 주소/영역 | 현시점 의미 | 보존 판단 |
| --- | --- | --- |
| `0x08000000–0x08007FFF` | Noodoe 최초 부팅/복구 코드 후보 | 원본 확보 우선 |
| `0x08008000–0x0800FFFF` | 앱이 수정하는 sectors 2/3 | 코드·상태 경계와 무관하게 통째로 보존 |
| `0x08010000–0x0807FFFB` | 로컬 V5.16 OTA 앱 범위 | 실물과 비교할 기준 |
| 실물 플래시의 나머지 영역 | 코드/상태/빈 공간 여부 미상 | 용량 확인 후 전부 보존 |
| ST system memory | 공장 ROM 부트로더 | Noodoe 순정 부트로더를 대신하지 않음 |

하위 64 KiB는 해당 OTA 앱에 없다. 최초 우선 단위는 `0x08000000`부터 `0x10000` bytes이며 최종 목표는 **실물 내부 플래시 전체**다. 하위 영역을 추정으로 합성하거나 OTA 파일을 `0x08000000`에 쓰는 것은 복구가 아니다. sectors 2/3의 앱 쓰기 코드는 존재하지만, 그 전체가 완전하게 보존·재기록된다고 여기서 단정하지 않는다.

로컬 근거: [MCU 설정서](ak550-sr15-mcu-peripheral-settings.md), [UART·OTA 연구서 §7](ak550-sr15-uart-ota-bluetooth-display-io-bringup.md), [부트로더 전략](custom-firmware-bootloader-strategy.md). 이번 OTA CRC 재현은 미확보 first-stage의 검증·설치 정책을 증명하지 않는다.

## 2. SWD 우선, 보호 해제는 백업이 아니다

현재 가진 디버거로 SWD를 읽는 것을 우선한다. 추가 구매를 전제로 하지 않는다. 연결 실패만으로 RDP를 판정하지 않는다.

| 읽은 상태 | 해석/행동 |
| --- | --- |
| RDP `0xAA` / Level 0 | 읽기 시도. PCROP도 확인 |
| RDP가 `0xAA`, `0xCC` 이외 / Level 1 | SWD 및 RAM/ROM 부팅을 통한 플래시 접근 차단. 로그 보존 후 중지 |
| RDP `0xCC` / Level 2 | 디버그 및 RAM/ROM 부팅 제한, 되돌릴 수 없음 |
| RDP0지만 특정 구간만 읽히지 않음 | SPRMOD/PCROP, 접근 오류 및 실제 읽기 길이 확인 |

**RDP1→RDP0는 원본 플래시와 backup SRAM을 지운다.** `Read Unprotect`, `-rdu`, RDP 값 쓰기는 백업 방법이 아니다. PCROP 해제도 원본 보존과 양립한다고 가정하지 않는다. 옵션 바이트는 표시·기록만 한다. WRP와 RDP/PCROP는 구분한다. [ST RM0090, §3.7.3–3.7.5, Rev21 pp.93–97](https://www.st.com/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)

이것은 **공식 비파괴 읽기 경로의 한계**다. RDP1에서 순정 플래시 코드가 자체 플래시에 접근할 수 있는 것과 외부 도구가 원본을 추출할 수 있다는 주장은 다르다. 현재 Noodoe 앱에 하위 플래시를 외부로 내보내는 명령이 있다는 증거는 없다.

## 3. 실물 준비와 읽기 일관성

1. MCU 마킹과 1번 핀 방향을 확인한 뒤 해당 패키지의 다리 번호를 정한다. F429일 때 SWDIO/SWCLK 신호는 PA13/PA14다.
2. 전원·USB를 분리하고 그 신호에서 패드·저항 단자로 연결을 추적한다. 코팅에 막힌 접촉을 단선으로 판정하지 않는다.
3. 확보한 **차량 ↔ 계기판** 핀아웃과 순정 세트 구조로 정상 전원을 확인한다. 이 핀아웃은 **계기판 ↔ Noodoe** 핀아웃이 아니다.
4. MCU 전압, GND, SWDIO, SWCLK, 가능하면 NRST를 확보한다. 디버거의 VTref 입력과 전원 출력을 구분한다.
5. 아래는 ST-LINK와 CubeProgrammer의 예시다. 가진 디버거가 다르면 제조사 도구의 동일한 읽기 기능을 사용한다.

앱이 sectors 2/3을 쓸 수 있으므로 **코어를 정지한 뒤 덤프**한다. NRST가 확인되면 Under Reset 연결을 우선한다. flash erase 명령은 아니지만 CPU/RAM 상태와 실행 흐름은 바꾼다. 부팅 순간에 이미 실행된 코드, watchdog, 연결 해제 뒤 재실행을 포함해 보드 상태가 전혀 변하지 않는다고 표현하지 않는다.

NORMAL은 reset 후 halt이며 그 사이 코드 실행 가능성을 기록한다. HOTPLUG는 자동 halt/reset 없이 붙으므로 일관된 첫 플래시 백업의 기본값으로 삼지 않는다. UR은 NRST 연결이 필요하며 reset vector catch를 이용한다. [ST CubeProgrammer CLI connect/halt](https://dev.st.com/stm32cube-docs/prog/2.23.0/en/docs/markup/CubeProg_Command_Lines.html#connect)

## 4. 먼저 기록할 값과 실제 덤프 크기

| 값 | F429 계열 주소/방법 | 해석 |
| --- | --- | --- |
| DBGMCU_IDCODE | `0xE0042000`, 32-bit | DEV_ID `0x419`는 F42/43 계열; 패키지·용량 확정 아님 |
| 플래시 용량 | `0x1FFF7A22`, 16-bit | KiB 단위: `0x0400`=1 MiB, `0x0800`=2 MiB |
| UID | `0x1FFF7A10`, 12 bytes | 보드와 덤프 연결 식별자 |
| 옵션 | GUI 또는 `-ob displ` | RDP, WRP/nWRP, SPRMOD, DB1M/BFB2 등 전체 |
| 실험 조건 | 사진/로그 | 마킹, 디버거, 도구 버전, VTref, 연결 모드·속도 |

레지스터 근거: [ST RM0090 §38.6.1, §39.1–39.2](https://www.st.com.cn/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf). 전체 길이는 `읽은 KiB×1024` bytes다. 1 MiB면 `0x08000000–0x080FFFFF`, 2 MiB면 `0x08000000–0x081FFFFF`. OTA가 약 448 KiB라는 이유로 실물 용량을 512 KiB로 제한하지 않는다. bank 구성과 swap 상태도 기록한다.

F42/43 system-memory 매핑 전체는 `0x1FFF0000–0x1FFF77FF`(30 KiB)다. 필요시 ROM 자체도 보존할 수 있지만 하위 Noodoe 플래시 백업의 대체물이 아니다. AN2606의 ROM 코드 사용량과 주소 매핑 크기는 구분한다. [ST RM0090 §3.4, flash organization](https://www.st.com/resource/en/reference_manual/rm0090-stm32f4xx-reference-manual-stmicroelectronics.pdf)

## 5. 조건부 CubeProgrammer 예시

**이번에 실행한 명령이 아니다.** 설치 버전의 `-h`와 실제 디버거를 먼저 확인한다. 공식 2.23.0 문법 `--upload <start_address> <size> <file_path>`에서 size는 바이트다. `-r32`는 짧은 레지스터 표시, 큰 덤프는 `--upload`를 사용한다. [ST CLI read/option bytes](https://dev.st.com/stm32cube-docs/prog/2.23.0/en/docs/markup/CubeProg_Command_Lines.html#read)

### 5.1 식별·옵션 확인

PowerShell. 설치 위치가 다르면 `$cp`를 바꾼다. `$captureDir`은 실험마다 새 이름을 써서 기존 덤프를 덮지 않는다.

```powershell
$cp = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
$captureDir = '<workspace>\evidence\swd\2017-ak550-donor-first-read'
New-Item -ItemType Directory -Path $captureDir -ErrorAction Stop | Out-Null
& $cp -h

# NRST가 실제 연결되었을 때만 UR/HWrst 사용.
# 100 kHz는 시작값 예시. 도구가 선택한 실제 속도를 기록한다.
& $cp -c port=SWD mode=UR reset=HWrst freq=100 -halt -ob displ -r32 0xE0042000 4 2>&1 |
    Tee-Object -FilePath "$captureDir\identify-options.txt"
```

이 시점에 출력을 읽는다. 접속 실패·불명 RDP·보호 구간이면 아래 읽기로 무작정 진행하지 않는다. `-ob displ`은 표시, `-ob 이름=값`은 쓰기다. GUI의 Apply/Read Unprotect를 누르지 않는다. 성공 종료 코드와 오류 로그도 확인한다.

### 5.2 RDP0 및 읽기 조건 확인 후 용량·UID

```powershell
& $cp -c port=SWD mode=UR reset=HWrst freq=100 -halt --upload 0x1FFF7A22 2 "$captureDir\flash-size.bin" --upload 0x1FFF7A10 12 "$captureDir\uid.bin" 2>&1 |
    Tee-Object -FilePath "$captureDir\identity-read.txt"

$sizeBytes = [IO.File]::ReadAllBytes("$captureDir\flash-size.bin")
if ($sizeBytes.Length -ne 2) { throw '용량 읽기 결과가 2 bytes가 아닙니다.' }
$flashKiB = [BitConverter]::ToUInt16($sizeBytes, 0)
$flashBytes = [int]$flashKiB * 1024
$flashKiB
$flashBytes
```

도구의 용량 표시와 마킹이 이 값과 맞는지 대조한다. 0, 65535 또는 칩 사양과 모순되는 값이면 덤프 길이로 쓰지 않는다.

### 5.3 하위 64 KiB부터 두 번, 전체 두 번

한 연결에서 halt 후 연속 읽어 중간 앱 실행 가능성을 줄인다. 설치 버전이 반복 `--upload`를 지원하지 않으면 GUI에서 연결을 유지한 채 같은 범위를 반복 저장한다.

```powershell
# $flashBytes는 실물 값과 대조를 마친 길이.
$readArgs = @(
    '-c', 'port=SWD', 'mode=UR', 'reset=HWrst', 'freq=100', '-halt',
    '--upload', '0x08000000', '0x10000', "$captureDir\lower64k-a.bin",
    '--upload', '0x08000000', '0x10000', "$captureDir\lower64k-b.bin",
    '--upload', '0x08000000', "$flashBytes", "$captureDir\internal-full-a.bin",
    '--upload', '0x08000000', "$flashBytes", "$captureDir\internal-full-b.bin"
)
& $cp @readArgs 2>&1 | Tee-Object -FilePath "$captureDir\flash-read.txt"

$dumpNames = 'lower64k-a.bin', 'lower64k-b.bin', 'internal-full-a.bin', 'internal-full-b.bin'
foreach ($dumpName in $dumpNames) {
    $dumpPath = Join-Path $captureDir $dumpName
    Get-Item -LiteralPath $dumpPath | Select-Object Name, Length
    Get-FileHash -LiteralPath $dumpPath -Algorithm SHA256
}
```

읽다가 코어 재실행·리셋·연결 끊김이 있으면 전원·접촉·watchdog을 확인한다. flash 옵션 변경으로 해결하려 하지 않는다.

완료 기준:

- 하위 파일은 각각 65,536 bytes이며 두 SHA-256이 같다.
- 전체 파일은 각각 실물 용량과 같으며 두 SHA-256이 같다.
- 전체 파일의 첫 65,536 bytes가 하위 파일과 일치한다.
- 로그에 보호·통신·부분 읽기 오류가 없고 벡터/코드가 타당하다.
- `0x10000` 위치부터 OTA와 비교하되 버전 차이를 불량으로 단정하지 않는다.
- 로그·마킹·UID·옵션과 덤프를 묶고 별도 저장 위치에 한 벌 더 복사한다.

같은 해시는 같은 오류 데이터를 배제하지 않는다. 파일 길이와 오류 로그, 전부 FF/00인지, 실제 벡터와 코드의 타당성까지 함께 확인한다. 이 단계 뒤 필요한 OTP/옵션 원시값과 외부 NOR 백업을 별도로 진행한다. 현재 내부 플래시 읽기 계획이 외부 NOR까지 백업하는 것은 아니다.

## 6. ROM 경로를 UART5/USB-B와 혼동하지 않는다

F42/43 ROM은 USART1 TX=PA9/RX=PA10, USART3 TX=PB10/RX=PB11 또는 TX=PC10/RX=PC11을 지원한다. **앱의 UART5 PC12/PD2는 ROM USART 목록에 없다.** ROM USART는 8E1이며 앱 링크의 8N1과 다르다. ROM USB는 **OTG_FS, D−=PA11/D+=PA12**다. V9는 V7 대비 I2C2/3, SPI1/2/4를 추가한다. [ST AN2606 Rev70 §41, Tables 89/91/92, pp.202–214](https://www.st.com/resource/en/application_note/an2606-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf)

V5.16 로컬 분석은 USART1을 Bluetooth HCI/RTS/CTS용으로, 활성 USB IRQ를 OTG_HS로 분류한다. 다음은 그 자료와 ST 표를 대조한 **추론**이다.

- 계기판 UART 커넥터에 ROM 명령을 보내면 백업된다는 근거가 없다.
- USART1에는 Bluetooth 컨트롤러가 연결되어 있을 수 있다. 외부 송신기를 무작정 병렬 연결하지 않는다.
- USB-B가 HS 코어의 embedded FS PHY 쪽이면 Full Speed로 동작해도 ROM OTG_FS 포트와 다르다. 전송 속도와 주변장치 이름을 구분한다.
- USB 단독 무반응은 ROM DFU 여부의 증거가 아니다. 정상 전원과 실제 D+/D− 배선을 확인한다.

ROM 진입에는 boot 선택과 주변장치 충돌을 확인해야 한다. AN2606 Pattern 5의 명시적 조건은 BOOT0=1, BOOT1=0, BFB2=0이다. BOOT0만 올리면 충분하다고 단정하거나 진입을 위해 옵션 값을 변경하지 않는다. 현재는 핀·전원 조건 미확인이므로 실행 명령을 제시할 단계가 아니다. [ST AN2606 Table 2/§41](https://www.st.com/resource/en/application_note/an2606-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf)

조건이 충족되면 Get/Get ID와 Read Memory로 같은 영역을 읽는 보조 경로를 검토한다. **RDP1이면 ROM 경로도 플래시 읽기를 우회하지 못한다.** USART Readout Unprotect는 전체 플래시를 지운다. [ST AN3155 Rev21 §3.4/§3.12, pp.16–18/36](https://www.st.com/resource/en/application_note/cd00264342-usart-protocol-used-in-the-stm32-bootloader-stmicroelectronics.pdf)

## 7. 기존 문서 표현의 보정

| 표현/오해 | 보정 |
| --- | --- |
| ROM 부트로더: SWD/UART/USB | SWD는 ROM 실행을 요구하지 않는 별도 디버그 경로 |
| UART 확보 → ROM 복구 | 앱 UART5와 ROM USART1/3은 다름 |
| USB 복구 후보 | HS/FS 코어와 실제 핀 대조 필요 |
| 보호 해제 후 읽기 | 해제 과정에서 원본이 지워질 수 있음 |
| OTA 끝 주소 → MCU 전체 용량 | 실물 용량 레지스터로 확인 |
| 하위 64 KiB 전체가 부트로더 | 전부 보존하되 코드/상태 경계는 실물 덤프로 확인 |

추가 CRC 참고: RM0090 Rev21 §4.1/4.2 p.114는 고정 polynomial `0x04C11DB7`과 32-bit 데이터 입력, §4.4.3/4.4.4 p.116은 CRC reset 뒤 DR=`0xFFFFFFFF`를 설명한다. [ST RM0090 CRC 장](https://www.st.com/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf). OTA 말미 계산을 재현한 결과와 실제 first-stage의 호출·검증 정책은 별도 증거다. ST 문서의 버전이 바뀌면 페이지가 이동할 수 있으므로 장/절을 우선한다.
