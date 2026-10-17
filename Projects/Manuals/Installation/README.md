## 0.11.5 Bootstrap 표시 개선

**0.11.4에서는 일반 CFW 업데이트를 사용합니다.** Gate와 언어 자원은 그대로입니다. 더 오래된 Gate에서는 기존 안내대로 CFW 데이터를 유지하여 순정 복귀 후 최신 APK·ZIP으로 재설치합니다. 아래의 0.11.4 Gate 교체 설명은 구형 Gate에서 넘어올 때 적용됩니다.

새 Bootstrap은 최초 파일 검사를 숫자 없는 가로 로딩바로 표시합니다. 가운데 정렬한 메뉴에서 민트색 항목이 현재 선택입니다. 설치 중 두 막대는 전체 단계와 현재 기기 작업의 진행률을 나타내며, 전송·검증 사이에도 같은 자리를 유지합니다. 앱과 기기는 같은 기기 상태값을 사용하지만 폰의 조회 간격만큼 시간차가 있을 수 있습니다.

**INSTALL CFW?** 화면의 30초 카운트다운이 끝나면 검증된 설치를 자동 승인합니다. UP/DOWN을 누르면 카운트다운이 멈춥니다. **Back → O**는 취소, **Install CFW → O**는 즉시 승인입니다. 연결이 끊기면 자동 승인은 중단되고, 검증된 같은 작업에 다시 연결된 뒤 새로 30초를 기다립니다. 이미 쓰기 시작한 작업의 안전 취소 절차는 그대로입니다.

Ambient sensor 메뉴와 Bootstrap의 센서 샘플링을 제거했습니다. **Developer’s message**에서 개발자 원문 전체를 볼 수 있습니다. DOWN은 다음 페이지, 마지막에서 DOWN은 처음으로, O는 메뉴로 돌아갑니다. 주행 화면 자동 밝기는 유지합니다.

Bootstrap은 순정에서 설치할 때만 실행됩니다. 이미 0.11.4 CFW를 쓰는 사용자가 새 메뉴만 보려고 순정으로 돌아갈 필요는 없습니다. 이번 APK와 ZIP은 0.11.5를 함께 사용하세요.

[NewDoe 0.11.0 — startup, recovery, parking / 부팅·복구·절전](../User/36-NewDoe-Recovery.md)

# 설치·복구·호환성 ·0.10.1 베타

[0.10.1 다국어·설치 전환 안내 — 0.10.1 베타](../User/35-Languages.md)

[설명서 첫 화면](../README.md) · [English](README.en.md)

**0.10.1 APK와 펌웨어 ZIP을 한 쌍으로 사용하세요.** 이번 베타에는 새 Product·Gate·Bootstrap과 별도 언어 ZIP이 포함됩니다. 구형 Gate는 데이터 유지 순정 복귀 후 한 번 재설치하며, 정상 실행 확인 뒤 언어 자원을 설치합니다. 위 전환 안내를 먼저 확인하세요.

설치 허용 범위는 **순정 5.14 또는 5.16**입니다. HW·부트로더 버전·모델명·PCBA는 제한하지 않습니다. AK550와 벤치에서의 사용 결과를 모든 차종·PCB 리비전 검증으로 확대하지 않습니다. 호환 표에 확인 수준을 구분했습니다.

1. [호환 모델·하드웨어·버전 조건](07-Compatibility.md)
2. [순정에서 최초 설치](08-First-Install.md)
3. [기존 CFW 업데이트](09-Update.md)
4. [설치 정지·순정 복귀·표시부 복구](10-Recovery.md)
5. [문제 해결과 진단 자료](11-Troubleshooting.md)
6. [연결·페어링 복구](12-Connection-Recovery.md)

과거 버전 변경 기록과 예전 그림보다 위의 현재 절차를 우선합니다. [0.9.66 조작 안내](../User/23-Controls-Recovery.md) · [릴리스·대응 다운로드](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/tag/cfw-v0.10.1-multilingual-beta)

[0.9.62 · Dashboard blink alerts](../User/24-Dashboard-Alerts.md)

[0.9.63 · 알림 표시·자동 재연결](../User/25-Connection-Indicators.md)

[0.9.64 · HOME AUTO · 수동 재페어링 · 자동 복귀](../User/26-Home-Auto.md)

[0.9.65 · 최신 알림 · 위치·음악 답장](../User/27-Latest-Messages.md)

[0.9.66 · 속도 링 색상 조정](../User/28-Speed-Ring.md)

[0.9.67 · 실제 순정 응답 우선](29-Stock-Mode.md)

[0.9.68 · 정보 전송·설정 보존·연속 색상](../User/30-Sharing-Settings.md)

[0.9.69 · 설정 목록 오류·업데이트 복구](../User/31-Settings-Catalog.md)

[0.9.70 · 시동 OFF 배경 음영 제거](../User/32-Off-Photo.md)

[0.9.71 · 요약 음영·설정 보존·오일 호](../User/33-Ride-Preservation.md)
