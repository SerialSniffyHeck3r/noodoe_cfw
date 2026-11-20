# NewDoe 1.0.0.0

Install app build 171 over the existing app, then update its included firmware. Keep app data; existing settings, pairing, maps and ride records are preserved.

- Settings > ABOUT THIS PROJECT opens on black, then reveals the developer credits from top to bottom. After the reveal, press DOWN for acknowledgements; UP returns a page. The final page shows Powered by, the official ST logo, then the STM32 logo and ownership notice. Press O there to finish; long O exits early.
- The Product hidden developer Easter egg is removed. The separate Bootstrap recovery developer message is unchanged.
- Credits have wider vertical spacing. GitHub, ST and STM32 graphics live in the existing NOR resource package. The onboarding wordmark uses a lossless palette of its original pixels to recover NOR capacity without changing its appearance.
- Full component, font, icon and compiler runtime terms are in THIRD-PARTY-NOTICES.txt, every installer ZIP, and the app's Firmware acknowledgements page. Existing Android dependency notices and replaceable-library kit remain included.
- Release numbers now use major.minor.bugfix.hotfix. The fourth component is reserved for app-only changes that reuse the exact firmware catalog; firmware releases end in .0.
- Internal settings metadata is smaller; compiled catalog identity verifies unchanged keys, labels, bounds, steps and ordering. Stored settings and phone commands keep their existing formats. Other riding, navigation, weather, brightness and power behavior is retained.

Software validation only; no new physical phone, vehicle, RF or LCD testing. Existing lint findings remain; the accepted legacy stock-return power-interruption limitation and separate, unconfirmed Google Play signing migration are unchanged. Logos identify their owners and do not imply endorsement.

## 한국어

앱 빌드 171을 기존 앱 위에 설치하고 내장 펌웨어도 업데이트하세요. 앱 데이터를 지우지 않으며 설정·페어링·지도·주행 기록을 유지합니다.

- 설정의 ABOUT THIS PROJECT에서 화면이 검게 바뀐 뒤 개발자 소개가 위에서 아래로 나타납니다. 소개가 끝나면 아래 버튼으로 라이선스 페이지를 넘기며, 위 버튼으로 이전 페이지로 돌아갑니다. 마지막에는 Powered by, ST 로고, STM32 로고와 소유권 문구가 표시됩니다. 마지막 페이지의 O로 종료하고, O 길게로 중간에 나올 수 있습니다.
- Product에 숨겨져 있던 이스터 에그를 제거했습니다. 별도의 Bootstrap 복구 개발자 문구는 유지했습니다.
- 소개 첫 페이지의 세로 여백을 넓혔습니다. 공식 GitHub·ST·STM32 로고는 NOR 리소스에 저장합니다. 기존 초기 설정 워드마크는 원래 픽셀을 그대로 보존하는 팔레트로 바꿔 공간을 확보했습니다.
- 폰트·아이콘·라이브러리·컴파일러 런타임의 전체 고지문은 THIRD-PARTY-NOTICES.txt, 각 설치 ZIP, 앱의 Firmware acknowledgements에 포함됩니다. 기존 Android 의존성 고지와 라이브러리 재링크 키트도 유지합니다.
- 버전은 메이저.마이너.버그수정.핫픽스 네 자리입니다. 네 번째 자리만 바꾸는 앱 수정은 기존 펌웨어를 그대로 사용하며, 펌웨어가 바뀌는 배포의 마지막 자리는 0입니다.
- 설정 목록의 내부 저장 크기를 줄였으며 항목·순서·조절 범위·단위가 기존과 같은지 대조했습니다. 저장 데이터와 폰 명령 형식, 그 외 주행·내비·날씨·밝기·절전 동작은 유지합니다.

소프트웨어 검증이며 새로운 휴대폰·실차·무선·LCD 실측은 없습니다. 기존 lint 항목, 수락한 구형 순정 복귀 구간의 전원 중단 한계 및 별도 Google Play 서명 전환 미확인 상태는 그대로입니다. 로고는 해당 소유자를 표시하며 본 프로젝트에 대한 보증을 뜻하지 않습니다.
