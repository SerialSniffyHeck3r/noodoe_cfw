## 0.11.6 Bootstrap 화면과 선택형 재페어링

**0.11.4·0.11.5에서는 일반 CFW 업데이트를 사용합니다.** Gate와 언어 자원은 같습니다. 더 오래된 Gate에서는 기존 안내대로 CFW 데이터를 유지하여 순정 복귀 후 최신 APK·ZIP으로 재설치합니다.

최초 9개 파일 검사는 숫자 없는 가로 로딩바로 표시합니다. 설치 중 위쪽 연한 민트색 막대는 전체 진행률, 아래쪽 민트색 막대는 현재 기기 작업입니다. 전체 막대는 완료한 단계와 현재 작업의 진행분을 합쳐 채웁니다. 누도에는 퍼센트를 표시하지 않습니다. 앱은 같은 상태값으로 계산하며 조회 주기만큼 표시 시점이 다를 수 있습니다.

메뉴에서 **Install CFW → Reset Bluetooth?**가 나옵니다. **Yes를 권장**하며 저장된 모든 휴대폰 페어링을 초기화합니다. 자동으로 Yes를 선택하지 않습니다. **No**는 초기화 없이 기존 연결을 사용합니다. Yes를 선택했다면:

1. 누도의 **PAIR YOUR PHONE** 안내를 기다립니다.
2. Android Bluetooth 설정에서 이 Noodoe를 등록 해제합니다.
3. 같은 Noodoe를 새로 페어링합니다.
4. 앱으로 돌아와 **현재 기기 확인 → 여기서부터 계속하기**로 설치를 이어갑니다. Bootstrap을 다시 전송하지 않습니다.

초기화 도중 기존 연결이 끊기는 것은 정상입니다. 기존 CFW 설정 저장소가 있으면 삭제한 키를 저장한 뒤 Bluetooth를 다시 시작합니다. 첫 설치로 저장소가 아직 없으면 이번 Bootstrap에서 재페어링하고 최종 설치 전에 새 키를 저장합니다. 이 경우 설치 전 전원 재시작으로 초기화 상태가 보존된다고 보장하지 않습니다. 실패·저장 오류는 성공으로 표시하지 않습니다.

최종 **INSTALL CFW? / Everything is ready.** 화면은 **Back / Install CFW**를 좌우로 표시합니다. 30초 카운트다운이 끝나면 기존 절차로 설치를 승인합니다. UP/DOWN은 카운트다운을 멈추고 선택을 바꾸며, O는 선택한 항목을 실행합니다. Back은 취소, Install CFW는 즉시 승인입니다. 연결이 끊기면 자동 승인은 중단됩니다.

**Developer’s message**는 원문 전체를 큰 원형 화면에 연속 배치합니다. **DOWN은 다음 화면, UP은 이전 화면**, 끝에서는 반대 끝으로 순환하고 O는 메뉴로 돌아갑니다. 원문·글씨 크기는 그대로이며 고정 제목과 하단 안내가 차지하던 영역까지 본문을 표시합니다.

Bootstrap은 순정에서 설치할 때 실행됩니다. 이미 CFW를 쓰는 사용자가 메뉴 변경만 보려고 순정으로 돌아갈 필요는 없습니다. 새 Bootstrap을 실제로 실행하려면 순정 설치 과정에 이번 ZIP을 사용해야 하며, 일반 Product 업데이트는 실행 중인 Product만 교체합니다. 이번 APK와 ZIP은 0.11.6을 함께 사용하세요.

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
