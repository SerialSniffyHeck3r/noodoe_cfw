# 실패 처리·APP 호환 계약 분석

통합 결과는 [연구노트](../../docs/2026-09-11-noodoe-update-failure-and-app-contract.md)에 있다. 원본과 장치에는 쓰지 않았고, production 코드도 수정하지 않았다.

| 범위 | 보고서 |
|---|---|
| resident의 실패·재시도·전원 출력 | [boot-failure](boot-failure/README.md) |
| MCU APP cleanup·타임아웃·부트 진입 ABI | [mcu-app](mcu-app/README.md) |
| 현재 OpenNoodoe 스냅샷과 순정 APK 대조 | [host-app](host-app/README.md) |
| 로컬 이미지·메타 분기 모델 | [contract_validator.py](contract_validator.py), [결과 JSON](contract-validation.json) |

핵심은 자동 롤백 부재, 설치 실패 시 다음 부트의 동일 이미지 재복사, 순정 APP 수용 정책과 resident 필수 조건의 분리다. 특히 섹터 2는 설치 후 20바이트만 복원되므로 일반 설정 저장소로 공유하지 않는다. 앱 진입은 하드웨어 reset 상태가 아니며 자체 VTOR·런타임·IRQ·클록 초기화가 필요하다.

`contract_validator.py` 인자 없이 실행하면 고정 원본과 in-memory 사례로 메타 14개, 부트 메타 해제 byte-prefix 21개, 이미지 4개를 검사하고 파생 JSON을 갱신한다. 이미지 경로와 `--version MAJOR.MINOR`를 주면 JSON 진단만 표준 출력한다. 펌웨어 생성기·송신기·하드웨어 오류 주입기가 아니다.

검증기의 추가 Reset 주소 범위·정렬 검사는 부트 원래 검사보다 엄격한 제안 정책이다. `boot_length_gate_only`, `stock_v516_numeric_policy_only`, `additional_offline_image_checks_pass`는 각각 다른 범위를 나타낸다. 어느 하나도 실행 성공을 보증하지 않는다.
