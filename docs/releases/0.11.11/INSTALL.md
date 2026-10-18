# NewDoe0.11.11 beta — Android dashboard

Full-width dark dashboard, horizontal oil gauge with remaining distance, tap-to-cycle trips, editable stock UI preview and uncropped transparent NEWDOE wordmark. **APK-only update for devices already on Product0.11.10. Firmware ZIP0.11.10 and language resources are unchanged.**

## Install

Update the APK over the existing app; its signature and saved settings are preserved. If Noodoe already runs Product0.11.10, do not reinstall firmware or language resources for this change. Older devices use the accompanying unchanged NewDoe-CFW-0.11.10.zip through the existing installer. Gate, Bootstrap, pairing, trial confirmation and firmware protocol are unchanged. The previously accepted old-code stock-return power-interruption limitation remains; this app update does not change that interval.

Home now has full-width dark rectangular instrument sections. Tap the trip information area to advance A → B → TODAY → THIS SESSION; bottom trip arrows are removed. Horizontal swipes retain page navigation. Oil has a larger horizontal pill bar and remaining km (or mi with selected device units); belt and service remain on its right. Day/hour-only intervals retain their applicable units. These are service intervals, not measured physical oil quantity.

Personalize → UI test screen: pause the connection first. All four display modes are visible buttons, including Stock KYMCO. Edit test values in stock mode to set ignition, speed, ODO, peak, stopped time, model, firmware, series and PCBA. Stock remains metric. Samples stay screen-local and never write device data or riding history. Real work closes the preview.

The original white/red wordmark is retained. Correct2172×724 source bounds show all letters without a separate background rectangle.

## 한국어

어두운 화면을 가로 폭에 맞추고, 오일 가로 막대·남은 거리, 트립 터치 순환, 순정 정보 테스트, 잘리지 않는 투명 NEWDOE 워드마크를 적용했어요. **누도가 이미0.11.10이면 APK만 설치하세요. 펌웨어ZIP0.11.10과 언어 자원은 그대로예요.**

트립 정보 영역을 눌러 A → B → 오늘 → 이번 시동 순서로 넘겨요. 하단 트립 버튼은 없어요. 좌우로 밀면 기존처럼 앱 페이지가 바뀌어요. 오일도 가로 막대와 남은 km를 보여주고, 벨트·정비는 오른쪽에 나와요. 오일량 센서가 아니라 교체 주기 표시예요.

꾸미기 → UI 테스트 화면에서 순정 KYMCO 버튼을 누르면 순정 정보도 볼 수 있어요. 테스트 값 바꾸기로 시동·속도·ODO·최고 속도·정차 시간·모델·펌웨어·시리즈·PCBA를 입력해 보세요. 연결이 중지된 상태에서만 사용할 수 있고 누도나 주행 기록에는 보내지 않아요.

기존 앱 위에 APK를 설치하세요. 누도가0.11.10이면 이것만으로 적용돼요. 구버전은 기존 설치 절차대로 함께 제공하는0.11.10 ZIP을 사용해요. Gate·Bootstrap·페어링·언어 자원은 이번에 바꾸지 않았어요. 이미 안내한 구형 코드의 순정 복귀 중 전원 중단 제한도 그대로예요.

## Validation / 검증

166 Android tests passed in 20 suites, including dashboard touch/readouts, preview isolation, actual firmware importer, installation, bond, handoff and reconnect regressions. Screenshot previews use Robolectric native Android graphics and sample data. No new Noodoe, RF or vehicle qualification. Wireless APK installation: not performed; phone unavailable.

![Home preview](https://raw.githubusercontent.com/SerialSniffyHeck3r/noodoe_cfw/main/docs/releases/0.11.11/home.png)
![Stock information preview](https://raw.githubusercontent.com/SerialSniffyHeck3r/noodoe_cfw/main/docs/releases/0.11.11/test-stock.png)
