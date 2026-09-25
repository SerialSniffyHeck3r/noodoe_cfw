# 메모리 배치와 자산 설치 계약

2026-09-16 구현. 이 문서는 이전 단일 사진 bank/전체 RAM_G 캡처 예약 및 Product LVGL32KiB SRAM 규칙을 대체한다. 수치의 최종 판정은 각 빌드의 `memory-report.json`과 실기기 시험 기록이다.

## 내부 메모리

| 영역 | 용도 | 계약 |
|---|---|---|
| 0x08000000..0x0800FFFF | 순정 BL·설치 메타데이터·기기 정보 | 이번 변경에서 쓰지 않음 |
| 0x08010000..0x0807FFFF | APP448KiB | Release 여유64KiB/Debug32KiB를 ELF로 강제 |
| APP+0x200 | ResourceRequirement44B | 형식·required·자산 SHA256, linker KEEP |
| 0x20000000..0x2002FFFF | 일반 SRAM192KiB | DMA·RTOS48KiB 힙·태스크 스택·진단·mailbox, 여유32KiB |
| 0x10000000..0x1000C03F | CCM | 32B guard + LVGL48KiB +32B guard |
| 0x1000C040..0x1000FFFF | CCM 여유 | 16,320B, DMA 금지 |

`Linker/Noodoe_APP.ld`의 `.ccm_bss`는 NOLOAD다. 프로젝트 소유 `bsp_reset.s`가 `BSP_CCM_Init`으로 CCM 클록을 켜고 해당 섹션만 지운 다음 libc 초기화를 한다. Cube startup과 vendor 원본은 수정하지 않는다. STOP 복귀에서는 이 초기화를 재실행하지 않는다. `GraphicsMemory_Check()`가 guard를 검사한다.

`BSP_RAM_DMAAccessible()`은 CCM 주소를 거부한다. 외부 포인터를 받는 BT HCI와 RAM DMA 시험 포트에 이 검사를 적용한다. 계기판 UART/NOR/DMA 초기 시험은 기존 일반 SRAM 고정 버퍼를 사용한다. EVE 폰트 전송은 CPU SPI다. 나중에 EVE를 DMA로 바꿀 때 CCM의 임의 LVGL 포인터를 직접 넘기지 않는다.

FreeRTOS 힙48KiB, 기본 태스크12KiB, 통신 큐와 두 폰+OBD 자원 수는 줄이지 않았다. 링크 여유와 이미 예약된 RTOS/LVGL 풀의 내부 여유는 합쳐 계산하지 않는다. 실제 RTOS 최소12KiB·29FPS·CPU/지연은 ELF만으로 증명할 수 없다.

## 외장 SDRAM과 자산

`BSP_RAM_AllocateNamed(owner, bytes)`는 같은 owner/같은 크기에 같은 주소를 돌려주고 다른 크기는 거부한다. `g_bsp_ram_allocations[32]`는 owner/address/rounded bytes/caller PC를 기록한다. 기존 owner0 할당은 caller를 ELF에서 찾아 용도를 확인한다. 범용 malloc의 위치는 바꾸지 않는다.

| 고정 용도 | 최대/예약 bytes | 수명 |
|---|---:|---|
| 검증된 폰트·BT 자산 | 524,288 | 부팅부터 리셋까지 불변 |
| 설치/호환성 검증 작업 공간 | 552,960 | 비활성 슬롯512KiB + FAT8KiB + root16KiB + 작업4KiB, 반복 재사용 |
| 완성 캡처 RGB565 | 460,800 | capture generation별, PC 다운로드 때 화면과 독립 |
| 캡처 display list 기록 | 4,096 | 동일 generation의 고정 프레임 |
| 기존 SWD 저장소 백업 공간 | 8MiB | 기존 유지 |
| 기존 사진3슬롯·JPEG worker·업데이트 공간 | 기존 유지 | 서비스별 한 번 할당 |

