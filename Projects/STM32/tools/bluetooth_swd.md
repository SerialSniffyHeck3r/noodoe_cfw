# BT 서비스 1회 재시작 / 독립 raw Reset 진단

`bluetooth_swd.py`는 실행 중인 BT 서비스가 OFF 또는 FAULT일 때 START를 한 번 요청한다. 기본은 오프라인 ELF 검증/계획 출력이며 실제 요청에는 `--execute`가 필요하다. HOST는 장치의 GPIO, UART, DMA, 전원, FLASH, reset, halt, resume를 직접 변경하지 않는다. BT owner가 원래 확인된 전원/리셋 순서와 HCI 초기화를 수행한다.

`--raw-reset-diagnostic`는 별도 원샷 시험이다. owner가 H4를 분리한 상태에서 고정 HCI Reset 네 바이트 `01 03 0C 00`만 전송하며, 그 동안 CTS만 일시 무시하고 즉시 복원한다. 재전송·벤더 패치·스택 초기화를 이어가지 않는다. 기본 START와 명시적 `--start`는 같은 명령1이며 `--start`와 raw 옵션은 상호 배타다. raw 모드도 `--execute`가 없으면 계획만 출력한다.

```powershell
# 기본 계획만: 장치 접근이나 output 폴더 생성 없음.
python tools/bluetooth_swd.py --elf <현재 설치 이미지와 동일한 새 ELF> --output <새 결과 폴더>

# 독점 SWD 사용을 확보한 루트 담당자가 명시 1회 실행.
python tools/bluetooth_swd.py --elf <현재 설치 이미지와 동일한 새 ELF> --output <새 결과 폴더> --read-khz 950 --timeout 30 --execute

# 원샷 raw Reset: 새 진단 코드가 들어간 정확한 설치 ELF 필요.
python tools/bluetooth_swd.py --elf <현재 설치 이미지와 동일한 새 ELF> --output <새 결과 폴더> --raw-reset-diagnostic
python tools/bluetooth_swd.py --elf <현재 설치 이미지와 동일한 새 ELF> --output <새 결과 폴더> --raw-reset-diagnostic --read-khz 950 --timeout 30 --execute
```

1. `validate_image.py`의 strict ALLOC 기반 canonical APP 전체와 실제 APP를 비교하고 모듈 UID `[3735583,875974927,892810041]`를 확인한다. `g_bluetooth_control`48B와 `g_bsp_bt_hci_fault`184B는 ELF의 실제 writable main-SRAM object여야 한다. 잘못된 ELF/UID 또는 이미 halt된 CPU에서는 요청을 쓰지 않는다.
2. 이전 control 및 first-fault snapshot을 읽는다. control+44와 fault+8의 sequence를 전체 읽기 전후에 확인하여 값이 모두 같고 짝수인 복사만 사용한다. START가 진행 중이거나 controller가 READY/STARTING/STOPPING이면 새 요청을 보내지 않는다.
3. host 쓰기는 control+12의 `command=1`, 그 다음 control+8의 nonzero `request_sequence`뿐이다. 요청은50kHz, upload는 선택한100/950/4000kHz다. owner의 signed sequence 비교에 맞춰 이전 값+1을 사용하며0은 건너뛴다. 게시 명령이 timeout이어도 이미 전달됐을 수 있으므로 자동 재전송하지 않는다.
4. ACK 결과는 접수 여부다. 접수0, 일치하는 operation ID, 같은 request sequence의 terminal completion을 모두 확인해야 시작 완료다. BUSY/NOT_READY 거절은 이전 operation/완료를 남기므로 그 값을 새 시도의 결과로 해석하지 않는다. 성공은 phase3/result0/READY이고 실제 실패는 phase4와 별도 completion_result다.
5. 게시부터 완료 관측까지 최대30초이며 각 CLI timeout도 남은 시간으로 제한한다. 사전 APP identity와 완료 후 coherent evidence 수집 시간은 이30초와 별도다. timeout 때 reset/재요청/연결 회복을 자동 수행하지 않는다. 모든 raw reads, CLI 로그, observations, 오류는 fresh output 폴더에 남는다.
6. 완료 후 control의 operation payload가 보존되는지 확인한다. first-fault184B를 두 번 coherent하게 읽어 고정 상태도 검사한다. 이전과 같은 fault sequence면 **이전 시도의 기록**이며 새 실패 원인이라고 귀속하지 않는다. reason, UART 설정, CTS 입력 HIGH, DMA 잔여량 등은 capture 당시 값이고 실제 전압/파형 측정이 아니다.

