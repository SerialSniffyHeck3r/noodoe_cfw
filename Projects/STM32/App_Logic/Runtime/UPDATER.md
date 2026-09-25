# RuntimeUpdate의 저장 표현과 실행 계약

`RuntimeUpdate_Init()`은 StorageTask에서 검증된 RAM/NOR 초기화 뒤 호출한다. 0이면 성공이며 서비스 객체와 전용16KiB 메타 scratch를 하나의 독점 SDRAM 할당에서 확보한다. `RuntimeUpdate_GetService()`는 release/acquire 게시 완료 전에는 NULL이다. `RuntimeUpdate_Process(now_ms)`만 SPI5·메타·reset callback을 실행한다. Init 성공 후 재호출로 세션을 지우지 않으며 기본 authorization은0이다.

## 세 가지 위치와 바이트 표현

| 위치/API | 바이트 표현 |
|---|---|
| PC APP 파일, NDCP DATA, UpdateService의 SHA/CRC/vector, READ_STAGE 응답 | canonical APP byte 순서 |
| `BSP_NOR_Read/OTAProgram`, StorageSWD 및128MiB A/B 파일 | NOR physical wire byte 순서 |
| 순정 BL의 SPI16 DMA 수신 RAM | physical byte-pair를 교환한 순서; 설치할 canonical APP여야 함 |

OTA adapter만 `[0x07F90000,0x08000000)`에서 `physical_address = logical_address XOR1` 변환을 한다. 논리 홀수 선두와 짝수 꼬리1byte는 각각 대응 물리 주소에만 접근한다. 짝수 중간은 pair-swap해서 전송한다. 홀수 길이 DATA가 이어져도 이웃 byte를 읽어 다시 쓰지 않는다. 256-byte page와4KiB sector 경계는 짝수라 변환이 다른 page/sector로 넘지 않는다. 각 물리 program 직전에 연결 epoch·authorization·transaction을 재검사한다. 이미 BSP로 넘긴 물리 operation 하나는 취소할 수 없다.

raw BSP/백업/FS/NVM에 전역 swap을 적용하지 않는다. 원본 A/B 파일도 변환해 덮어쓰지 않는다. **메타데이터의 CRC와 전송 SHA는 canonical 이미지에 대해 계산한다.** physical raw CRC로 대체하면 안 된다. `READ_STAGE`는 canonical 비교·중복 전송 검증용이며 raw NOR 백업 API가 아니다.

이 변환은 파일시스템 서명 추측만으로 추가하지 않았다. 실물 A의 physical APP448KiB를 pair-swap하면 기존 원본 V516+마지막FF4와 전 바이트가 일치했다. 순정 BL의 `0x20005ED6..DC`는 bulk callback `0x200033E1`을 선택하고, `0x20003480`에서 SPI16, `0x2000348C`에서 length/2, `0x20003492..9C`에서 원래 목적지에 halfword DMA를 수행한다. APP installer는 받은 RAM bytes를 그대로 쓴다. [A 관측 근거](../../../../analysis/2026-09-12-integrated-bringup/storage-layout-observation.md)는 작성 시점 A-only이며 전체 A/B 검증 완료와 구분한다.

## 승인·메타데이터·재시작

IOTask 하나가 SetConnected/Authorize를 직렬화한다. 전체 NOR A/B 검증에 근거한 명시 승인 전에는 Begin token만으로 쓰기 권한이 열리지 않는다. 재연결 epoch가 바뀌면 이전 DATA의 다음 물리 조각은 거절한다. 작은 전송 표본의 `transport_verified`를 전체 NOR 백업 완료로 해석하지 않는다.

COMMIT은 canonical448KiB의 SHA/CRC/vector 검증 완료 후에만 실행한다. adapter는 외부 COMT를 확인한 뒤 내부 writer에 UMC2 token과 전용16KiB scratch를 넘긴다. 첫 erase 전에 BT owner의 quiesce를 최대1000ms 기다리며 실패하면 writer를 호출하지 않는다. 대기 후 연결 epoch/ready를 다시 확인한다. 성공한 quiesce 뒤에는 writer의 모든 반환 경로에서 resume를 요청하고 writer/resume 오류를 성공으로 숨기지 않는다. GPIO·BT 레지스터를 StorageTask가 직접 조작하지 않는다.

RESET은 별도 요청이다. ACK의 전체 local 전송 완료 통지 후1500ms와 동일 epoch를 확인하고 실제 pending metadata가 해당 version/CRC인지 다시 읽는다. 메타 기록 실패에서 자동 erase 재시도나 자동 reset을 하지 않는다. BT pause가 물리 계기판 UART 입력까지 무손실로 보존하는 것은 아니다.

## 검증 범위

최신 실제 ARM C 모델은 O0/Os 각각45,529 checks와 제품 CMSIS compile을 통과했다. 물리 NOR 대역은 단순 raw byte 저장이며 codec을 대신 구현하지 않는다. 홀수 조각·재전송·READ_STAGE·page/sector/staging 끝·이웃 byte 보존·분할 중 epoch 변경과 기존 메타/BT pause/reset guard를 포함한다. 전체448KiB physical image가 독립 pair-swap 기대값과 같고 stock-decode 결과도 canonical 전체와 일치한다.

[시험 설명](../../tools/tests/protocol_services/runtime_update_README.md)과 [결과/해시](../../tools/tests/protocol_services/runtime_update_output/runtime_update_results.json)를 따른다. 이는 실제 부트로더 설치나 전원 차단 시험 완료가 아니며 최종 장치 readback·새 APP 기동 확인은 별도다.