512KiB 자산 풀과 설치 작업 공간은 구분한다. 후자는 사용 중인 폰트 주소를 바꾸지 않고 비활성 슬롯을 검증하기 위한 별도 예약이다. 자산 이관으로 늘어난 SDRAM 예약 합계는1,542,144B다.64MiB 보드에서 기존 최대로 로드된 세 사진 기준 예상 여유는 약53MiB이며, 실제 주소/수치는 SDRAM 진단으로 확인한다.

`Resources/manifest.json`에는 모든10개 크기의 Lato/D-DIN 비트맵과 CC256xB/C 두 패치의 CRC/SHA가 있다. 총81,587B(폰트65,584B+패치16,003B)이며 원본을 다시 래스터화하거나 압축하지 않았다. 문자 매핑/메트릭과 `lv_font_get_bitmap_fmt_txt` 함수 포인터는 내부에 유지한다. `Product_TextFont()`/`Product_NumberFont()` API는 동일하다.

## 비동기 부팅과 오류

- `Resources_RequestLoad()`는 읽기 요청만 기록한다. READY에서는 불변 주소를 그대로 유지한다.
- `Resources_GetStatus()`로 상태를 확인하고 READY일 때만 `Resources_Get(id,&view)`를 사용한다.
- StorageTask가 한 번에 최대4KiB 파일 읽기/해시 작업을 수행한다. UI/BT 태스크에서 파일을 읽지 않는다.
- `ProductBoot_Run`은 Runtime/UART/IGN/진단과 앱 모델을 먼저 시작한다. 자산 검증→font descriptor 연결→LVGL/UI 생성 순서다. `main` 태스크 본문은 계속 `LCDTest();` 한 줄이다.
- BT 시작은 자산 READY이며 시스템 오류가 없을 때만 허용한다. 제조사/revision 선택과 두 패치 지원을 유지한다.
- `SystemError_Report(code,detail)`는 첫 오류와 최근8개 고정 기록을 남긴다. 일반 오류 화면은 EVE ROM 폰트/기본 명령만 사용하며 LVGL·자산 SDRAM·동적 할당에 의존하지 않는다.
- `g_system_error.retry_request`를 새 값으로 올리면 읽기/검증만 재시도한다. recovery1=검사,2=검증 성공·재시작 필요. 자동 정상 UI 복귀/포맷/재기록/무한 리셋은 없다.
- 정상 task 문맥의 malloc 실패와 LVGL assertion은 오류 화면 경로를 사용한다. HardFault/스택 파손/IRQ 문맥은 기존 정지·SWD fault 기록 정책을 유지한다. CPU가 실행 불능이면 화면을 보장하지 않는다.

## NOODOE.RSC 설치

1MiB 고정 FAT 파일 안에512KiB A/B 슬롯을 둔다. 각 슬롯은4KiB 헤더와 원본 payload이며 완료 word는 마지막에 기록한다. 헤더CRC, 각 자산CRC, 테이블+payload SHA를 검사한다. 이 SHA는 APP와 자산의 정확한 호환성/손상 검사다.

`tools/resource_pack.py --check`는 생성된 package/header가 소스와 일치하는지 검사한다. 폰트를 다시 생성했다면 `tools/product_fonts.py` 뒤 `tools/resource_pack.py`를 실행하고 결과를 함께 보존한다.

`tools/resource_install.py plan --backup <verified-full-NOR-AB-directory> --output <new-directory>`는 오프라인 변경 계획만 만든다. 모든 FAT 디렉터리/파일 chain, 두 FAT 복사본,32개 연속 free cluster와 금지 영역을 검사한다. 성공 시 전체 메타데이터와 컨테이너의 before/after 원본·해시를 보존한다. `execute`는 명시적 serial/현재 ELF를 추가로 요구하며 동일 기기의 live precondition, 입력 readback, 기기 독립 검사, 최종 영역 readback을 수행한다.

