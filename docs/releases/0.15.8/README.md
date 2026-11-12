# NewDoe 0.15.8 beta

App / firmware 0.15.8 · Android build 154

## 한국어

APK를 기존 앱 위에 설치하고 **기기 → 현재 상태 확인 → 설치 계속 / 언어 확인**으로 일치하는 본체 펌웨어와 자원을 업데이트하세요. 일반 설치에 별도 ZIP 선택은 필요 없습니다. 기존 미완료 작업이 있으면 먼저 결과를 확인하세요. 전문가용 ZIP은 기기 A/B 프로필과 언어가 맞아야 합니다.

- 내비 안내 중 본체 지도는 50m보다 넓게 축소되지 않습니다. NAV는 축척 제한이 적용됐다는 뜻입니다. 안내 종료 시 저장된 축척이 다시 적용되고, 휴대폰 큰 지도는 계속 확대·축소할 수 있습니다. 작은 도로는 밀집도에 따라 줄이며 주요 도로와 경로를 우선 표시합니다.
- 현재 TBT와 더 작은 다음 TBT를 각각 둥근 사각형에 표시합니다. 다음 안내의 거리는 현재 회전 지점부터 다음 회전 지점까지입니다. 다음 안내가 없으면 아래 카드는 숨습니다. 내비 사용 중이라는 반복 문구는 제거하고 회전교차로 출구 번호는 유지합니다.
- 재탐색은 가운데 원형 화살표로 표시합니다. 상단 메뉴가 숨으면 전체 TBT 화면을 제외한 TRIP 등 주행 페이지에 미니 TBT가 나타납니다. TRIP 제목·시간·비율 바·거리 간격을 조정하고 최고·평균속도 줄은 유지했습니다. MAP의 목적지 거리도 정렬했습니다.
- 별도 대형 나침반 모드는 제거했습니다. 안내가 꺼진 HOME TBT에는 내비게이션 비활성을 표시합니다. 기존 정보 줄의 작은 나침반은 유지합니다.
- 빠른 답장에 목적지·남은 거리·예상 소요시간/도착시간 공유 항목을 추가했습니다. 폰에서 계산해 기존 답장 기능으로 전송하며, 본체는 선택 명령만 보냅니다. 현재 내장 안내에 적용되고 오래된 위치나 안내 없음 상태에서는 보내지 않습니다. 지원되는 알림에서 사용자가 직접 선택해야 합니다.
- Google Maps/OsmAnd의 현 알림 연동에서는 그 다음 회전 방향을 확실히 얻을 수 없어 현재 TBT만 표시합니다. 알려지지 않은 방향이나 외부 앱의 ETA를 추측하지 않습니다.

Gate·복구 로직·Bluetooth 연결·기존 NOR 파일 이름과 배치는 유지합니다. 재탐색 화살표는 기존 자원 파일 안에 넣었습니다. 설정·TRIP·정비·사진·페어링을 정리 목적으로 지우지 않습니다.

인천시청 부근 실제 지도·경로 데이터를 사용한 소프트웨어 시험과 LVGL/EVE 렌더링 결과입니다. 이번 작업에서 실차·실물 LCD·무선 연결은 시험하지 않았으며, 제보된 찢어짐 해소는 실차 재확인이 필요합니다. 기존 순정 복귀 구간의 전원 차단 제한을 해결한 릴리스가 아닙니다.

[한국어 그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md)

## English

Install the APK over the existing app, then use **Device → Check current status → Continue installation / check language** to install its matching firmware and resources. Normal installation does not require choosing a ZIP. Reconcile any unfinished operation first. Expert ZIPs must match the actual A/B stock profile and selected menu language.

- During guidance, the dashboard cannot zoom out beyond the 50 m scale. NAV indicates the limit. Ending guidance restores the saved scale; the phone's large map retains its zoom controls. Dense minor roads are reduced while main roads and the route are prioritized.
- The current maneuver and smaller following maneuver have separate rounded cards. The lower distance is from the current maneuver to the following one. No following maneuver means no lower card. The repeated mode caption is removed; roundabout exit numbers remain.
- A centered circular-arrow icon indicates recalculation. Mini TBT appears on riding pages including TRIP after the menu icons hide, except on the full TBT page. TRIP title/time/bar/distance spacing and MAP destination alignment are adjusted; the maximum/average speed line stays in place.
- The separate large compass is removed. HOME TBT shows Navigation inactive when guidance is off. The information strip's small compass remains.
- Quick reply adds destination, remaining distance and estimated duration/arrival time. The phone composes the message for the existing reply action; the dashboard only sends the selection. This currently uses built-in guidance and rejects stale location or absent guidance. It requires an explicit selection on a reply-capable notification.
- Google Maps/OsmAnd currently expose no reliable following-turn direction through this notification integration, so only the current maneuver is shown. Unknown directions and external-app ETAs are not invented.

Gate, recovery, Bluetooth transport and existing NOR file names/layout remain unchanged. The rerouting arrow fits the existing resource file. No cleanup of settings, trips, maintenance, photos or pairing is performed.

Validation uses real map/route data near Incheon City Hall in software and actual LVGL/EVE rendering. No physical vehicle, LCD or wireless test was performed this release; elimination of the reported tearing still needs vehicle confirmation. The accepted legacy stock-return power-interruption limitation remains.

[English illustrated guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)
