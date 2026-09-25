# 순정 STM32 word CRC 서비스와 검증

`Drivers/BSP/src/BSP_CRC.c`와 `Drivers/BSP/inc/BSP_CRC.h`는 SR1 이미지 trailer의 CRC를 계산한다. polynomial `0x04C11DB7`, 초기값 `0xFFFFFFFF`, 입력 4 bytes를 little-endian word로 조립, 입력/출력 bit 반사 없음, 최종 XOR 없음이다. **Bluetooth 전송용 reflected CRC32와 다른 값이다.** 이 서비스는 이미지의 서명, 허용 버전, 설치 범위 또는 부트로더 업데이트 정책을 판정하지 않는다.

순정 근거는 `Reversing/analysis/2026-09-09-ak550-boot-update-audit/verify_sr1_crc.py`, `crc-independent-verification.json`, `Reversing/docs/2026-09-11-noodoe-bootloader-and-bluetooth-update.md`다. V5.16 전송 CRC `0xAF819880`과 이미지 trailer `0x70067874`를 혼용하지 않는다. 마지막 word를 포함한 전체 이미지의 word CRC residue는 0이다. 순정 BL의 복사 경로가 이 trailer를 별도 검증한다는 뜻은 아니다.

ST의 [STM32F4 HAL CRC 구현](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/master/Src/stm32f4xx_hal_crc.c)은 reset 뒤 32-bit word를 DR에 차례대로 쓰고 결과를 읽는 동작을 보여 준다. 하드웨어 정의는 [RM0090 CRC 장](https://www.st.com/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)을 따른다. 이번 웹 열람에서 RM0090은 파일 크기 제한으로 본문을 재수집하지 못했으며 기존 로컬 분석과 프로젝트 CMSIS 정의를 함께 사용했다.

## 호출 계약

| API | 입력과 결과 |
|---|---|
| `BSP_CRC_Init` | 호출자 소유 `BSP_CRC_Context`를 초기화한다. 다른 context와 레지스터를 건드리지 않는다. |
| `BSP_CRC_Update` | byte 단위 길이의 임의 청크를 받아 남은 1~3 bytes를 다음 청크와 결합한다. 입력 누계는 uint32 범위다. |
| `BSP_CRC_Final` | context를 바꾸지 않고 결과를 반환한다. `REQUIRE_FULL_WORDS`는 부분 word를 오류로 돌려주고 `PAD_FF`는 남은 word의 상위 bytes만 FF로 채운다. 빈 CRC는 FFFFFFFF다. |
| `BSP_CRC_TryHardwareWords` | 정렬되지 않은 주소도 허용하지만 길이는 4의 배수, 최대 4096 bytes다. 독립 계산이라 호출마다 CRC를 초기화한다. 대기하지 않고 BUSY를 반환한다. |
| `BSP_CRC_CompareHardwareWords` | 불변 입력의 SW/HW 결과를 비교해 `BSP_CRC_Comparison`에 두 값과 길이를 남긴다. 다르면 MISMATCH다. 다른 오류는 출력 구조체를 보존한다. |

소프트웨어 context 하나에 대한 동시 Update/Final은 호출자가 직렬화한다. 다른 context는 서로 독립적이다. Final은 봉인 연산이 아니므로 조회 이후 Update를 이어도 된다. PAD_FF 결과를 한 번 조회했다고 입력 누계나 다음 청크가 바뀌지 않는다. 실제 포맷의 padding 값이 FF임을 입증하는 API가 아니므로, 기존 완전 word 이미지에는 REQUIRE_FULL_WORDS를 사용한다.

```c
BSP_CRC_Context crc;
uint32_t image_crc;
BSP_CRC_Init(&crc);
/* storage reader의 각 청크마다 호출하며 모든 반환값을 검사한다. */
BSP_CRC_Status status = BSP_CRC_Update(&crc, chunk, chunk_bytes);
/* 마지막에는 trailer를 제외한 원래 계약 범위가 4의 배수인지 확인한다. */
if (status == BSP_CRC_OK) {
    status = BSP_CRC_Final(&crc, BSP_CRC_REQUIRE_FULL_WORDS, &image_crc);
}
```

HW API의 공유 lock은 이 API를 사용하는 호출끼리만 유효하다. **HAL_CRC_*, MX_CRC_Init/DeInit 또는 직접 DR/RCC 조작과 동시에 사용하지 않는다.** 소프트웨어 CRC는 하드웨어 소유권을 요구하지 않는다. 기존 `MX_CRC_Init`이 서비스 시작 전에 순차 실행되는 것은 허용되며 그때 켜진 clock은 HW 계산 종료 후에도 켜져 있다. CRC DR/CR은 변경되고 이전 누적값은 보존하지 않는다. IDR, 옵션 바이트, 플래시에는 접근하지 않는다.

HW 소유권 획득과 RCC 갱신만 짧은 PRIMASK 구간에서 처리한다. 계산 중에는 호출 전 IRQ 상태를 유지하며, 이미 IRQ가 금지되었으면 임의로 켜지 않는다. RCC의 CRC enable bit는 진입 상태를 복원하고 다른 clock bit는 현재 값을 보존한다. 단일 Cortex-M 코어 전제이고 NMI/fault handler 호출은 제외한다. 4096-byte 상한은 최대 1024 DR 쓰기를 뜻하며, 실제 168 MHz 처리 시간이나 ISR 지연을 측정했다는 뜻은 아니다.

기본 BSP include 경로와 `Drivers` 소스 루트에서 빌드되는 파일이므로 이 작업에서는 Core, IOC, 프로젝트 설정 또는 장치 상태를 수정하지 않았다.

## 검증 결과

`crc_run.py`는 설치된 ARM GCC로 **실제 BSP_CRC.c**를 컴파일하고 Unicorn Cortex-M에서 실행한다. Python이 제품 CRC를 재구현해 대신 계산하지 않는다. CRC/RCC/PRIMASK 경계만 `crc_test_port.h`와 `crc_test.c`의 모형으로 바꾼다. CRC 모형은 별도 16-entry nibble table을 사용한다. 제품의 CMSIS 레지스터 경로도 `STM32F429xx`, `-Os`, `-Wall -Wextra -Werror`로 별도 컴파일한다.

검증 명령:

```powershell
& '<local-user>/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tools/tests/protocol_services/crc_run.py
```

- `-O0`, `-Os` 각각 2,865개 CHECK 통과.
- 알려진 답, endian 순서, trailer를 붙인 residue 0, 256-byte 입력의 모든 257개 분할 위치와 1~19-byte 청크 통과.
- 교차 갱신하는 두 context, 부분 word FF padding, Final 뒤 Update, 잘못된 인수와 누계 overflow 시 상태/출력 보존 통과.
- 모의 레지스터의 재진입 BUSY, 호출 전 PRIMASK 0/1, clock 켜짐/꺼짐, 계산 중 변경된 다른 RCC bit의 보존, 비정렬 주소, 최대 4096-byte 입력, 강제 mismatch 통과.
- `-Os` 실제 C에서 원본 OTA 이미지 네 개를 각각 389-byte 청크로 읽어 trailer와 full-image residue를 검증했다.

| 원본 이미지 | 길이 | trailer / 실제 C CRC | 전체 residue |
|---|---:|---:|---:|
| SR1 V1.09 | 458748 | `0x93A080E2` | 0 |
| NewAK V2.07 | 458748 | `0xD108D5F1` | 0 |
| SR1.5 V5.16 | 458748 | `0x70067874` | 0 |
| EBike V5.16 | 458748 | `0x74733DD4` | 0 |

결과와 소스/원본 파일 SHA-256은 `crc_output/crc_results.json`에 있다. 이 검증은 실제 CRC peripheral, AHB 지연, 인터럽트 선점 타이밍 또는 실기 성능을 확인하지 않는다. 실제 장치에는 접근하지 않았다.

## 상위 작업의 실기 비교 진입점

장치 사용을 맡은 코드가 CRC의 다른 사용을 멈춘 뒤 아래와 같은 읽기 전용 입력으로 호출할 수 있다. 이 함수를 추가하는 것만으로 호출되거나 장치 검증이 수행되지는 않는다.

```c
static const uint8_t crc_probe[4] = {0x78, 0x56, 0x34, 0x12};
BSP_CRC_Comparison comparison;
BSP_CRC_Status status = BSP_CRC_CompareHardwareWords(
    crc_probe, sizeof crc_probe, &comparison);
/* 실기 합격 조건: status == BSP_CRC_OK,
 * comparison.software == comparison.hardware == 0xDF8A8A2B,
 * comparison.bytes == 4. status/두 CRC/clock 전후를 실제 로그에 남긴다. */
```

더 긴 불변 입력은 최대 4096 bytes까지 같은 API로 비교할 수 있다. **여러 독립 HW 호출의 결과를 마지막 값 하나로 이어 붙여 전체 이미지 CRC라고 해석하면 안 된다.** 전체 이미지의 스트리밍 CRC는 caller-owned 소프트웨어 context를 사용한다.