최초 생성은 FAT 갱신 중 전원 차단에 원자적이지 않다. 검증된 전체 A/B 백업과 변경 영역 기록 없이 설치하지 않는다. 생성 후 크기/FAT chain은 고정이다. 갱신은 현재 자산 슬롯을 거부하고, 보존 슬롯의 유효성도 다시 검증한 후 비활성 슬롯만 쓴다. 기존 사진·stock 파일·staging·NVM에는 쓰지 않는다.

업데이트 APP는 고정 위치의 요구 자산 레코드를 가진다. RuntimeUpdate는 현재/비활성 완료 슬롯 중 요구 SHA와 일치하는 것을 비동기로 검증해야 COMMIT을 허용한다. 이미 화면에서 사용하는 SDRAM 주소는 갱신 직후에도 바뀌지 않으며 새 패키지는 재부팅 후 선택된다.

**2026-09-16 donor FAT 복구:** 최초 순정 USB 취득본에도 FONT 디렉터리 본문 손상, FREE로 끊긴 체인, 중복 클러스터, 잘린 resource_config.json과 FAT 예약 엔트리 오류가 존재했다. 단일 FONT 예외로 우회하지 않는다. `tools/stock_fat_repair.py`가 독립 전체 NOR A/B와 검증된 복구 자료로 전체 할당 그래프를 다시 만들고, 모든 읽히던 파일 및 복구 파일의 해시를 확인한다. 최초 손상을 일으킨 프로그램/시점은 증명하지 못했다.

복구는 기존 비어 있지 않은 데이터 섹터를 덮어쓰지 않는다. 새 디렉터리와 필요한 사본은 원래 균일한 0/FF인 FREE 공간에만 기록하며, 미분류 데이터는 `RECOVERY/ORPHANS.BIN`과 전체 원본 백업에 보존한다. `RECOVERY/BEFORE.BIN`에는 이전 FAT/root가 있다. 원래 FONT 내용은 검증 가능한 자료가 없어 복구하지 못했고 정상적인 빈 디렉터리로 만들었다. 이것은 원래 순정 파일 전부를 복원했다는 뜻이 아니다.

`ResourceRepair`는 StorageTask의 명시적 command3/BAK2 전용이다. IGN ON, 자산 미활성 상태, 요청 전체 SHA 및 모든 4KiB 섹터의 전후 SHA를 첫 erase 전에 검사한다. 0x1000..0x07F6FFFF 이외의 주소와 기존 데이터 덮어쓰기를 거부하고 완료 섹터의 정확한 재요청은 쓰지 않는다. 새 데이터→보조 FAT→주 FAT→root 순서로 기록한다. FAT 재구성 자체는 전원 차단에 원자적이지 않으므로 자동 실행/재시도하지 않는다. 중단 시 전체 백업과 변경 섹터 저널을 분석한 뒤 별도 복구한다.

실기기 `stock_fat_repair_apply.py`는 ARM O0/Os에서 검증한 동일 이미지와 기기 UID/APP를 요구하고, 쓰기 후 변경 구간과 인접 공백을 읽어 대조한다. `resource_install.py --repair-journal`은 원래 독립 A/B 백업에 이 검증된 변경 이력을 결합한다. 이것을 새로운 독립 전체 백업이라고 기록하지 않는다. 사진 import도 같은 엄격한 FAT 검사를 먼저 통과해야 한다. 상세 실행 증거는 `../../analysis/2026-09-16-storage-root-cause`에 있다.

## GPU RAM_G와 캡처

| 범위(끝 제외) | 용도 | bytes |
|---|---|---:|
| 0..0x17000 | 글꼴·아이콘 cache | 94,208 |
| 0x17000..0x17800 | 작은 앨범 아트 | 2,048 |
| 0x17800..0x18000 | shade480B+정렬 | 2,048 |
| 0x18000..0x88800 | RGB565 사진A | 460,800 |
| 0x88800..0xF9000 | RGB565 사진B | 460,800 |
| 0xF9000..0xFEA00 | 480×24 snapshot stripe | 23,040 |
| 0xFEA00..0x100000 | 여유 | 5,632 |

