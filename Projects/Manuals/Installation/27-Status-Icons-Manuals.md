# FuckNudo 0.9.60 — centered warnings, status icons and manuals

연료 경고 위치와 상단 표시를 다듬고, 현재 기능과 설치·복구 절차에 맞춰 설명서를
정리했습니다. 사용자가0.9.59에서 확인한 연료 음영과 순정→CFW 설치 성공을
기록했으며, 이번 버전은 그 음영 구현과 설치 경로를 유지합니다.

- **연료 경고:** 아이콘이 줄어들고 LOW FUEL 문구가 나오는 순간의 묶음을
  화면 중앙으로 옮겼습니다. 최종 위치만24px 아래로 이동하며 원래 Rounded
  아이콘, 크기, 글꼴, 점멸·전환 시간과 배경 음영은 같습니다.
- **상단 메뉴:** HOME에서만5초 뒤 자동으로 숨습니다. 다른 주행 페이지에서는
  5개 아이콘을 계속 표시합니다. 전체 설정·경고·전원 화면은 기존 배치를 사용합니다.
- **GPS:** 새 위치를 받을 때120ms만 완전히 꺼졌다 켜집니다. 빠른 연속 수신은
  묶어 최소480ms의 점등 시간을 확보하고 같은 표본으로 펄스를 연장하지 않습니다.
- **Bluetooth 배터리 표시:** 휴대전화가 충전 중이 아닐 때20% 미만 노랑,
  15% 미만 빨강,5% 미만 빨강/흰색 교대입니다. 충전 중에는75% 미만 초록/흰색
  교대,75% 이상 초록 고정입니다. 교대는 각 색0.5초이며 통화 아이콘은 기존 우선순위입니다.
- **설명서:** 한국어·영어 사용 및 설치·업데이트·복구 안내를 정리하고 앱 내
  도움말도 갱신했습니다. 오래된 전체 설명서 링크와180초 안내도 바로잡았습니다.

**주행 연동을 멈추고0.9.60 APK를 덮어 설치한 뒤, 같은0.9.60 ZIP으로 Product를
업데이트하세요.** 충전 상태 전송을 위해 APK와 Product를 함께 올려야 합니다.
일반 업데이트는 설정·사진·트립을 유지합니다. 기존 정상 CFW는 일반 업데이트를
사용하며 Gate 재설치가 필요하지 않습니다. Gate·Bootstrap·제거·진단·순정·자원
이미지는0.9.59와 바이트가 같습니다.

Stage6/8 Code6/Phase4 또는 결과 미확인 안내가 나오면 같은 기기·ZIP으로 마지막
작업을 조회하고 진단을 보존하세요. 구버전에서 넘어오는 첫 재시작은 기존 펌웨어가
수행합니다. 전송100%와 NOODOE INSTALLER는 최종 부팅 확인이 아닙니다.

[한국어 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/README.md)
· [English manual](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/README.en.md)

Android554개 시험, ARM 표시·입력·프로토콜 시험, EVE1,203프레임 및18개 상태
교란 시험을 통과했습니다. Debug 여유1,592B, Release33,676B로 승인된 최소
기준을 충족합니다. **0.9.60은 실기에서 시험하지 않았습니다.**0.9.59의 사용자
성공 보고를 모든 리비전이나 모든 업데이트 경로의 검증으로 확대하지 않습니다.
상세 결과는 [검증 보고서](../../documentations/status-icons-0.9.60.md)를 참고하세요.

## English

The settled fuel icon/text group is centered24px lower, preserving the original
rounded icon, size, font, animation and full-screen veil. Only HOME auto-hides
the five page icons. GPS gives a120ms OFF pulse per fresh fix, coalescing rapid
updates to keep at least480ms ON. Bluetooth reports phone battery: below20%
yellow, below15% red, below5% red/white. Charging takes priority: below75%
green/white, otherwise steady green. Each alternating color lasts0.5seconds;
the existing active-call glyph retains priority.

Use the matching0.9.60 APK and ZIP. Ordinary Product updates preserve user data;
Gate and all non-Product payloads match0.9.59. Current Korean/English manuals and
all six bundled help languages cover controls, first installation, updates and
unknown-result recovery. Preserve diagnostic evidence before retrying a paused
operation.554 Android tests and ARM/EVE software checks pass. The user's0.9.59
stock-to-CFW and fuel-veil success is recorded;0.9.60 has no physical vehicle test.
