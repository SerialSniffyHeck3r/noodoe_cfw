# Zephyr Classic SPP 대안 평가 — 2026-09-21

## 결론

**시험 빌드는 성공했다. 그러나 현재 Noodoe의 용량 절감을 위한 즉시 교체안으로는 채택하지 않는다.** 폰 1대 + SPP + 인증·암호화 구성에서도, 기본 프로그램 대비 증가분이 최적화 후 Flash **86,056B (84.04KiB)**, 정적 RAM 점유 범위 **44,452B (43.41KiB)**다. FreeRTOS 이식과 Noodoe/TI startup·본딩 저장 연동은 아직 포함하지 않는다.

이는 Zephyr 자체가 불가능하다는 뜻이 아니다. 공개 소스를 Classic 전용으로 재구성하고 FreeRTOS에 이식할 수는 있지만, 지금의 메모리 문제를 작은 변경으로 해결하는 방법이 아니다. 현재 제품 코드/빌드/펌웨어/벤치에는 변경을 가하지 않았다.

## 실제 빌드 결과

동일 Cube GNU Arm GCC 13.3.1, Cortex-M4F hard-float, STM32F429 compile target, 512KiB Flash / 192KiB SRAM. UI·사진·기존 APP 코드는 시험에 넣지 않았다. 아래는 linker와 ELF에서 읽은 정렬 포함 점유 범위다.

| 구성 | Flash | 정적 SRAM | 실제 LTO |
|---|---:|---:|---|
| 기본 Zephyr / `-Os` | 14,328B | 6,016B | 꺼짐 |
| SPP+보안 / `-Os` | 112,092B | 50,468B | 꺼짐 |
| 기본 Zephyr / `-Oz + LTO` | 12,652B | 6,016B | 켜짐 |
| SPP+보안 / `-Oz + LTO` | **98,708B** | **50,468B** | 켜짐 |
| SPP 추가분 / `-Oz + LTO` | **86,056B** | **44,452B** | 켜짐 |

순수 ELF 할당 section 합계는 LTO SPP Flash 98,704B / SRAM 50,351B다. 표와의 차이는 정렬·빈 공간이다. `results.json`에 합계/점유 범위/해시/.config를 모두 남겼다. RAM은 stack과 packet pool의 예약을 포함하며 런타임 최대 사용량 측정값은 아니다. 시험의 일반 heap 설정은 0이며 제품 FreeRTOS의 48KiB heap을 변경한 것이 아니다.

초기 `base-lto`/`spp-lto`는 cross-compile toolchain의 local-ISR-table 조건 때문에 LTO가 거절된 대조군이다. 실제로는 `-Oz`만 적용되어 Os와 같은 크기였다. 이를 LTO 결과로 보고하지 않는다. 최종 `base-gcc-lto`/`spp-gcc-lto`는 같은 GCC를 `gnuarmemb`로 선택하고 `CONFIG_ISR_TABLES_LOCAL_DECLARATION=y`를 사용했다. 최종 `.config`의 `CONFIG_LTO=y`와 ELF 생성 성공을 확인했다.

## 무엇이 큰가

아래는 `-Os` 최종 ELF의 Zephyr 공식 footprint 도구가 원본 소스에 귀속시킨 symbol 크기다. 상위 항목과 하위 항목은 중복되므로 합산하지 않는다. LTO는 함수를 합치거나 인라인화해서 소스별 귀속이 불안정하므로 분해 설명에는 비LTO ELF를 사용했다.

| 구성요소 | Flash 귀속량 | 설명 |
|---|---:|---|
| Bluetooth host 전체 | 66,592B | 공통 HCI/연결 관리 + LE + Classic |
| └ Classic 하위 부분 | 19,588B | RFCOMM/SDP/BR L2CAP/SSP 등 |
| └ LE `smp.c` | 10,130B | LE pairing/security |
| └ ATT + GATT | 13,444B | SPP만 원해도 현재 의존성에서 함께 남음 |
| TF-PSA-Crypto | 15,936B | AES/CMAC/P-256/PSA 관리 |
| Zephyr kernel | 7,694B | 전체 kernel 귀속량. 모두 BT 추가분인 것은 아님 |
| net_buf | 1,176B | buffer 관리 코드 |
| H4 driver | 1,248B | TI patch/startup 제외 |

`BT_CLASSIC`이 `BT_PERIPHERAL`, `BT_CENTRAL`, `BT_SMP`를 select하며 `host/CMakeLists.txt`가 관련 파일을 포함한다. 따라서 `BT_SMP=n`으로 안전하게 LE만 제거하는 구성이 아니다. Classic 전용 fork를 만들려면 HCI init, 연결 객체, 보안, key 저장 경계를 실제로 분리하고 회귀검증해야 한다. 현재 보이는 LE 코드 바이트를 그대로 전부 절약할 수 있다고 계산하지 않는다.

폰 한 대는 `CONFIG_BT_MAX_CONN=1`, `CONFIG_BT_MAX_PAIRED=1`로 반영했다. RFCOMM 데이터 MTU와 ACL 버퍼는 1024B, TX ACL 수는 기본값 3을 유지했다. 최대 동시 폰 수를 줄인다고 프로토콜 코드 자체가 비례해서 줄지는 않는다.

## TI/현재 프로젝트와의 비교 한계

