## 0.11.3 언어 안정성 업데이트

0.11.2를 사용 중이면 **0.11.3 APK + NewDoe-CFW-0.11.3.zip**으로 CFW 업데이트하세요. Gate는 0.11.2와 동일하므로 이 경로에서는 순정 복귀가 필요하지 않습니다. 더 오래된 Gate는 기존의 데이터 유지 순정 복귀 안내를 따릅니다.

- 언어팩은 **NewDoe-LANGUAGE-B-0.11.2.zip**을 그대로 사용합니다. 파일명이 이전 버전이어도 맞습니다. 이미 설치된 팩의 실제 해시가 같으면 다시 보내지 않습니다.
- 이전 팩이 있으면 갱신 여부를 따로 묻습니다. 나중에를 선택하면 현재 팩과 언어 선호도를 유지합니다.
- 전송 완료 → 검증·저장 → 자동 재시작 → 실제 메뉴 언어 확인을 구분합니다. 저장 뒤 마지막 응답이 끊겨도 다시 연결해 기기 상태를 확인하며, 펌웨어를 다시 설치하지 않습니다.
- 언어를 바꿀 때는 영어 전용을 끄고 원하는 언어를 선택한 뒤, 정차 상태에서 기기의 재시작 안내를 확인하세요. 앱 따르기는 앱의 언어를 다음 부팅에 적용하고, 직접 선택한 언어는 재연결로 바뀌지 않습니다.
- 부팅 중 일시적 읽기 오류는 두 차례 더 읽어봅니다. 검증을 통과한 번역·글자 캐시는 재검사 때문에 갑자기 영어로 바뀌지 않습니다. 지속적인 오류는 영어로 대체하고 선호도를 보존합니다. 자동 폰트 복구는 하지 않습니다.

확인 순서: 한국어 선택 → 기기의 재시작 확인 → 앱을 누르지 않고 한국어 설정 화면 확인 → 일본어로 같은 과정 반복 → 전원을 다시 켜도 선택 언어가 유지되는지 확인. 이미 현재 팩이 설치돼 있다면 언어 자원 설치/이어가기에서 재전송 없이 실제 적용 언어가 표시돼야 합니다. 읽기 오류가 계속되면 진단을 보존하고 언어 자원 설치/이어가기로 다시 확인하세요. 펌웨어 완료와 언어 자원 오류는 별개입니다.

주행 중에는 설치하지 마세요. IGN ON/OFF는 설치를 취소하지 않지만 상시 전원은 유지해야 합니다. 실제 저장 공간이 부족하거나 폰트 손상이 확정되면 기존 선택 안내를 따릅니다.

---

# FuckNudo 설명서

[0.10.1 다국어·설치 전환 안내 — 0.10.1 베타](User/35-Languages.md)

**APK / Product0.10.1 beta · 2026-10-16**

1. [사용 설명서](User/README.md): 버튼, 연결, 음악·전화·알림, 지도, 전원과 설정
2. [설치·업데이트·복구](Installation/README.md): 순정 최초 설치와 기존 CFW 업데이트를 구분한 절차
3. [문제 해결과 진단 자료](Installation/11-Troubleshooting.md): Paused, 결과 미확인, 페어링과 복구
4. [상단 아이콘·배터리·GPS·연료 경고](User/23-Controls-Recovery.md)
5. [NOR 백업·파일 탐색기](User/13-NOR-Backup.md)

[0.10.1 APK](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest/download/NoodoeCompanion-0.10.1.apk) · [대응 설치 ZIP](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest/download/NoodoeInstaller-CFW-0.10.1.zip) · [릴리스 설명](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest)

APK와 ZIP은 같은 릴리스의 한 쌍을 사용합니다. 일반 업데이트는 설정·사진·트립·정비 기준점을 유지합니다. 최초 설치·명시적 새로 시작·완전 삭제와 구분하세요. 앱의 사용 설명서에는 연결 없이 읽는 요약이 포함되며 전체 설명서는 이 페이지로 연결됩니다.

과거 버전명이 붙은 변경 기록과 스크린샷은 당시 모습입니다. 현재 동작은 위의 사용·설치 설명서를 기준으로 하며 소프트웨어 렌더링 그림에는 시뮬레이션임을 표시합니다.

## 호환 모델과 설치 준비

[호환 조건·실차 확인 범위](Installation/07-Compatibility.md)를 먼저 확인하세요. 순정 5.14·5.16을 허용하며 HW·부트로더 버전·모델·PCBA로 차단하지 않습니다. 다른 차량의 배선·데이터 호환까지 모두 확인된 것은 아닙니다. 설치·복구의 한국어/영어 본문은 같은 순서와 조건으로 갱신했습니다. 0.10.1에는 새 Product·Gate·Bootstrap과 별도 언어 자원이 포함됩니다.

[0.9.62 · Dashboard blink alerts](User/24-Dashboard-Alerts.md)

[0.9.63 · 알림 표시·자동 재연결](User/25-Connection-Indicators.md)

[0.9.64 · HOME AUTO · 수동 재페어링 · 자동 복귀](User/26-Home-Auto.md)

[0.9.65 · 최신 알림 · 위치·음악 답장](User/27-Latest-Messages.md)

[0.9.66 · 속도 링 색상 조정](User/28-Speed-Ring.md)

[0.9.67 · 실제 순정 응답 우선](Installation/29-Stock-Mode.md)

[0.9.68 · 정보 전송·설정 보존·연속 색상](User/30-Sharing-Settings.md)

[0.9.69 · 설정 목록 오류·업데이트 복구](User/31-Settings-Catalog.md)

[0.9.70 · 시동 OFF 배경 음영 제거](User/32-Off-Photo.md)

[0.9.71 · 요약 음영·설정 보존·오일 호](User/33-Ride-Preservation.md)