START의 exit0은 성공 완료와 최종 READY를 관측한 경우다. 실제 HCI 실패·접수 거절·host 검증 실패는 exit2다. CLI 성공이나 ACK0만으로 BT가 작동한다고 보고하지 않는다. 프로세스가 끝난 뒤 또 다른 명시 실행을 하려면 새 output 폴더와 전체 identity 검증이 필요하다.

raw 모드는 같은48B control에 **command=2 → request_sequence 마지막** 순서로만 쓴다. argument 필드나 임의 HCI payload 입력은 없다. 추가 읽기는 ELF에 있는 `g_bsp_bt_reset_diagnostic`192B만 허용하며 mailbox/fault와 중첩되면 거부한다. record는 magic`0x42524431`/version1, sequence@8, operation_id@12, request_sequence@16, raw_data64B@128이다. 전후 sequence가 같은 짝수인 복사를 두 번 얻고 frozen payload도 비교한다. 이번 request와 opid가 일치하고, 이전과 다른 nonzero sequence이며 phase6 완료일 때만 이번 결과로 해석한다. 거절된 요청에 이전 raw 성공을 귀속하지 않는다.

raw의 exit0은 control phase3/result0, raw result0, 정확히1회 네 바이트 송신, NDTR0와 USART TC, DMA 정지 후 유효 raw bytes, close_result0, CTSE 복원과 RTSE 보존, 실제 H4 `04 0E 04 <credits> 03 0C 00` 응답을 모두 확인한 경우다. 종료 후 controller는 기존 OFF/FAULT를 유지한다. **이 exit0은 한 번의 Reset 응답이며 BT READY·SPP·페어링·벤더 패치 성공을 뜻하지 않는다.**

실패도 독립 기록한다. `primary_result`는 cleanup 전 결과이므로0만으로 통신 성공이라고 해석하지 않는다. `result`는 최종 오류이고 cleanup 실패가 이를 덮을 수 있다. flow_restore는0(override 없음),1(복원 검증),2(실패)이며 host의 `cleanup_verified`는 close0/flow1과 CR3의 before/bypass/restored CTSE·RTSE 실제 비트까지 맞아야 true다. RX_TIMEOUT에서도 동일하게 검사한다. data_valid1이면 `rx_captured_bytes == 64 - dma_rx_remaining`과 close0을 확인한다. data_valid0의 bytes를 응답으로 파싱하지 않는다. GPIO/SR/DMA CR은 Close 직전 표본이고 유효 count/NDTR는 DMA 정지 후 값이다. GPIO HIGH 및 DMA 완료는 전압·파형이나 상대 칩 수신을 측정한 결과가 아니다.

`tools/tests/protocol_services/bluetooth_swd_test.py`는 실제 Python host/CLI 생성부를 FakeCLI에 연결하고 모든 subprocess를 금지한다. 보존731f ELF의 임시 non-ALLOC symbol/string metadata만 바꾸며 canonical APP가 원본과 동일함을 검증한다. 이 fixture는 실제 장치에 쓰지 않는다. 기존 START15개를 포함한27개 시험은 identity/UID/halt 차단, 선택 명령 allowlist/1회 제한, ACK·완료 분리, 거절 시 old operation, signed 실패, sequence wrap, timeout, torn/odd/frozen snapshots, raw byte oracle, 실패 시 복원 비트/NDTR 교차 검증을 검사한다. 실제 C source/header ABI도 대조한다. 결과는 `tools/tests/protocol_services/bluetooth_swd_output/results.json`에 있으며 실물 성공 검증을 대신하지 않는다.
