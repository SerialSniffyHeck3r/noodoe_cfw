# SWD 백업의 실제 ELF identity 회귀

2026-09-12 최초 백업은 SRAM 요청을 쓰기 전에 APP/ELF 불일치로 중단됐다. 실제 APP 문제는 아니었다. 직접 PT_LOAD의 file bytes를 비교하면 `.isr_vector`와 `.text` 사이 `0x080101AC..0x080101AF`의 미할당 정렬 패딩도 포함된다. ELF에는0x00, 설치 exporter와 실물에는0xFF였다.

`tools/storage_swd_backup.py`는 이제 기존 `tools/validate_image.py`의 `Elf32`와 `validate`를 그대로 사용한다. PT_LOAD의 APP 범위 검사, 섹션 LMA/VMA/파일 매핑, 벡터·startup·RAM 계약을 유지한다. ALLOC file-backed section을 배치하고 gap을0xFF로 채운 전체 canonical APP를 비교하므로 실제 코드·초기화 데이터·gap 어느 바이트도 무시하지 않는다. MCU의 mailbox 주소도 검증된 ELF 심볼과 대조한다.

고정 근거는 `Reversing/analysis/2026-09-12-integrated-bringup/integrated-final-image`의 ELF/app.bin과 `nor-full-backup/00006-running-app-identity.bin`이다. canonical·app.bin·실물 파일은 모두383,400bytes, SHA256 `89b28979eef992a7d1dea8361755625e0ac41edc56e33f34673682c1981898f6`이다. 원본 파일은 수정하지 않았다.

`storage_swd_pc_test.py` 9개 시험이 통과했다. 실제 ELF 회귀, lower FLASH를 향하도록 손상한 LOAD 거절, truncated ELF 거절, 실제 읽기 파일의 vector/code/.data initializer/gap 한 비트 변경 거절을 포함한다. 테스트 전체에서 subprocess 장치 호출은 강제로 차단한다. 결과와 입력 파일 해시는 `storage_swd_pc_output/results.json`에 기록했다.

이 수정은 PC 소스와 시험에 한정한다. 펌웨어 생성·빌드·기록 또는 새 하드웨어 읽기를 실행하지 않았다. 이전 failed 백업 폴더는 증거로 남기고 실제 재시도는 새 폴더에 수행한다.
