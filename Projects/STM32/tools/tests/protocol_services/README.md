# 실제 ARM C 프로토콜 시험

`run.py`는 설치된 STM32CubeIDE1.18.1의 ARM GCC로 서비스 원본 C와 시험 C를 `-O0`, `-Os`, `-Wall -Wextra -Werror`로 컴파일하고 Cortex-M4 Unicorn에서 실행한다. Python은 ELF 적재와 결과 수집만 수행한다. libc byte/string 경계는 작은 freestanding 구현을 사용한다. 출력은 이 폴더의 output만 변경하며 장치/GUI/프로젝트 빌드를 사용하지 않는다.

```powershell
& '<local-user>\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' .\tools\tests\protocol_services\run.py
```

2026-09-12 결과: Vehicle/GNSS/OBD/NDCP/Updater 합계 최적화별 **11,883 assertions** 통과. [결과와 source SHA](output/results.json).

- Vehicle: 실측 fuel0/1·ODO36,475, 모든13-byte fragment 경계, XOR 오류 뒤 정상 frame, 최대255-byte payload의 F5, CMD22, 짧은 payload 거절, 잘못된 length의 timeout 재동기화, stale/tickwrap.
- GNSS: 표준 RMC/GGA 고정 checksum 및 모든 RMC fragment 경계, GP/GN·S/W·음수 고도·정수 단위, no-fix, 외부 연결 중 전화로 fallback 금지, disconnect/reconnect, field별 stale, 잘못된 checksum/좌표/시간/날짜/overflow, tickwrap.
- OBD: 초기 command 정확한 byte,8개 PID 공식과지원 bitmap, 조각 응답·echo·prompt, NO DATA/error/negative/wrongPID/headers/garbage/overflow, 복수 ECU, timeout/늦은 응답 격리, init실패/reconnect, stale/tickwrap.
- NDCP/Updater: 모든 frame 분리 경계와 오류 뒤 복구, SHA256 알려진 vector, 독립 Python hashlib/zlib로 생성한448KiB 이미지의 실제 C 전송·NOR 재읽기·SHA/CRC 검증, 중복 chunk, 잘못된 vector/hash, 실패 후 재승인 강제, 검증 완료 후 재연결, 명시 commit/reset과 ACK 유예, queuefull. NOR/메타/reset은 경계 모형이며 물리 쓰기는 없다.

PC CLI는 [test_ota_pc.py](test_ota_pc.py)의8개 시험을 통과했다. 최대 frame 분리, 손상 복구, APP 범위, 백업 크기, 응답 조각, reset ACK 뒤 연결 유지, FINISH/COMMIT ACK 유실 후 읽기 전용 journal 대조를 포함한다. [PC 결과](output/pc_results.json).

별도 [STM32 wordCRC 시험](crc_README.md)은 실제 ARM C 최적화별2,865 assertions와 순정4개 이미지의 trailer/residue를 확인했다. [메타 writer 시험](metadata_output/results.json)은 최적화별4,193 assertions 및 제품 CMSIS backend/RAM relocation을 확인했다. [쓰기 전제와 제한](../../../Middlewares/Noodoe/Update/METADATA.md)을 별도로 따른다.

이 시험은 실제 UART/SPP/GNSS/ELM/ECU 및 멀티태스크 동기화, 전기적 파형, 하드웨어 링크 신뢰도를 검사하지 않는다. 통합 펌웨어는 별도로 다시 빌드·검증해야 한다.
