# 실행 중 SWD를 통한 NOR 백업

`StorageSWD_Init()`은 검증된 BSP_RAM/NOR 초기화 뒤 StorageTask에서 한 번 호출한다. 전용 8MiB SDRAM을 확보하며 성공은 0이다. 같은 태스크의 `StorageSWD_Process()`는 호출마다 최대 4KiB를 읽고 ISO CRC32를 누적한다. 완료 descriptor는 DSB/DMB 뒤 response_seq를 마지막에 게시한다. 새 유효 요청을 수락하기 전까지 버퍼를 보존한다. NOR 쓰기, 옵션 변경, FLASH 기록, reset은 없다.

백업은 **NOR physical wire byte 순서 그대로**다. 순정 APP staging/FAT의 논리 해석에 필요한16bit byte-pair swap을 적용하지 않으며 원시 A/B를 canonical APP로 취급하지 않는다. OTA의 `RuntimeUpdate` codec 및 READ_STAGE 응답은 별도 logical 표현이다. [표현과 근거](../Runtime/UPDATER.md)를 따른다.

`g_storage_swd`는 128-byte ABI1이다. host가 쓰는 범위는 SRAM의 offset48..64 다섯 word뿐이다. request_seq(+64)를 0으로 만든 뒤 request fields를 쓰고 새로운 nonzero seq를 마지막에 쓴다. device response_seq(+96)가 일치하고 READY/result0일 때만 데이터를 읽는다. BUSY 중 새 요청은 원 작업을 바꾸지 않고 거절한다.

PC에서 실제 실행 ELF와 특정 ST-LINK를 지정한다. 다음은 명령 형식이며 이 구현 시험에서는 장치를 실행하지 않았다.

```powershell
python tools/storage_swd_backup.py status --serial <STLINK_SN> --elf <running.elf> --directory <new-status-folder> --read-khz 950
python tools/storage_swd_backup.py backup --serial <STLINK_SN> --elf <running.elf> --directory <new-backup-folder> --read-khz 950
```

`status`는 읽기만 한다. `backup`은 반드시 --elf를 요구하고 기존 `validate_image.Elf32/validate`의 엄격한 주소·벡터·섹션 검사를 재사용한다. ALLOC 섹션과 빈 영역의0xFF 채움으로 구성한 canonical APP 전체가 실물과 일치해야 첫 SRAM 요청 쓰기를 허용한다. PT_LOAD 정렬 패딩의 파일상0x00을 설치 바이트로 오인하지 않는다. 주소만 지정하는 방식은 읽기 전용 status만 가능하다. ELF 파일 SHA와 실물 APP 읽기 파일/해시를 manifest에 남긴다.

`backup`은 NOR 전체128MiB를 A/B로 각각 새로 읽는다. 세그먼트의 순번·주소·길이·CRC와 읽기 전후 descriptor를 확인하고, SWD 데이터 CRC 오류에서는 새 NOR 요청 없이 같은 불변 SDRAM 버퍼만 재독한다. 최종 SHA256와 byte comparison이 같아야 verified manifest를 남긴다. MCU UID를 직접 읽고 mailbox UID와 대조한24자리 serial은 기존 `tools/storage_backup.py`의 unlock manifest와 호환된다. manifest 생성 자체는 쓰기 권한을 열지 않는다.

read rate 기본값은100kHz이며 950 또는4000은 별도 실측으로 검증한 경우에만 지정한다. 요청 SRAM 쓰기는 read rate와 무관하게50kHz다. HOTPLUG upload와 SRAM `-w32`만 사용하고 halt/run/reset 명령은 제공하지 않는다. DHCSR의 S_HALT가 감지되면 자동 재개 없이 중단한다. 설치된 CLI가 실제로 CPU를 멈추지 않는지와 SDRAM 전송 안정성은 실제 장치 검증 항목이다. 동시에 다른 디버거를 연결하지 않으며 백업 중 NOR writer를 실행하지 않는 정책은 Runtime 소유다. 오류·취소 때 원시 파일과 partial/로그를 남기고 결과는 미검증으로 유지한다.

실제 ARM C 모델: O0 4,143 / Os 4,149 checks, Os 최대8MiB 전체 비교 통과. PC 오프라인9개 시험 통과. 제품 CMSIS compile 통과. 근거: `tools/tests/protocol_services/storage_swd_output/storage_swd_results.json`, `storage_swd_pc_output/results.json`. 이 모델 결과는 실물 백업 완료를 뜻하지 않는다.
