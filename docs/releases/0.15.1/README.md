# NewDoe 0.15.1 beta

## 한국어

**NewDoe-0.15.1.apk**를 기존 앱 위에 설치하세요. APK에는 같은 버전의 A/B·언어별 펌웨어, 자원과 필요한 폰트 준비용 펌웨어가 들어 있습니다.

- 구버전 기기에 연결하면 업데이트 안내가 나옵니다. **기기 → 현재 기기 상태 확인 → 언어와 작업 확인 → 업데이트**로 진행하세요. APK 설치만으로 본체를 자동 업데이트하지 않습니다. 주행 연동은 이 APK와 일치하는 펌웨어의 정상 부팅 확정 후 시작합니다.
- 본체 **설정 → 차량 → Fuel recognition(연료량 인식)** 또는 앱의 기기 설정에서 끌 수 있습니다. OFF는 연료 경고뿐 아니라 **자동 주유 인식과 연료 예비 구간 처리 전체를 중지**합니다. 연료값을 보내지 않는 차량에서 사용하세요. 차종·연식만 보고 자동으로 끄지 않습니다.
- OFF여도 TRIP·정비 기록은 유지됩니다. SINCE REFUEL 화면에서 **O 길게 누르기**로 직접 초기화할 수 있습니다. 다시 ON으로 켜면 현재 값부터 기준을 잡으며, OFF 중의 변화를 주유로 소급 처리하지 않습니다. RAW 진단은 계속 볼 수 있습니다.
- 자체 오프라인 내비게이션은 **BRouter만** 포함합니다. 공식 `.rd5` 경로 파일과 별도 `.map` 표시 지도를 준비하세요. 도로명 자리에는 **BRouter 모드**가 표시됩니다. Google Maps·OsmAnd 외부 TBT는 실험 기능으로 유지합니다. Valhalla·국가 팩 생성/배포는 이번 빌드에서 제외합니다.
- 일출·일몰은 설정에서 고른 도시를 기준으로 대략 계산합니다. GPS가 기준 도시를 자동 변경하지 않습니다.

중단된 설치는 원래 기기·파일 기록을 유지합니다. 앱 데이터를 지우거나 다른 ZIP으로 덮어 시작하지 말고 먼저 현재 상태를 확인하세요. 정상 기기는 Product 업데이트를 사용하며 Gate 교체·전체 초기화를 요구하지 않습니다. 0.14.1의 설정 저장/복원 오류로 차단된 기기는 기존 **데이터 보존 순정 복귀 → 재설치** 안내를 따르세요. CJ 공간 부족에서는 제한을 읽고 계속/취소 또는 다른 언어를 직접 선택합니다.

설치 중 배터리 단자·배선을 분리하거나 스타터를 누르지 마세요. 각 단계의 키 ON/OFF 안내를 따르세요. **기존 순정 복귀 코드 실행 구간의 전원 차단 제한은 그대로입니다.** Gate 바이너리·ABI·NOR 파일 배치는 변경하지 않았습니다.

[한국어 그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md)

## English

Install **NewDoe-0.15.1.apk** over your existing app. Matching A/B language firmware, resources and the font preparation firmware are included.

- An older device receives an update notice. Open **Device → Check current device status**, review the language and operation, then update. Installing the APK never automatically flashes Noodoe. Riding sync starts after the matching firmware is confirmed.
- Disable **Settings → Vehicle → Fuel recognition**, or the equivalent phone setting, if the dashboard does not supply usable fuel data. OFF stops **automatic refuel detection, fuel warnings and reserve handling**. It is not merely a notification mute and is never selected automatically from model/year.
- Trip and maintenance records remain. Hold **O on SINCE REFUEL** to reset it manually while recognition is OFF. Re-enabling starts a fresh baseline; changes during the disabled interval are not counted as refuelling. RAW diagnostics remain available.
- Built-in offline routing uses **BRouter only**. Obtain official `.rd5` routing files and a separate `.map` display map. The road-name line says **BRouter mode**. Experimental Google Maps/OsmAnd external TBT remains. Valhalla and country-pack generation/distribution are excluded from this build.
- Sunrise/sunset uses a manually selected reference city. Live GPS does not change that city.

Interrupted installations keep their original device/file records. Keep app data and check the current device before resuming. Healthy devices use a Product update without replacing Gate or wiping data. Devices blocked by the 0.14.1 settings defect use the existing **data-preserving stock return → reinstall** procedure. Insufficient CJ space waits for an explicit continue/cancel or language choice.

Keep power connected and do not press the starter during installation. Follow the key ON/OFF instructions for each step. **The existing stock-return power-interruption limitation remains.** Gate binaries, ABI and NOR file layout are unchanged.

[English illustrated guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)

Software builds, emulation and automated tests are documented separately. This release does not claim a physical vehicle test or that every reported Samsung/POCO reconnection fault has been reproduced and resolved.

## 0.15.1 정비 기준 / Maintenance criteria

오일은 ODO 거리만 사용해요. 모든 정비 항목에서 시동 시간을 더 이상 반영하지 않아요. 기존 정비 거리는 유지하고 옛 시간값은 무시해요. 시간만 설정했었다면 거리 주기를 지정해 주세요. 벨트·정기 점검의 날짜 기준은 계속 사용할 수 있어요.

Oil uses odometer distance only. Key-ON time is no longer used for any maintenance item. Existing service distances are kept; old hours are ignored. If only hours were configured, set a distance interval. Belt and service calendar reminders remain available.
