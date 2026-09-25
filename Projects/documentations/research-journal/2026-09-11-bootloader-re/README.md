# 실물 부트로더 + APP + APK 업데이트 경로 분석

2026-09-11. A/B 전체 덤프의 바이트 일치와 실물 APP = SR1.5 V5.16 OTA 일치를 전제로, 실제 resident 부트로더를 복원하고 양쪽 업데이트 프로토콜과 대조했다. 오프라인 분석만 수행했다.

**통합 결론과 메모리 지도는 [연구노트](../../docs/2026-09-11-noodoe-bootloader-and-bluetooth-update.md)에 있다.**

실패·타임아웃·부트 진입 ABI·커스텀 APP 및 호스트 요구사항은 [후속 실패·호환 계약](../../docs/2026-09-11-noodoe-update-failure-and-app-contract.md)을 따른다.

핵심 흐름은 Bluetooth SPP → 실행 중인 APP → 외부 NOR `0x07F90000` 저장·전송 CRC 확인 → IGN OFF 이벤트 → 내부 `0x08008004` 설치 요청 → 다음 부트에서 SRAM 코드가 내부 `0x08010000`에 복사 → 재부팅이다. IGN OFF에서 즉시 reset하는지는 별도의 보드 HW revision 조건이 있다.

| 분석 | 산출물 |
|---|---|
| Reset·압축·메타데이터·APP 진입 | [startup](startup/README.md) |
| NOR→내부 FLASH, APP·BL 자체 갱신 | [full-flash-update](full-flash-update/README.md) |
| APP의 BT 수신·검증·IGN OFF 인계 | [app-bt-update](app-bt-update/README.md) |
| 순정 APK의 payload·ACK·업데이트 순서 | [apk-bt-protocol](apk-bt-protocol/README.md) |
| 이번 원본 A/B의 바이트·영역 비교 | [full-flash-ab-verification](../2026-09-11-full-flash-ab-verification/README.md) |

BL 복사 코드에 서명 검증은 확인되지 않았지만 실제 새 이미지의 BT 설치·부팅은 아직 검증하지 않았다. APP 수신 경로에는 별도의 전송 CRC·버전·길이 조건이 있다. 부트 자체 갱신 루트의 존재와 순정 APK가 그 루트를 호출할 수 있는지는 구별했다.
