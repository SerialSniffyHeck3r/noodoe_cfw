# Zephyr Classic SPP 크기 시험

사용자 승인한 대안 조사용 독립 프로젝트다. 제품 CFW에 설치하거나 Cube 프로젝트에 추가하지 않는다. ELF는 Discovery의 startup/clock을 쓰며 주소 0x08000000에서 링크하므로 **Noodoe에 다운로드하지 않는다.** 내부 Flash 용량만 실제 장치와 같은 512KiB로 제한했다. SRAM은 192KiB, CCM은 미사용이다. 외부 SDRAM/NOR 실행은 하지 않는다.

## 재현

기준 소스는 `source-lock.json`, Python 패키지는 `python-requirements-lock.txt`로 고정한다. `upstream/zephyr`는 v4.4.2다. `upstream/.west`에서 `west update --narrow -o=--depth=1 cmsis cmsis_6 hal_stm32 mbedtls tf-psa-crypto`로 모듈을 받았다.

현재 디렉터리에서:

```powershell
& .\venv\Scripts\python.exe .\build_eval.py base
& .\venv\Scripts\python.exe .\build_eval.py spp
& .\venv\Scripts\python.exe .\build_eval.py base-gcc-lto
& .\venv\Scripts\python.exe .\build_eval.py spp-gcc-lto
& .\venv\Scripts\python.exe .\measure_eval.py
```

`base-lto`와 `spp-lto`는 초기 cross-compile 설정이 LTO를 거절한 것을 보존한 대조군이다. 실제로는 `-Oz`만 적용됐다. 최종 LTO 결과는 반드시 `*-gcc-lto`에서 읽는다. 측정 도구가 최종 LTO/IRQ 설정을 assertion으로 확인한다. Cube GCC 13.3.1을 계속 쓰며 GNU Arm toolchain 선택 방식만 변경했다.

`build/<variant>/build.log`, `.config`, `zephyr.map`, `zephyr.elf`가 각 결과의 근거다. `results.json`은 ELF section과 LMA를 이용해 Flash/RAM 합계와 정렬 포함 span을 기록한다. Flash는 .data 초기값도 포함하고 .bss/.noinit은 제외한다. RAM은 정적 packet pool과 stack 예약을 포함한다. 실행 중 stack high-water/최대 힙 사용량을 측정한 것은 아니다.

## 들어간 기능

- ACL 연결 1개, 본딩 슬롯 1개, SDP Serial Port record, RFCOMM 서버 1개.
- SPP 송수신 실제 코드 경로. 앱 프로토콜/OTA/parser/GUI는 없음.
- `BT_SECURITY_L3`, 인증+암호화, BR 최소 key size 16, 숫자 비교 확인/거절 콜백. 자동 페어링 승인 없음.
- H4 UART/RTS/CTS 드라이버와 STM32 RNG, Zephyr kernel/net_buf/settings/PSA crypto.
- `BT_SETTINGS` 직렬화 포함. `SETTINGS_NONE`이므로 **본딩의 영구 저장은 미구현**. 이 시험은 durable-storage 평가가 아니다.

## 아직 들어가지 않은 것

Noodoe용 TI enable/reset/patch/baud 초기화, FreeRTOS 이식 계층, 기존 CFW 설정 저장 연동, OTA 보안·복구 계약, IGN/절전 수명 관리. `PORTING.md`에 경계를 기록했다. 시험 소스의 debugger mailbox는 코드가 링크되게 만드는 harness이며 완성된 동시성/제품 API가 아니다.

## 라이선스와 원본

Zephyr Classic source는 Apache-2.0이며 이 시험에 복사한 SDP record는 NXP 원 저작권 표기를 유지한다. `app/src/spp_record.inc`는 v4.4.2 `tests/bluetooth/classic/rfcomm_s/src/rfcomm_s.c`의 service-record 선언을 그대로 가져왔다. Mbed TLS와 TF-PSA-Crypto의 LICENSE는 Apache-2.0 OR GPL-2.0-or-later 선택형이며 이번 후보는 Apache-2.0 경로로 평가한다. 제품 배포 시에는 실제 포함 파일의 개별 SPDX, 저작권·NOTICE, TI service pack 조건도 별도로 유지해야 한다. Bluetooth qualification은 이 소스 라이선스와 별개다.

Upstream source를 수정하지 않았으며 펌웨어 코드나 사용자 자료를 외부에 업로드하지 않았다.
