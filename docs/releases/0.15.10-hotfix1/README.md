# NewDoe 0.15.10 hotfix1 — App build 159

Install **NewDoe-0.15.10-hotfix1.apk** over the existing app. Do not uninstall or clear app data. The package and signing key are unchanged. Firmware, resources and installer ZIPs remain byte-identical to 0.15.10: **a dashboard already running 0.15.10 does not need another firmware installation**. Earlier firmware can still use the matching packages included in this APK.

- Fixes Weather, Map, photo selection and other shared buttons ignoring taps while automatic connection is running. Returning to the app and Bluetooth state changes use the same availability rule as the click handler. Actual firmware/NOR transactions remain serialized, with existing confirmations and handoff checks.
- With Weather enabled, a verified NewDoe connection triggers one refresh as soon as a recent location and the selected provider API key are available. The result is sent to Noodoe even if another page is open.
- Later automatic requests retain the instrument Weather-page rule: one hour or 30 km since the last request. Page changes do not restart the interval or cancel the initial connection refresh.
- No automatic or manual weather API requests while disconnected. Cached data remains visible with its update time. Disconnecting cancels queued work and stops further requests in an active refresh. A request already sent to the provider may already count toward its quota.
- Missing location/key waits without fabricated data or a repeated request loop. Weather OFF still prevents automatic fetching. Manual checks require the live connection and retain the one-minute retry limit. One Call uses one request per refresh; Basic API 2.5 uses the current-weather and forecast endpoints, two requests per successful refresh. Provider account charges and limits still apply.

Validation covers actual Activity clicks, resume/Bluetooth state changes, maintenance handoff, weather request gating/cancellation and repository-to-NDCP delivery with a simulated provider and radio. No live provider key, physical phone/vehicle Bluetooth or LCD test was performed. The 0.15.10 firmware and its existing Gate/stock-return power-interruption limitations are unchanged. Existing lint findings are recorded separately; they are not reported as a clean full lint run.

---

# 한국어

**NewDoe-0.15.10-hotfix1.apk(빌드 159)**를 기존 앱 위에 설치하세요. 앱 삭제·데이터 초기화·재페어링은 필요하지 않습니다. 기존 패키지명과 서명을 유지합니다. 내장 펌웨어·자원·설치 ZIP은 0.15.10과 바이트 단위로 동일하므로 **본체가 이미 0.15.10이면 본체 펌웨어를 다시 설치하지 않아도 됩니다.** 구형 펌웨어의 업데이트에는 APK에 들어 있는 대응 패키지를 사용합니다.

- 자동 연결 중 날씨·지도·사진 선택 등 공통 버튼을 눌러도 반응하지 않던 문제를 수정했습니다. 앱으로 돌아오거나 Bluetooth 상태가 바뀌어도 같은 활성화 조건을 적용합니다. 실제 펌웨어/NOR 작업의 중복 실행 방지와 확인 절차는 유지합니다.
- 날씨 사용을 켜 두면 NewDoe 연결 확인 후 최근 위치와 API 키가 준비되는 즉시 한 번 갱신하고 본체로 전송합니다. 본체가 다른 페이지에 있어도 전달합니다.
- 이후 자동 조회는 본체 날씨 화면에서 마지막 조회 후 1시간 또는 30km 이동 조건을 따릅니다. 화면 전환으로 간격을 초기화하지 않으며 첫 연결 조회는 다른 페이지로 가도 완료합니다.
- 미연결 상태에서는 자동·수동 날씨 API를 호출하지 않습니다. 기존 정보는 갱신 시각과 함께 볼 수 있습니다. 연결이 끊기면 대기 요청과 진행 중인 후속 요청을 취소합니다. 이미 제공업체에 보낸 요청의 할당량까지 되돌릴 수는 없습니다.
- 위치나 키가 없으면 기다립니다. 날씨 OFF는 자동 조회를 하지 않으며, 수동 확인도 연결 중에만 1분 간격으로 가능합니다. One Call은 갱신당 1회, Basic API 2.5는 정상 갱신당 현재 날씨·예보 2회 요청입니다.

버튼 조작과 복귀, 유지보수 연결 인계, 날씨 조회 차단·취소·NDCP 전송은 모의 제공업체/통신을 이용한 소프트웨어 검사입니다. 실제 API 키 조회·휴대폰/실차 Bluetooth·LCD 시험은 하지 못했습니다. 0.15.10 펌웨어의 기존 Gate·순정 복귀 전원 차단 한계도 그대로입니다.
