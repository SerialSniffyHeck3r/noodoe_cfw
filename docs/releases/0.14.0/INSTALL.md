# NewDoe 0.14.0 beta

앱에서 본체의 시스템 언어를 선택해 설치합니다. 별도 언어팩을 받을 필요 없이, 선택한 언어의 펌웨어·기본 자원·필요한 글꼴을 APK에서 준비합니다.

1. **NewDoe-0.14.0.apk**를 기존 앱 위에 설치하세요. 앱 데이터는 유지하세요.
2. 차량을 정차한 뒤 **현재 기기 상태 확인**을 누릅니다.
3. **기기 시스템 언어**를 고르고 **설치 / 업데이트**를 누릅니다. 처음 순정에서 설치한다면 설치 마법사를 사용할 수 있습니다.
4. 안전 안내를 확인한 뒤 화면 안내를 따릅니다. 완료될 때까지 배터리를 분리하지 마세요.

| 본체 자원 | 선택 가능한 시스템 언어 |
|---|---|
| A | 영어·일본어·중국어 간체·번체 대만·번체 홍콩 |
| B | 영어·독일어·스페인어·프랑스어·이탈리아어·그리스어·한국어·베트남어·일본어 |

본체에는 영어 또는 영어와 선택한 한 언어가 설치됩니다. **설정 → 시스템 → 영어 전용**으로 영어와 설치 언어를 전환하고 정차 상태에서 재시작하세요. 다른 언어를 원하면 앱에서 다시 선택해 업데이트하면 됩니다. 일반적인 언어 변경에는 순정 복귀가 필요하지 않습니다.

정상적인 기존 B 글꼴은 그대로 사용합니다. 파일 준비가 필요한 경우에는 앱이 전환용 펌웨어를 잠시 설치하고, 글꼴을 준비한 뒤 선택한 최종 펌웨어로 돌아옵니다. Bluetooth 페어링·설정·트립·정비·사진은 유지합니다. 중간에 끊기면 앱 데이터를 지우지 말고 **현재 기기 상태 확인 → 이어가기**를 사용하세요. 전환용 파일을 직접 선택할 필요는 없습니다.

NOR 공간이 부족하면 CJ 글꼴을 건너뜁니다. 다른 파일을 지워 공간을 만들지 않습니다. 선택한 메뉴 언어의 필수 글꼴도 없으면 영어로 열리며 앱에 이유가 표시됩니다. 음악·알림은 한 글자라도 본체에서 표시할 수 없으면 해당 문자열 전체를 휴대폰에서 그립니다. 영어 전용 설치에서도 이 기능을 사용할 수 있습니다.

HOME에 아날로그·디지털 시계가 추가됐습니다. 연료 경고를 원하지 않으면 **설정 → 차량 → 연료 알림**을 끌 수 있으며, 이때 SINCE REFUEL에서 O를 길게 눌러 수동으로 초기화할 수 있습니다. 연료 진단에서는 계기판이 보낸 원시 값을 확인할 수 있습니다.

Gate는 기존 호환 버전을 유지합니다. 구형 Gate만 앱이 안내하는 **데이터 보존 순정 복귀 → 재설치**를 따르세요. 기존 구형 코드가 수행하는 순정 복귀 중에는 전원을 차단하지 마세요.

일반 사용자는 APK만 설치하면 됩니다. 언어별 ZIP은 **0.14.0 앱의 전문가용 설치**를 위한 파일입니다. A/B를 임의로 추측해 고르지 말고 앱에서 기기를 확인하세요.

[한국어 그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md) · [English illustrated guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)

## English

Install **NewDoe-0.14.0.apk** over the existing app, preserving its data. While parked, check the actual device, choose **Device system language**, then install or update and follow the safety screens. The APK contains the firmware, matching base resources and required supplemental fonts.

A units offer English, Japanese, Simplified Chinese and Traditional Chinese for Taiwan or Hong Kong. B units retain English, German, Spanish, French, Italian, Greek, Korean, Vietnamese and Japanese. Each Product contains English alone or English plus one selected menu language. **Settings → System → English only** switches between them after a stationary restart. Choose a different installed language through the app; ordinary language changes do not require returning to stock.

Healthy existing B fonts are reused. Only when storage preparation is needed does the app temporarily install preparation firmware, prepare the fonts, then return to the selected final firmware. Pairing, settings, trips, maintenance and photos are retained. After an interruption, keep the app data and use **Check current device → Continue**. Do not select preparation firmware manually.

If NOR space is insufficient, CJ is skipped without deleting stock or user files. Missing required menu fonts result in English menus with the reason shown in the app. Unsupported music/notification fields are rendered entirely on the phone. English-only menu firmware retains this external-text support.

HOME now offers analog and digital clocks. Fuel notifications can be disabled in vehicle settings; with fuel notifications disabled, hold O on SINCE REFUEL to reset it manually. Fuel diagnostics expose the raw dashboard input.

The compatible Gate is retained. Only older incompatible Gate versions use the app's keep-data stock-return/reinstall path. Keep power connected during that legacy stock-return operation.

Normal installation needs only the APK. Per-language ZIPs are for expert installation with the **0.14.0 app**; use actual device identification rather than guessing A/B.
