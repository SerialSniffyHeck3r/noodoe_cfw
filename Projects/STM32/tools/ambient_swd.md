# 조도 센서 SWD mailbox 진단

`ambient_swd.py`는 실행 중인 새 `AmbientService`에80/100/400kHz probe를 요청한다. 기본 요청 순서는400/100kHz이며80kHz는 `--rates 80000`으로 명시할 때만 선택한다. IOC 기본400kHz는 바꾸지 않는다. 도구 자체는 장치 GPIO·전원·I²C 레지스터·FLASH를 직접 쓰지 않는다. 서비스의 StorageTask owner가 확인된 I2C3/PH7/PC9 경로만 재초기화하고 센서 ID를 읽는다. ID가 정상일 때만 BSP의 설정 쓰기가 이어진다.

```powershell
# 기본: ELF 검증 및 계획 출력만. 장치 접근/폴더 생성 없음.
python tools/ambient_swd.py --elf <보존한 새 ELF> --output <새 결과 폴더> --rates 100000

# 루트 장치 담당자가 독점 debugger 사용 상태에서 명시 실행한다.
python tools/ambient_swd.py --elf <보존한 새 ELF> --output <새 결과 폴더> --rates 400000 100000 --read-khz 950 --execute

# 추가 독립 ID 시험: 새 ELF의 command3/진단 ABI가 있어야 한다.
python tools/ambient_swd.py --elf <현재 설치된 새 ELF> --output <새 결과 폴더> --id-bitbang --read-khz 950 --execute

# ABI3 전용 명시 비교: 이번 ID 읽기에서만 PC9 내부 약 pull-up을 추가하고 원복한다.
python tools/ambient_swd.py --elf <현재 설치된 ABI3 ELF> --output <별도 새 결과 폴더> --id-bitbang --sda-pullup --read-khz 950 --execute

# 새288B 주소 진단 ABI 전용: 고정44..47 네 후보를 명시적으로 한 번 시험한다.
python tools/ambient_swd.py --elf <현재 설치된 주소 진단 ELF> --output <별도 새 결과 폴더> --address-diagnostic --read-khz 950 --execute
```

- ELF를 기존 strict validator로 검증하고 canonical APP bytes 전체와 실제 MCU UID를 대조한 뒤 SRAM 쓰기를 허용한다. UID words는 현재 모듈 `[3735583,875974927,892810041]`다. 새 ELF에 `g_ambient_mailbox`124-byte object가 없으면 연결 전 실패한다.
- HOTPLUG 읽기만 사용하며 DHCSR의 S_HALT가 설정되면 실패한다. halt/resume/reset/freeze/option 변경이나 flash programming은 제공하지 않는다. Control request 쓰기는 항상50kHz, read는100/950/4000kHz 중 선택한다.
- Host 쓰기는 mailbox command+12, argument+16, request_seq+8 세 word뿐이며 sequence를 마지막에 게시한다. 일반 probe는 명령1의80000/100000/400000, ID-only bitbang은 명령3/argument0을 쓴다. `--sda-pullup`을 명시한 ABI3 시험만 argument1을 허용한다. 주소 진단은 별도 명령4/argument0이다. 서비스의 enable 명령2나 임의 값을 host raw write API로 제공하지 않는다.
- 응답 sequence+20을 전체124-byte 읽기 앞뒤에서 확인하고, 일치하는 완료를 한 번 더 읽어 결과가 고정돼 있는지 검증한다. 요청 seq, command, argument, operation ID, acceptance status, BSP result가 분리된다.
- DeInit/Init 실패 시 observed_bus_hz는 이전 값 또는0일 수 있다. phase>=3 또는 성공일 때만 요청한 속도가 실제 적용됐는지 검사한다. requested_hz와 observed_bus_hz를 모두 기록한다.
- 제조사/장치 ID, HAL status/ErrorCode, SR1, 실패 단계, 마지막 단계 소요ms가 응답에 남는다. ARLO와 AF/NAK를 구별한다. 단계 소요시간을 전체 작업 시간이나 파형 측정으로 표현하지 않는다.
- 모든 CLI 명령·raw read·요청/완료를 새 폴더에 보존한다. 기본 두 속도 중 첫 probe가 실제 센서 오류여도 두 번째를 수행하고 두 결과를 남긴다. host timeout, 타 클라이언트의 요청 변경, ABI/identity 오류는 자동 reset/재요청 없이 중단한다.
- exit0은 모든 요청이 접수·완료됐고 BSP 성공/ID/ready가 검증된 경우다. 실제 센서 실패·요청 거절 또는 host 검증 실패는 exit2이며 manifest의 상태와 각각의 completion을 확인한다. BUSY 거절을 센서 시험 완료로 취급하지 않는다.

