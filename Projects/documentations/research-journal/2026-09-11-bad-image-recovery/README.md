# 잘못된 이미지의 동작·복구 분석

2026-09-11. 별도 Android 개발 프로젝트를 제외하고 도너 Noodoe의 실행/메모리/복구 통로를 분석했다. 장치 쓰기나 오류 주입은 수행하지 않았다.

**통합 결과: [잘못된 펌웨어와 복구 연구노트](../../docs/2026-09-11-noodoe-bad-firmware-and-recovery.md)**

- [boot-scenarios](boot-scenarios/README.md): 잘못된 메타·이미지·길이, FLASH 부분 교체, pending이 복구 APP를 다시 덮는 경우.
- [app-scenarios](app-scenarios/README.md): 실제 boot/APP fault 벡터, VTOR·초기화 오류, freeze/reset/lockup·접속 문제의 구분.
- [rom-recovery](rom-recovery/README.md): ST 공식 ROM 부트·USB/USART·보호·OTP 조건.
- [recovery-reference](recovery-reference/README.md): 일치하는 A/B에서 분리한 네 원본 영역과 해시. 실행 스크립트가 아닌 복구 참고 자료.

현재 내부 A/B 전체 SHA-256:

`38207bed741a8fef9e6d2ed24b86c8cb8b376ba5e4c957163f5edd83fbeb8037`

원본 보존, 영역별 분리 및 전체 재조립 일치는 `prepare_reference.py`로 재현한다. 부트·APP 파일의 실제 실행 보장이나 실물 복구 성공을 대신하는 검증은 아니다.