- 앞서 측정한 TI 수정 SDK의 SPP+보안 core는 **73,694B**였다. 이것은 OS/UART/patch 미해결 symbol을 가진 vendor core이고, 이번 **86,056B**는 Zephyr 네이티브 프로그램의 전후 차이다. 두 값은 동일한 범위가 아니므로 정확한 우열·최종 제품 크기를 단순 뺄셈으로 확정하지 않는다.
- 현재 BTstack 32,640B는 함수 text 귀속량이고 별도 Noodoe Bluetooth 함수 9,342B가 있다. rodata·공유 helper까지 동일 경계로 산정한 값이 아니므로 32KiB 대 84KiB의 정확한 배율 비교도 하지 않는다.
- 기존 Product Release APP는 341,272B / 384KiB, 여유 51,944B로 목표 64KiB에 **13,592B** 부족하다. 이 목표는 새 stack을 단순 추가하는 것이 아니라 전체 Product 최종 ELF로 다시 평가해야 한다. 이번 시험은 목표 통과 증거가 아니다.
- 현재 결과에는 Zephyr를 채택하면 공간이 줄어든다는 근거가 없다. 무료 사용 조건을 우선하면 TI 이식과 APP 감량을 계속 검토하는 쪽이 현재로서는 더 예측 가능하다. 기존 BTstack을 유지하려면 별도 상업 라이선스 조건 확인이 필요하다. 어떤 계약도 체결하거나 문의를 발송하지 않았다.

## FreeRTOS/CC256x 적용성

H4 host 구조라 기존 CC256x와 연결할 출발점은 있다. 그러나 v4.4.2의 Bluetooth driver/host/sample/boards TI 경로에서 CC256x 전용 초기화 구현은 찾지 못했다. generic H4 driver는 TI service pack·enable/reset·baud sequence를 대신하지 않는다.

host archive에서 OS/버퍼/암호/settings 관련 외부 symbol **94개**를 추출했다. 이는 94개 모두를 새로 작성해야 한다는 뜻은 아니다. net_buf와 crypto는 함께 가져올 수 있지만, workqueue 취소/flush, thread/condvar/semaphore/mutex, timeout, 참조 수명, ISR 문맥은 FreeRTOS에서 의미가 유지되어야 한다. `PORTING.md`와 `results.json.host_external_port_symbols`에 경계를 기록했다.

현재 harness는 숫자 비교 확인/거절과 L3 인증·암호화, 최소 16-byte BR key 설정, SDP/SPP 송수신을 실제 코드로 링크했다. 자동 pairing 승인은 하지 않는다. `BT_SETTINGS` 직렬화 경로는 포함하지만 `SETTINGS_NONE`이므로 **전원 재인가 후 본딩 복원은 구현/검증하지 않았다**. 제품 적용 시 기존 StorageTask의 CFWCFG 저장 경로에 연결해야 한다.

## 검증 범위와 남은 항목

완료:

- upstream v4.4.2/의존 module commit 고정, 원본 source 무수정 확인.
- Os, Oz-only 대조군, 실제 Oz+LTO의 기본/SPP 총 6개 ELF 링크 성공.
- security/one-connection/H4 설정 assertion, ELF section/LMA 및 linker 수치 대조.
- Flash/RAM/소스별 footprint, host 외부 symbol, 재현 도구·로그·해시 기록.

아직 하지 않은 것:

- FreeRTOS shim 및 제품 통합, TI patch/reset/baud 구현, durable bonds/OTA/절전 연결.
- 실제 CC256x 무선 검색·pairing·암호화·SPP 송수신·장시간 부하·IGN 복귀 시험.
- 런타임 peak stack/RAM 검증, 최종 Product 메모리 목표 통과.

최종 LTO 링크에는 GNU ld의 RWX LOAD segment 경고와 serial LTRANS 안내가 남았다. 경고를 숨기지 않았으며 로그에 보존했다. 이 ELF는 메모리 크기 측정용으로만 사용하고 production memory-protection/runtime 검증을 통과했다고 보고하지 않는다. 특히 이 ELF는 0x08000000에 링크되어 있으므로 Noodoe에 쓰면 순정 BL 영역과 충돌한다. 이번에 장치 접근/다운로드는 수행하지 않았다.

## 라이선스·출처

Zephyr Classic/RFCOMM은 Apache-2.0이며 상업 사용 가능한 후보지만, **실험적 기능**으로 표시되어 있다. 전체 SDK 이름만으로 제품의 모든 파일 라이선스가 검증된 것은 아니다. 포함 파일의 SPDX/NOTICE, TI patch 재배포 조건은 제품 배포 시 별도로 반영해야 한다.

- [Zephyr v4.4.2 LICENSE](https://github.com/zephyrproject-rtos/zephyr/blob/v4.4.2/LICENSE)
- [Classic Kconfig — experimental/의존성](https://github.com/zephyrproject-rtos/zephyr/blob/v4.4.2/subsys/bluetooth/host/classic/Kconfig)
- [Zephyr host CMakeLists](https://github.com/zephyrproject-rtos/zephyr/blob/v4.4.2/subsys/bluetooth/host/CMakeLists.txt)
- [Zephyr cross compiler 설정](https://docs.zephyrproject.org/latest/develop/toolchains/other_x_compilers.html)

측정 파일: `results.json`, `component-size-os.json`, `build/spp/{rom,ram}.json`, `build/spp-gcc-lto/{rom,ram}.json`. 이식 계획: `PORTING.md`. 재현 명령: `README.md`.
