> 2026-09-21: BL0.15/SR0701 호환 구현은 [BL_COMPATIBILITY.md](BL_COMPATIBILITY.md)를 따른다. 아래의 0.14 분석 기록은 당시 원본의 관측값이며, 현재 writer는 0.14/0.15의 word0을 그대로 보존한다.

# 순정 BL용 APP 설치 요청 기록기

2026-09-12. 이 문서는 로컬 순정 BL 정적 분석, 새 C 구현, ARM 모델 시험을 구분한다. 실제 장치에 메타데이터를 쓰거나 OTA 설치를 실행한 결과가 아니다.

## 공개 연결 계약

추가 소스는 `Middlewares/Noodoe/Update/src/Update_Metadata.c`, include는 `Middlewares/Noodoe/Update/inc`다. 제품에서는 `UPDATE_METADATA_TESTING`을 정의하지 않는다. Core/링커/프로젝트 빌드 파일을 이 작업에서 수정하지 않았다.

```c
uint32_t words[5];
UpdateMetadataResult result = UpdateMetadata_Read(words);

/* 통합 계층이 BSP_RAM_Allocate(UPDATE_METADATA_SECTOR_SIZE) 등으로 미리 확보.
 * NULL이면 commit을 호출하지 않는다. allocator의 검증된 SDRAM만 전달한다. */
result = UpdateMetadata_Commit(image_version, verified_staging_crc,
    UPDATE_METADATA_ARM_TOKEN, owned_scratch, UPDATE_METADATA_SECTOR_SIZE);
```

`UpdateMetadata_Commit(version, crc, arm, scratch, scratch_bytes)`은 동기 호출이다. scratch는 최소 16KiB, 4바이트 정렬, 호출 동안 독점 소유한 SRAM/검증된 SDRAM이어야 한다. 주소창 검사 자체가 SDRAM 실용량/초기화 검증을 대신하지 않는다. 내부 SRAM의 16KiB 정적 배열은 없다. `-Os` 제품 object 기준 RAM 실행 코드 1,092바이트, 정적 상태 56바이트다. caller의 scratch와 작은 호출 스택은 별도다.

caller는 먼저 APP staging `[0x07F90000,0x08000000)`의 **전체 448KiB**를 기록·읽기 검증하고 이미지 벡터/길이/CRC를 검증해야 한다. 이 기록기는 NOR에 접근하지 않으므로 arm 상수는 이미지 검증이나 인증을 대체하지 않는다. commit 동안 staging, scratch 및 내부 FLASH의 다른 writer를 배제한다. 이미 처리 중인 통신/DMA를 정리하는 일도 상위 계층의 소유다. 성공 후에도 BL이 소비할 때까지 staging을 덮어쓰지 않는다.

호출은 privileged Thread, PRIMASK/BASEPRI/FAULTMASK=0에서만 허용한다. ISR 호출과 재진입을 거절한다. 시작부터 종료까지 IRQ가 차단되므로 UI/BT/USB 서비스와 RTOS tick이 잠시 멈춘다. 타임아웃은 HAL tick에 의존하지 않는다. 실제 처리 시간과 통신 손실은 모델 시험으로 검증하지 않았다.

## 기록과 정적 근거

| 주소 | 새 기록값 |
|---|---|
| `0x08008000` | resident `0x000E0000` |
| `0x08008004` | caller의 image version, `FFFFFFFF` 제외 |
| `0x08008008` | APP NOR block `0x7F90` |
| `0x0800800C` | 고정 full APP 길이 `0x70000` |
| `0x08008010` | caller가 검증한 CRC, `0`/`FFFFFFFF` 제외 |

현재 resident가 `0x000E0000`이고 CRC가 정확히 0일 때만 갱신한다. CRC가 0이면 version/slot/length가 이전 값으로 남아 있어도 허용한다. 순정 BL의 성공 처리가 CRC만 0으로 만들기 때문이다. erased CRC=`FFFFFFFF`나 다른 resident를 기록기가 임의 정상화하지 않는다.

이 형식은 [metadata 정규화와 APP gate](../../../../analysis/2026-09-11-update-failure-contract/boot-failure/metadata-normalization.asm.txt)의 `0x20004678`, `0x20004782`에서 확인했다. 후자는 비영 CRC/비영 길이/APP slot을 검사한다. BL이 CRC 수학 검증을 대신해 준다고 가정하지 않는다.

순정 APP writer [0x080426D4](../../../../analysis/2026-09-11-update-failure-contract/mcu-app/metadata-sector-writer.asm.txt)는 sector를 RAM에 읽고 IRQ를 막아 갱신한다. 새 구현은 이 코드를 복제하지 않고 **S2 전체 16KiB 보존과 읽기 검증을 명시적으로 수행**한다. 순정 writer의 기록 종료 경계는 `+0x1000`이므로 전체 16KiB를 온전히 복원한다고 해석하지 않는다.

순정 BL의 [성공 정리 0x20004AA4](../../../../analysis/2026-09-11-bootloader-re/full-flash-update/metadata_result.asm.txt)는 CRC를 0으로 바꾸고 **S2 전체를 지운 뒤 처음 20바이트만 다시 쓴다**. 따라서 새 기록기가 보존하는 S2의 나머지 바이트도 BL 설치 성공 뒤에는 지워질 수 있다. **새 설정/저널을 S2에 배치하지 않는다.** 승인된 설정 영역은 외부 NOR `[0x07F70000,0x07F80000)`다.

## transaction 순서

