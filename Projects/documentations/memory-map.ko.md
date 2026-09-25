# 메모리 지도와 자산 배치

이 표는 **조사한 STM32F429IE 계열 누도와 현재 CFW 링커 계약**에 대한 것이다. 그렇지만 noodoe 시스템은 오랫동안 변하지 않았고, 확보한 여러 개의 순정 펌웨어 파일이 대부분 비슷한 구조인 것을 보아 큰 차이는 안 날 것이라고 추정한다. 표에서 0x0800…과 0x2000…은 MCU 주소이고, 0x07F…로 적은 외장 NOR 값은 칩 내부 오프셋이다. [English](memory-map.md)

## MCU 내부 플래시와 RAM

| 주소 | 크기 | 사용 목적 |
|---|---:|---|
| 0x08000000–0x0800FFFF | 64KiB | 순정 상주 부트·설치 코드, 공장 관련 보존 데이터. 여기에는 차량 고유 번호, 해당 Noodoe가 설치된 차량의 모델명, 보드 리비전 등을 포함하고 있다. KYMCO의 서버를 거쳐 이 고유 번호는 차량의 VIN으로 변환된다. |
| 0x08010000–0x0801FFFF | 64KiB | 독립 RecoveryGate. 원래 APP 영역의 첫 소거 섹터를 사용한다. |
| 0x08020000–0x0807FFFF | 384KiB | 평소 실행하는 Product CFW. 일반 업데이트는 이 부분만 교체한다. |
| 0x20000000–0x2002FFFF | 192KiB | 일반 SRAM. DMA 버퍼, 48KiB FreeRTOS 힙, 스택·큐·진단 등에 사용한다. 끝 256바이트는 Gate/Product 공유 mailbox용이다. |
| 0x10000000–0x1000FFFF | 64KiB | CCM. 보호 경계가 있는 LVGL 48KiB 풀을 둔다.  |

Gate와 Product를 합친 CFW의 내부 APP 영역은 448KiB다. 

## 외장 NOR: 순정 FAT 안의 소유 공간

조사한 보드는 128MiB NOR와 64MiB SDRAM을 쓴다. NOR는 전원이 꺼져도 남고, SDRAM은 작업 메모리다. 순정 NOR에는 FAT 파일과 업데이트용 예약 공간이 있다. CFW는 빈 클러스터의 소유권, FAT 사본, 예약 영역과의 충돌을 확인한 후 **이름이 있는 고정 크기 파일**을 만든다. 임의로 추측한 RAW 오프셋에 설정을 쓰지 않는다. NOR 오프셋 0x07F80000–0x07FFFFFF는 순정 업데이터를 위해 보존한다.

| CFW가 소유하는 파일 | 용량 | 내용 |
|---|---:|---|
| NOODOE.RSC | 1MiB | 폰트 비트맵과 CC256x 패치 자산. 검증·활성화를 위한 512KiB 두 뱅크이다. |
| CFWCFG.DAT | 128KiB | 형식 버전이 있는 설정·페어링 정보 순환 저널. |
| CFWRIDE.DAT | 256KiB | 트립·정비 주기·시동시간 체크포인트. |
| CFWPIC.DAT | 1MiB | 사진 세 슬롯과 각 사진의 교체 안전 공간. |
| CFWREC.DAT | 512KiB | 버튼 복원 때 사용하는 정확한 순정 APP. |
| CFWA.DAT · CFWB.DAT | 각 512KiB | 검증된 Product A/B 후보와 이전 정상본. 헤더·384KiB 본체·식별 기록 포함. |
| CFWBOOT.DAT | 64KiB | 설치·부팅 상태 저널. |
| CFWLOG.DAT | 256KiB | 크기를 제한한 기기 이벤트 로그. |
| CFWTEXT.DAT | 버전별 선택 | 이름·문자 캐시. 존재와 형식은 기능 협상·파일 감사로 판단. |

여기서 **A/B는 외장 NOR에 보존하는 두 개의 Product 후보**다. MCU 내부에 384KiB 실행 파티션 두 개가 있는 것은 아니다. 내부 0x08020000에서 실행하는 Product는 하나이며 Gate가 검증된 NOR 이미지 하나를 그 주소에 복사한다. 순정 사진과 순정 FAT 파일은 별도로 남는다. Gate로 순정 APP만 복원하면 CFW 파일은 보통 그대로이고, 데이터까지 지우는 작업은 별도로 하는 모드가 따로 있다.

내부 플래시에는 폰트 문자 매핑·메트릭을 남기고 큰 비트맵·Bluetooth 패치는 외장 자산 파일에서 검증 후 SDRAM에 올린다. UI가 글자를 그릴 때마다 NOR 파일을 실시간으로 읽는 방식이 아니다. SDRAM에는 사진 디코딩, 자산, 480×480 RGB565 캡처 460,800바이트, 설치 작업 공간을 각각 용도별로 한 번 할당해 재사용한다.

## EVE 그래픽 RAM

FT81x의 RAM_G는 1MiB다. 현재 그래픽 배치는 앞부분 약 96KiB를 폰트·아이콘·정렬 여유에, 사진 두 BANK에 각각 450KiB를, 480×24행 부분 캡처에 약 22.5KiB를 책정한다. 두 BANK 가 있어 사진 A에서 B로 천천히 바꿀 때 현재 보이는 텍스처를 덮어쓰지 않을 수 있다. 전체 RGB565 화면 캡처는 GPU에 별도 전체 화면 버퍼를 오래 붙잡아 두지 않고 24행씩 읽어 SDRAM에서 조립한다. 디스플레이 리스트가 실제로 교체되었는지 확인하기 전에는 참조 중인 메모리를 재사용하지 않는다.

근거: [링커](../STM32/Linker), [Gate ABI](../STM32/RecoveryGate/include/gate_abi.h), [메모리 계약](../STM32/MEMORY.md), [자산 목록](../STM32/Resources/manifest.json), [NOR 저장 코드](../STM32/Middlewares/Noodoe/Storage/src/bootstrap_storage.c), [FT81x 데이터시트](https://www.brtchip.com/wp-content/uploads/Support/Documentation/Datasheets/ICs/EVE/DS_FT81x.pdf).