현재 사진을 유지하며 퇴역한 bank에 다음 사진을 제한 청크로 올리고, 완료 후240ms 교차 전환한다. 반전은 현재 mix에서 이어지고 제3 요청은 현재 전환 후 최신 요청으로 합친다. cache의 주소/용량 가드는 GNU wrap으로 구현하며 vendor는 변경하지 않는다.

캡처는 실제 swap 완료 후 display list와 GPU 자산 변경을 고정한다. CMD_SNAPSHOT2를24행씩20회 수행해 SDRAM에 조립한다. 모든 행이 완성된 후에만 generation/성공을 공개하고 화면 소유권을 즉시 돌려준다. 후속4KiB CRC mailbox 다운로드는 SDRAM만 읽는다. cmd4의4KiB display-list 기록도 같은 프레임이다. IGN 변화/2초 deadline/취소는 미완성 결과를 폐기한다. 진행 중에는1ms 서비스 간격을 사용한다.

부분 영역 명령 근거: [Bridgetek BRT_AN_088, CMD_SNAPSHOT2](https://brtchip.com/wp-content/uploads/2025/02/BRT_AN_088_FT81x_BT88x-Programming-Guide.pdf). 픽셀은 EVE 실제 출력이며 PC가 재그린 UI가 아니다.

## 재생성과 검증

`tools/build.ps1 -Configuration Release -Profile Product`와 Debug는 package 일치, APP 경계/startup, 메모리 목표까지 검사한다. Integrated/Graphics는 기존 시험용 기능을 유지하며 Product 메모리 예산의 적용 대상은 아니다. Integrated/Graphics Debug의 용량 초과는 명시된8개 HAL·10개 메모리/전송/서비스·7개 표시 드라이버를 필요한 프로필에서 Release와 같은-Os/-g3로 빌드해 해결한다. Graphics 단독은 StorageTask가 없으므로 Graphics_Init에서 DMA/SDRAM을 먼저 초기화해 캡처를 유지한다. 해당 파일의 step/local 디버깅은 최적화 영향을 받는다.

`sync_project.ps1`/`services_build.py`가 상대 include·소유 linker/startup·프로필 정책을 복원한다. Core 변경은 USER CODE malloc hook뿐이고 보존 여부도 검사한다. 실제 CubeMX GUI 재생성은 실행하지 않았다. `tools/tests/project_layers`는 재생성된 형태의 프로젝트 fixture를 복구하고 Core/IOC 불변성과 반복 동기화 동일성을 검사한다.

시험 위치: `tools/tests/resources`, `display_capture_host`, `wallpaper`, `bt_transport_host`, `power_host`, `power_ui_host`, `product_ui`, `dashboard_pages`, `protocol_services`, `project_layers`. ARM 모의시험의 SPI/RTOS/MMIO 경계는 실기기와 다르다. STOP100회 모형 결과를 실측 retention/current로, 두 폰+OBD 모형 결과를 실제 무선 연결로 보고하지 않는다.

2026-09-16 후속 실기기 설치: `analysis/2026-09-16-storage-root-cause/resource-install-resumed`에서 FAT/컨테이너 전체 readback을 통과했고, `boot-installed`에서 자산과 사진3개 READY 및 정상 UI를 확인했다. 약23초 표본 구간의 평균30.006FPS, CPU65.0~65.1%, RTOS 최소 여유24,936B, LVGL 여유30,976B, CCM guard 실패0이다. 최종 Product Release APP 여유90,116B·SRAM35,936B·CCM16,320B다. 정상 UI20stripe 캡처의 실제 완료 시간은1,126ms였다. 이 제한된 정상 부팅 시험을 물리 STOP100회/모든 메뉴 부하/실제 무선 BT 합격으로 확대하지 않는다.