`--id-bitbang`은 `--rates`와 상호배타다. 한 실행에서 선택한 command3 variant를 한 번만 요청하며 뒤이어 정상 probe를 자동 수행하지 않는다. PH7/PC9의 기존 I2C3 경로에서 고정 주소0x45의 ID 레지스터0x7E/0x7F만 읽는다. I²C register-pointer 전송은 읽기에 필요한 단계이며 센서 설정 쓰기·주소 scan·bus-clear train·전원 GPIO 조작은 포함하지 않는다. 실제 핀 전환/복원은 서비스 owner가 수행한다.

bitbang 완료는 기존124-byte mailbox의 seq/opid와 **별도 `g_bsp_ambient_bitbang` evidence**를 교차검증한다. 실제 ELF object 크기184B면v1,208B면v2,212B면v3를 선택하며 임의 크기는 거절한다. wire magic/version/bytes도 이 선택과 같아야 한다. 별도 sequence@12는 nonzero equal-even이어야 하며 완료 후 전체를 다시 읽어 고정 여부도 확인한다. 과거 HAL driver76-byte의 ready/ID/error/bus_hz는 이번 bitbang 성공 판단에 사용하지 않는다. ID `5449/3001`,6회 ACK/마스크3F, wire result0, 실제 pin_changes1, 복원 result0, 저장/최종 pin·I2C 설정 일치, timing 상태를 함께 확인한다. NACK·SCL timeout·SDA conflict·복원 실패를 각각 남긴다. 복원 result2는 핀 인수 전 전제조건에서 중단한 경우다.

v3는208B prefix 뒤 offset208에 `pullup_mode`를 추가한다. `--sda-pullup`은 `--id-bitbang` 없이 사용할 수 없고, 실제 ELF가 v1/v2이면 장치 연결 전에 거절한다. 기본 argument0에서는 PH7/PC9 모두 NOPULL이다. argument1은 PC9에만 일회성 내부 약 pull-up을 적용하고 PH7은 NOPULL을 유지하며, 종료 시 PC9의 원래 설정까지 복원해야 한다. mailbox argument와 별도 pullup_mode가 다르면 raw 결과를 저장한 뒤 실패한다. mode1은 전제조건 거절에도 요청 variant로 남으므로 `sda_pullup_requested`와 `pin_takeover_observed`를 구분한다. `temporary_sda_weak_pullup`은 mode1에 실제 pin_changes1 및 인수 후 복원 결과가 있을 때만 true다. **약 bias에서의 ID 성공은 원래 pull-up 회로의 정상 판정이나 조도 측정 복구가 아니다.** `original_hardware_proven`과 `lux_measurement_verified`는 false로 유지한다. 외부 pull-up 단선, 직렬 저항 유실, 누설, 센서 전원 등 개별 원인은 이 시험만으로 확정하지 않는다.

v2는 기존184B prefix 뒤에 첫 주소byte의 bit0에서 관측한 `first_pre_lines/us`, `first_early_lines/us`, `first_late_lines/us`6word를 붙인다. timestamp는 이번 진단의 DWT 시작부터 지난µs이며0은 미관측이다. lines bit0은SCL,bit1은SDA이므로 lines0도 timestamp가 있으면 실제 양선LOW 관측이다. pre는25µs setup 뒤 SCL 상승 전, early는 SCL HIGH 관측 시점, late는 early conflict 뒤 정상25µs HIGH 구간이 지난 시점이다. 결과의 `first_address_bit`는 v1이면 unavailable, v2 미도달 시점은 unobserved/null로 표시한다. **early SDA conflict는 result6으로 유지하며 late SDA HIGH를 성공으로 바꾸지 않는다.** 세 디지털 sample은 연속 파형이나 아날로그 상승시간 측정이 아니다.

bitbang은 명목20kHz다. 진단의 `clock_hz`는 **DWT 시간 환산에 사용하는 SystemCoreClock**이며 I²C 주파수가 아니다. 보고서에는 `source_clock_hz`와 `nominal_bus_hz`를 구분한다. IRQ/스케줄링으로 실제 파형 간격이 늘어날 수 있어 max_gap_us, max_low_us, timing_uncertain을 기록하며 max_low_us>=28000은 센서 SCL-low timeout 가능성으로 표시한다. 게시부터 응답까지 기본/최대30초이며 개별 CLI도 남은 시간으로 제한한다. 사전 APP 검증과 완료 후 frozen evidence 수집은 별도 시간이다. 요청 timeout은 자동 재전송/reset으로 처리하지 않는다.

