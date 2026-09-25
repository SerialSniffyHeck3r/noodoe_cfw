# AK550 SR1.5 V5.16: 부팅·업데이트 바이너리 재검증

작성: 2026-09-09 KST. 모든 분석은 로컬 파일에 대해 수행했다. 실제 계기판, Noodoe,
USB, SWD, Bluetooth에 명령을 보내지 않았다. 원본 BIN은 수정하지 않았다.

## 결론

1. 아카이브 9개 펌웨어의 크기·SHA-256이 보존 manifest와 모두 일치한다.
2. V5.16 SR1.5 복사본 두 개는 byte-identical이다. 고정 주소 분석은 아래 SHA에만 적용한다.
3. 기존에 미확인이던 SR1 계열 마지막 4바이트는 STM32 방식 CRC로 재현된다.
   4개 서로 다른 이미지에서 두 독립 계산 구현이 일치하고 전체 이미지 CRC residue는 0이다.
4. 하위 64KiB는 이 OTA application 이미지가 나타내는 주소 범위 밖이다.
   실물의 Noodoe resident loader와 영구 상태를 보존하려면 별도 읽기가 필요하다.
5. 기존 문서의 sector 2/3 '전체 보존 재기록' 설명은 틀렸다. 함수는 16KiB를 RAM에
   복사·병합하지만 재기록 루프는 앞 4KiB로 제한된다. 호출자·사용 범위는 미확정이다.
6. 파일 전송 핸들러에서 비동기 callback으로 넘어가는 경로와 일반 reset 경로는
   각각 확인했다. firmware DONE → 저장 위치 → install marker → loader로 이어지는
   전체 연결은 아직 확인하지 못했다.

## 1. 대상 식별과 여러 BIN의 차이

기준 이미지:

- `artifacts/ota-archive/2026-08-31-full/blobs/firmware/1657088080998-s1-SR1.5_ota_V516.bin`
- SHA-256: `3b64673054ca84b9cf504f4cc7ebcb2e6615b9fbf59df79a37a92dcf14f037ca`
- 크기: `458748 = 0x6FFFC` bytes
- initial MSP: `0x20025318`
- reset vector: `0x08076719` (Thumb 실행 주소 `0x08076718`)
- application base: `0x08010000`
- 표현된 마지막 주소: `0x0807FFFB`
- CRC word: 파일 offset `0x6FFF8`, 주소 `0x0807FFF8`, 값 `0x70067874`

| 아카이브 이름 | 크기 | initial MSP | 분류상 주의 |
| --- | ---: | --- | --- |
| SR1 V1.09 | 458748 | 0x20022AF0 | manifest series 3; 파일명 s2만으로 시리즈 추정 금지 |
| NewAK V2.07 | 458748 | 0x20022B10 | manifest series 3; 과거 AK 후보이며 donor 버전 미확인 |
| SR1.5 V5.16 | 458748 | 0x20025318 | 현재 상세 분석 기준 |
| EBike V5.16 | 458748 | 0x20025318 | 같은 크기·MSP지만 다른 hash/reset/CRC |
| SR2 Common V0.49 / V0.53 | 각각 917480 | 0x240383F8 / 0x24038410 | manifest series 1이 SR1.5와 겹친다 |
| SR2 CBK V0.50 | 917480 | 0x24038658 | 다른 메모리 계열 후보 |
| SR2 CV3 V1.24 / V1.28 | 각각 917480 | 0x24038748 | AK용 주소표 적용 금지 |

SR2 5개 이미지의 끝에서 8바이트 앞 word는 `zlib.crc32(file[:-8])`와 모두 일치한다.
SR1 계열과 trailer 규칙도 다르다. 파일명이 비CV3라는 이유, series 값 하나 또는
크기가 같다는 이유만으로 하드웨어·loader·이미지 호환성을 인정하지 않는다.

소유자는 2017 AK550 계기판·Noodoe 세트를 확보했고 실차는 2018 AK550이다.
그러나 구입한 모듈의 실제 MCU suffix, PCBA, firmware/boot 버전, RDP는 아직 없다.
'CV3 외 모든 모델이 같은 Noodoe 하드웨어'는 현재 소유자의 가설이며, 이 목록과
APK 세대 분기만으로 보드 동일성을 확정할 수 없다.

기계 검증 결과: [image-inventory.json](image-inventory.json).

## 2. 새로 재현한 SR1 계열 CRC

대상은 마지막 4바이트를 제외한 `458744 = 0x6FFF8` bytes, 즉 114686개 32-bit word다.