1. S2 `[0x08008000,0x0800C000)` 전체를 caller RAM으로 읽고 다시 비교한다. 20바이트 밖의 보존 영역에는 별도 CRC32/IEEE도 계산한다.
2. 현 resident/CRC와 MCU group ID `0x419`/512KiB, FLASH idle/locked, WWDG 비활성을 검사한다. 다른 writer가 이미 FLASH를 unlock했다면 건드리지 않는다.
3. FLASH를 unlock한 뒤 **sector 번호 2 하나만** erase한다. MER1/MER2는 설정하지 않는다. 전체 erased readback을 검사한다.
4. CRC 워드를 erased 상태로 남기고 prefix 4워드와 20바이트 밖의 모든 데이터를 복원한다. `FFFFFFFF` 워드는 program을 생략한다.
5. 전체 워드 비교와 보존 CRC를 검증한다. 이 단계에서 CRC 워드는 반드시 `FFFFFFFF`다.
6. CRC 워드를 마지막 32-bit program 한 번으로 기록하고 다시 sector 전체를 비교한다. FLASH를 lock하고 LOCK readback을 확인한다.

erase/program 주소는 코드에서 S2로 제한한다. sector0/1/3, APP, 옵션 바이트, 외부 BL staging, reset, GPIO 전원 제어를 수행하는 API는 없다. 성공은 **설치 요청 readback 완료**이며 실제 BL 설치나 새 APP 부팅 완료가 아니다.

## RAM 실행·전압·실패 한계

FLASH busy 동안 실행하는 함수와 literal은 `.RamFunc.update_metadata`에 둔다. 현재 `Noodoe_APP.ld`의 `.data`에는 `.RamFunc*`가 포함되어 startup의 RAM 복사 대상이다. commit 전에 transaction 함수 주소가 실행 가능한 내부 SRAM에 있는지도 확인한다. 모델 시험은 제품 backend를 별도 object로 컴파일하여 RAM section에서 FLASH/libc/HAL로 나가는 relocation이 없음을 검사했다. 최종 통합 ELF의 배치/복사/실행은 별도 확인해야 한다.

제품 backend는 FLASH peripheral status를 유한 횟수만 poll한다. erase는 최대 20,000,000회, program은 2,000,000회다. 이는 고정 밀리초가 아니다. 대기와 긴 CRC 계산에서는 기존 IWDG에 `0xAAAA` reload만 쓰며 watchdog 시작/옵션 변경은 없다. 활성 WWDG는 지원하지 않는다.

STM32의 single-bank flash 접근은 busy 동안 stall될 수 있다. x32 program은 안정적인 MCU VDD 2.7~3.6V가 전제이며 이 코드가 전압을 측정하거나 변경하지 않는다. cache의 이전 S2 데이터로 검증하지 않도록 각 완료 뒤 FLASH data cache를 비우고 enable 상태를 복원한다. 근거: [ST RM0090, Embedded flash memory interface](https://www.st.com/resource/en/reference_manual/rm0090-stm32f4xx-reference-manual-stmicroelectronics.pdf).

**유한 polling은 영구 BSY 고착에 대한 wall-clock 복귀 보장이 아니다.** 고착 시 CR 쓰기나 FLASH caller 복귀 자체가 stall될 수 있다. BSY=0인 정상/오류 완료 경로는 항상 mode clear/LOCK 설정/검증을 수행하지만, BSY가 남으면 CR 쓰기를 강행하지 않고 `AMBIGUOUS`, `poll_exhausted`, lock 미확인 상태를 남긴다. 따라서 모든 물리 고장에서 relock/반환을 보장한다고 주장하지 않는다. NMI/fault는 PRIMASK로 막히지 않으며 SRAM vector로 이관하는 기능도 추가하지 않았다.

sector erase와 마지막 CRC store는 전원 중단에 원자적이지 않다. 특히 부분 erase 또는 부분 CRC store가 남으면 BL이 비영 CRC를 요청으로 볼 수 있다. **erase가 시작된 뒤의 모든 실패는 `UPDATE_METADATA_AMBIGUOUS`**다. 자동 재시도 erase나 자동 reset을 하지 않으며 같은 부팅의 두 번째 commit은 `UPDATE_METADATA_REVIEW_REQUIRED`로 거절한다. 상위 계층은 실패 뒤 자동 재부팅/전원 종료/새 OTA를 막고 SWD로 상태를 확인해야 한다. 전원 자체가 사라지는 상황까지 소프트웨어가 막지는 못한다.

## 검증 결과

재현: `tools/tests/protocol_services/metadata_run.py`. 결과는 [metadata_output/results.json](../../tools/tests/protocol_services/metadata_output/results.json)에 source SHA-256과 함께 보관한다.

- 실제 엔진 C를 Cortex-M4 ELF로 만들어 Unicorn에서 `-O0`, `-Os` 각각 **4,193 assertions 통과**.
- full sector 패턴/erased-word 보존, S0/S1/S3 불변, CRC-last와 사전 readback, 현재 CRC=0+이전 slot/length 허용, arm/인자/context/pending 거절, 재진입 및 두 번째 erase 차단을 검사했다.
- 두 번째 read 불일치, unlock 실패, erase 실패/부분 erase, 앞·뒤쪽 program 실패, 사전/최종 readback 오류, CRC 쓰기 실패/부분 쓰기, lock 실패를 주입했다.
- 실제 CMSIS target backend는 `-Wall -Wextra -Werror`로 두 설정 모두 컴파일했다. RAM section relocation은 내부 `.bss` 상태만 참조했다.

flash/IRQ/BSY는 모델이며 실물 erase 시간, 전압 강하, flash cache, watchdog, NMI, SDRAM/DMA 동시성, 전원 중단, 통합 ELF와 실제 OTA는 이 시험으로 검증하지 않았다. 원본 BIN/APK/덤프는 수정하지 않았다.