`--address-diagnostic`는 `--rates`/`--id-bitbang`과 상호배타이며 `--sda-pullup` 조합도 거절한다. 이 명시적인 command4/argument0 자체가 PC9 weak pull-up을 쓰는 고정 주소 진단이다. 임의 주소/핀/레지스터 인수는 제공하지 않는다. 후보는 TI ADDR strap에 대응하는 `0x44,0x45,0x46,0x47` 네 개뿐이다. 각 후보에 주소W/ACK/STOP을 확인하고 ACK가 있는 경우에만 ID 레지스터7E/7F를 읽는다. NACK/완전한 다른 ID는 다음 후보를 허용하지만 line/timing/STOP 오류는 batch를 중단한다. 전원 GPIO·센서 설정·general call·bus clear는 요청하지 않는다. PH7은 NOPULL이며 종료 후 PC9도 원래 NOPULL로 복원해야 한다.

주소 진단의 근거는 실제 ELF에 있는 `g_bsp_ambient_address` object 288B, magic41424131/version1이다. 기존 cmd3의184/208/212B 기록과 HAL driver76B는 읽거나 성공 판정에 사용하지 않는다. header14word 다음에 entry4×10word(offset56/stride40), offset216부터 saved/final CR1/CR2/CCR/TRISE/FLTR/PH7/PC9와 pin_changes/pullup_mode/max_gap_us/max_low_us18word가 온다. sequence@12를 전체 읽기 앞뒤에서 확인하고 동일한 nonzero even 완료만 받는다. mailbox seq/opid/result와 대조하고 완료를 한 번 더 읽어 고정 여부를 확인한다.

`attempted_mask`, `address_ack_mask`, `id_match_mask`의 bit0..3은 각각44..47이다. **주소 ACK만으로 OPT3001을 식별하지 않는다.** `attempted=0` 행의 기본 result는 `NOT_ATTEMPTED`로 출력하며, precheck 실패에서도 주소 네 개가 채워졌다는 이유로 시험했다고 하지 않는다. ID5449/3001을 읽은 후보는 별도로 표시한다. 하나의 ID가 일치했어도 뒤 후보의 전기적 오류·STOP 실패·전체 복원 실패가 있으면 전체 성공으로 승격하지 않는다. exit0에는 하나 이상 ID 일치, 네 후보 방문 완료, 허용된 행 결과, 정상 STOP/복원/시간 근거가 함께 필요하다. 모든 NACK·ID 불일치·중간 중단은 exit2다. 성공도 일회성 weak-bias ID 판독으로 한정하며 조도/lux 측정 및 원래 보드 pull-up 회로의 정상 상태는 검증하지 않는다. 명목20kHz, BSP active budget100ms/최대 scheduling gap10ms와 별개로 host의 게시·완료 대기는 최대30초다. 시간 초과나 충돌에 자동 재게시/reset을 하지 않는다.

`tools/tests/protocol_services/ambient_swd_test.py`의43개 시험은 실제 Python 코드와 Board 명령 생성부를 FakeCLI에 연결하며 `subprocess.run`을 금지한다. 기존731f ELF의 코드/LOAD는 보존하고 임시 복사본의 non-ALLOC symbol/string metadata만 바꾼 test-only fixture를 사용한다. 이 fixture는 장치에 쓰지 않았으며 실제 새 펌웨어의 실행 검증을 대신하지 않는다. 헤더 magic/version/size/선언순서와 host ABI를 직접 비교한다. bitbang의 opid 불일치/거절/1회 게시/timeout/별도 ID/복원 해석, v1/v2 호환성, late HIGH의 실패 유지, timestamp0, size/version 불일치 차단을 포함한다. v3의 명시 arg1, 구 ABI/단독옵션 차단, argument/mode 양방향 불일치, 요청과 실제 pin 인수의 구분, weak-bias ID 성공의 제한도 검사한다. 주소 진단은 실제288B decode, all-NACK/일치 ID/이후 실패/복원 오류/미방문, 잘못된 mask·주소·seq·opid, tearing/frozen 변화, 한 번 게시 및 별도 whitelist를 검사한다. 결과와 소스 SHA는 `ambient_swd_output/results.json`에 있다.