```text
polynomial  = 0x04C11DB7
initial     = 0xFFFFFFFF
input       = 파일에서 little-endian u32를 읽어 각 word의 최상위 bit부터 처리
reflection  = 없음
final xor   = 없음
stored      = 마지막 4바이트의 little-endian u32
```

| 이미지 | 저장값 | 재계산 | CRC word까지 포함한 residue |
| --- | --- | --- | --- |
| SR1 V1.09 | 0x93A080E2 | 일치 | 0 |
| NewAK V2.07 | 0xD108D5F1 | 일치 | 0 |
| SR1.5 V5.16 | 0x70067874 | 일치 | 0 |
| EBike V5.16 | 0x74733DD4 | 일치 | 0 |

`analyze_images.py`는 바이트 처리와 워드 byte-swap으로, `verify_sr1_crc.py`는
32-bit word XOR/다항식 나눗셈으로 각각 계산한다. 각 이미지의 한 비트를 메모리에서만
뒤집으면 원래 CRC와 달라지는 것도 확인했다. 원본 파일은 바꾸지 않았다.

STM32F4 CRC 장치의 word 입력, polynomial 및 reset값과 일치한다.
공식 근거: [ST RM0090](https://www.st.com/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf),
CRC chapter §4.1/4.2, §4.4.3/4.4.4 (Rev21 기준 pp114/116; 문서 판본별 페이지 변동 가능).

**확정 범위는 파일의 CRC 관계다.** 이것만으로 loader가 CRC만 검사한다거나,
서명이 없거나, CRC를 맞춘 custom image를 받아준다고 결론내리지 않는다.
APK의 전송용 padded CRC32와 이미지 내부의 이 CRC는 서로 다른 계산이다.

바이너리에서도 CRC 계열 루틴이 존재한다:

- `0x08042804`: 결과 포인터, data pointer, word count, accumulate flag를 받는 wrapper.
- `0x0804DB38`: peripheral +8 bit0을 set하여 CRC reset, 32-bit word를 DR에 순차 기록.
- `0x0804DB00`: reset 없이 같은 word 입력 loop로 누적.
- `0x0804DB7C`: handle state 조회.

다만 이 루틴과 OTA trailer 검사 호출자를 아직 연결하지 못했다. 함수 모양의 일치와
파일 CRC 재현은 별도 증거다. [crc-hal.asm.txt](crc-hal.asm.txt),
[crc-independent-verification.json](crc-independent-verification.json).

## 3. 파일 전송은 어디까지 확인됐나

`0x08028CE4/0x08028EA0/0x0802902C/0x080290FE`가 각각 command 0x0A/0x0B/0x0C/0x0D를 처리한다.

- negotiate `0x08028D66..0x08028D6E`는 packed `0x08000002`를 허용한다.
  이는 **location 0x0800 + type FILE(2)**이며, 실제 flash 주소 `0x08000000`과는 다르다.
- 일반 FILE은 `0x08028DF8`의 context+0x10 경로를 선택한 뒤 context+0x14 callback을 호출한다.
- `0x0A`는 internal event 0x0F, `0x0B`는 0x16, `0x0D`는 0x17을 구성한다.
- control/data는 context에 등록된 callback을 간접 호출한다.
- 따라서 명령 핸들러에서 다음 계층으로의 위임은 확인되지만 callback 자체를
  최종 NOR staging/flash install 함수로 간주할 근거는 아직 없다.

[spp-file-handlers.asm.txt](spp-file-handlers.asm.txt)는 raw bytes와 PC-relative literal을
같이 남긴다. 연속 range에는 literal pool이 섞일 수 있으므로 모든 출력 줄을 실행 코드로
취급하지 않는다. 직접 호출·정상 함수 경계·실제 분기를 확인한 항목만 본문에 사용했다.

## 4. 하위 flash 함수: 기존 문서 정정

함수 `0x080426D4`의 동작:

1. 대상 시작 주소가 `0x08008000..0x0800FFFF`인지 검사한다.
2. 주소를 16KiB 경계로 내리고 요청이 그 한 sector 안에 있는지 검사한다.
3. `0x4000` byte buffer를 할당하여 앞부분/변경분/뒷부분을 병합한다.
4. sector 2 또는 3 하나를 erase한다.
5. 그런데 재기록 loop는 `sector_base + 0x1000`에서 끝난다.

핵심 raw evidence:

```text
0x08042716  4f f4 80 40  mov.w r0,#0x4000       ; allocation
0x0804276c  17 f5 80 40  adds.w r0,r7,#0x4000   ; shadow-copy bound
0x080427b8  b9 46        mov r9,r7              ; destination begins sector base
0x080427c0  19 f1 04 09  adds.w r9,r9,#4         ; word increment
0x080427c4  17 f5 80 50  adds.w r0,r7,#0x1000    ; program bound = 4KiB
0x080427c8  81 45        cmp r9,r0
0x080427ca  09 d2        bhs 0x080427e0
0x080427d6  0b f0 f3 f9  bl 0x0804dbc0          ; word program
```

이 4KiB 경계는 같은 해시의 파일을 별도 분석 작업에서 독립적으로 재확인했다.
이는 실제 현장에서 데이터가 손실됐다는 관찰이 아니다. 나머지 12KiB가 원래 미사용인지,
이 함수가 어떤 데이터를 위해 호출되는지, 해당 코드가 현재 경로에서 사용되는지는 미확인이다.
그러나 **'16KiB sector 전체를 보존 재기록한다'는 기존 설명은 유지할 수 없다.**
이 루틴을 custom loader의 안전한 보존 갱신 알고리즘으로 복제해서는 안 된다.

[persistent-and-crc-wrapper.asm.txt](persistent-and-crc-wrapper.asm.txt).

## 5. application entry와 일반 reset

확인된 application 시작 경로:

```text
vector reset 0x08076719
  -> 0x08076718
  -> SystemInit 0x08073E90
       VTOR(0xE000ED08) = 0x08010000 at 0x08073ECA
  -> 0x08076A44
  -> 0x08075A70
       low-level-init check -> data-init candidate 0x08074DEC
       main candidate 0x0802FD20
```

이 시작점은 MCU 전원 투입 후 최초 resident loader의 reset entry가 아니라
보존 OTA application의 entry다. 하위 64KiB loader의 벡터·분기·검증 코드는
별도 실물 dump가 있어야 읽을 수 있다.

일반 reset 경로:

```text
0x080427FA wrapper
  -> 0x08043C50 (interrupt mask, GPIO sequence)
  -> 0x080366FE
  -> 0x080366A6
       AIRCR(0xE000ED0C) = preserved PRIGROUP | 0x05FA0004
       DSB; infinite wait
```

`0x08070D58/60/68/70`에도 0x08043C50 호출이 있다. 그러므로 reset routine이
있다는 사실은 firmware DONE의 install reset임을 증명하지 않는다.
`0x080427FB` raw pointer bytes는 `0x0807738B`에서 발견되지만 비정렬 데이터 위치다.
이를 확인된 호출자로 셈하지 않았다. 초기화/압축 데이터 및 간접 callback 해석은 미완료다.

## 6. 재현

이미 설치된 Python에 capstone 5.0.9를 workspace `.tools/analysis-python`에 추가했다.
표준 라이브러리만 필요한 inventory/CRC와 Capstone이 필요한 trace를 구분한다.

```powershell
$researchPython = '<local-user>\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $researchPython .\analysis\2026-09-09-ak550-boot-update-audit\analyze_images.py
& $researchPython .\analysis\2026-09-09-ak550-boot-update-audit\verify_sr1_crc.py
$env:PYTHONPATH = '<workspace>\.tools\analysis-python'
& $researchPython .\analysis\2026-09-09-ak550-boot-update-audit\trace_boot_update.py
```

실물과 연결되는 코드가 없다. manifest mismatch 또는 V5.16 hash mismatch면 중단한다.
각 출처 파일을 새 디렉터리에만 읽기 결과로 기록한다.

## 7. 다음에 필요한 실제 증거

- donor MCU marking, package, board revision, DeviceInfo.
- read-only SWD IDCODE, flash size, option bytes/RDP/PCROP.
- lower 64KiB 두 번 읽기 + 실제 용량의 전체 internal flash 두 번 읽기와 hash 비교.
- NOR 원본과 정상 전원 조건 보존; firmware write 전에 storage 및 복구 경로 확보.
- lower loader의 CRC/size/series 검사와 update journal/staging slot 역어셈블리.
- 실행 중 context callback 포인터 snapshot 또는 IAR 초기화 데이터 복원으로
  SPP callback에서 storage/install state machine까지의 누락된 연결 확인.

함께 읽기: [UART 감사](../2026-09-09-ak550-uart-audit/README.md),
[APK 감사](../2026-09-09-ak550-apk-update-audit/README.md),
[부트로더 백업 계획](../../docs/2026-09-09-ak550-bootloader-backup-plan.md).