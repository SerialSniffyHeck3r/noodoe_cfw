# Product UI·음악 전송·Debug 메뉴 검증 — 2026-09-22

사용자가 6.5.2 실차 설치 성공을 확인한 다음 요청한 UI 작업이다. OpenNoodoe 원본과 순정 BL은 수정하지 않았다. GUI 조작 없이 Android native Canvas 테스트와 벤치의 EVE 캡처를 사용했다.

## 변경과 원인

| 항목 | 변경 |
|---|---|
| 상태 아이콘 | 24px 크기 유지. BT X120→112, GPS X336→344, 공통 Y78→73 |
| 폰 패널 | 중앙 body Y145 기준 local Y50→6. 패널은 288×128 유지. 헤더 캔버스 시작 absolute Y195→151 |
| 음악 문자 | 304×72 A4 형식 유지. local Y137→120, 표시 범위 absolute Y265–336. 제목 최대32→28px, 아티스트24→20px. fallback24/20px |
| 음악 재생바 | absolute Y354–359 유지. A4 마지막 8행 비워 글자가 bar에 닿지 않음 |
| 라이브 JPEG | 기존에도 480 크롭이 있었음. 이번에는 별도 라이브 인코더로 480×480·48KiB 제한, 시작 quality80. 영구 사진은 기존128KiB 예산 |
| 음악 키 | panel/reply/notification에서 증가하는 공용 visualKey를 음악 재게시에도 사용하던 것이 결함. musicVisualKey를 트랙/이미지 변경 때만 새로 할당하여 완성된 mask/art와 metadata가 같은 키 유지 |
| 전송 우선순위 | 메타데이터 갱신을 알림 큐 구성보다 먼저 실행. 트랙 변경 시 오래된 음악 RAM 업로드를 버리고 최신 곡으로 전환. 독립 panel 전송과 기기의 BUSY/검증 계약 유지 |
| 앱 | 서비스는 그대로 단일 소켓 소유. 화면만 주행/꾸미기/설치 및 접힌 권한·복구/진단으로 분리. 테마/선택 탭 복원, 빈 진행 영역 제거. 주행 연결 종료 버튼은 주행 연결에서만 활성화 |
| Debug | App_Logic/Settings/src/settings_debug.c. 캐시 조회만 사용하며 버스 probe/restart/NOR 쓰기 없음. 14개 읽기 전용 항목, 설정 영구 ID/배열에 추가하지 않음. raw/유효성/stale/오류를 구분 |

## 실기 증거

- `identity.json`, `internal-before.bin`: 벤치 UID `0039001f 3436510f 35373339`, 실행된 구버전 ELF를 내부 플래시와 대조한 후 RAM 주소를 사용했다.
- `bench-write-evidence-*/result.json`: 비활성 CFW bank를 기록·물리 읽기 대조, 기존 active 보존, 저널 commit. 내부 Product 영역만 기록하고 전체512KiB 독립 읽기2회 대조. 순정64KiB와 기존 Gate64KiB는 바이트 동일. FAT 동일. 이것은 로컬 SWD 정비이며 무선 trial-confirmation 성공을 주장하지 않는다.
- `before.png` / `after.png`: 상태 아이콘 위치. after는 부팅 초기 사진 로딩 전이며 사진이 사라진 회귀가 아니다. 이후 실기 페이지 캡처에서 기존 배경 복원을 확인했다.
- `before-phone.png` / `after-phone.png`: RAM 임시87%·Pixel9 header로 위쪽 위치 비교.
- `before-music.png` / `after-music.png`: 동일 영어 fallback 데이터로 글자/재생바 간격 비교.
- `after-music-cjk-unique.png`: 새 Android 렌더러가 만든 번체 중국어 A4를 실기에서 표시. `after-music-cjk.png`는 최초 시험 fixture가 panel과 mask의 revision=1을 재사용하여 복사를 생략한 **무효 fixture 캡처**다. 실제 서비스의 전역 revision 계약에 맞춰 고친 `preview.py`와 unique 캡처가 최종 근거다.
- `debug-live.png`, `debug-ambient.png`: 실기 Debug 메뉴. 메뉴 열림에 필요한 stationary freshness만 임시RAM 주입, BT/조도 값은 실제 캐시. BT err0x302, ALS error3/No sample을 성공으로 위장하지 않음. 이전 표시/신호 캐시는 finally에서 복원.
- `after-stable-status.json`: 29.9FPS, CPU 비유휴59.8%, RTOS 힙 최소23,720B, CCM guard0, 활성 system error0/fault signature0. 샘플 창 측정이며 8시간 부하 시험으로 주장하지 않음.

## 빌드·시험·경계

| 자원 | Release | Debug |
|---|---:|---:|
| APP 사용/여유 | 327,040 / 66,176B | 359,072 / 34,144B |
| 일반 SRAM 여유 | 56,352B | 54,888B |
| CCM 여유 | 16,320B | 16,320B |

기존64/32KiB APP 예산 통과. Release 여유는 예산 위640B이므로 다음 기능 확대 전 코드 예산을 확인해야 한다. 힙/스택/큐를 축소하지 않았다. Release의 기존 misleading-indentation/array-parameter 경고와 링커 RWX/LTO 경고는 이번 변경 외 기존 위치이며, 새 settings_debug의 경고는 정리했다.

`settings-test.log`: 실제 ARM O0/Os 각각316 assertions. Debug는 읽기 전용·설정 변경 불가·미수신/stale 표시·Back·메뉴 탐색 검증.

Android: `android-final.log`, `android-layout-final.log`, 최종 `android-verified.log`. stable music key/곡 변경,480 JPEG byte budget, CJK mask 밑 여백, 화면 native 렌더/테마/재생성, 기존 설치 회귀시험. 파일 형식·파일별 해시는 `verification-summary.json`; 동일 서명은 `apk-signature.txt`; 생산용 ZIP importer는 `bundle-import.txt`.

이번 배포는6.5.2 Bootstrap/Gate/순정 복구/자산과 바이트 동일하고 Product만 변경했다. 최종 ZIP은 새 Product와6.5.2 Gate를 포함한다. 벤치에서는 현재 Gate를 유지했으므로 벤치용 first-install.bin은 배포용이 아니다.

정상 RF에서 앨범아트 지연·실차 GPS/알림 동시 부하, 물리 IGN 절전, 장시간 운행은 미검증이다. 변경된 전송 용량/키 처리와 모의시험을 실제 RF 실측으로 표현하지 않는다.
