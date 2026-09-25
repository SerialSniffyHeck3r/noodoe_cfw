# 주행 UI·전화·독립 진단 구현

작성: 2026-09-23. 최종 바이너리·APK 해시와 시험 수치는 `verification-summary.json`을 기준으로 한다.

## 구현

- GPS 격자는 사용자 최종 지시에 따라 **반투명 실선**이다. 실제 거리 좌표에 고정되며 궤적과 함께 이동·회전·줌한다. 위치는 300ms, 방향은 최단 회전으로 보간한다. 표시 보간을 주행거리 계산에 사용하지 않는다. EVE/LVGL 선 드라이버는 수정하지 않았다.
- 링 22px, 버튼 힌트 2px 안쪽, 트립 거리 40px·단위 24px. HOME 숫자는 외장 D-DIN 160px 원본을 현재 값만 A4 버퍼에 합성한다. 3자리 고정 위치와 리딩 제로 제거를 유지한다.
- 큰 연료 경고는 Material Rounded 원본의 도형을 LVGL 기본 도형으로 재구성해 표시 크기로 그린다. 작은 비트맵 확대를 제거했다. 자동 SVG 변환기나 SVG와의 픽셀 단위 동일성을 주장하지 않는다.
- 일반 LONG 800ms. PH9 HIGH/IGN ON만 일반 조작 허용, LOW에서 누름 취소·release 재무장·`Switch in Dash Mode` 토스트. Gate/설치/긴급 복구는 별도 확인 시간을 유지한다.
- 키 OFF 사진 설정은 기본 일반 배경 또는 슬롯 0/1/2. OFF 직후 1초 동결 → 일반 배경 20% Summary → 화면 유지 단계에서 OFF 사진. 업로드와 최종 프레임 반영이 끝나기 전 절전에 들어가지 않는다. BL을 다시 켜거나 절전 타이머를 재시작하지 않는다.
- 음악 제목 28px bold/아티스트 20px. 폰에서 고정 크기로 렌더링하고 제목 타일 두 개를 재사용한다. 1초 대기/30px·s/끝 1초 대기, 다음 타일이 없으면 정지한다. 연결·페이지·곡 세대가 다른 전송을 폐기한다.
- 전화는 음악과 GPS 사이. 즐겨찾기 10/최근 10, 전화 권한 상태·수신/진행/종료·요청 중복 방지·CJK 선택 항목. 수신 페이지 전환과 이전 페이지 복귀, PH9/IGN/설치 우선순위, 길게 받기/발신/거절·종료를 연결했다. Android 기본 다이얼러와 헤드셋 음성 경로는 유지한다.
- Android는 자동 주행/수동 중지/업데이트 상태를 분리한다. 폰 주행 기록은 기본 OFF이며 GPS·음악·알림 전송과 독립이다. CDM/foreground service와 MediaSession 감시를 서비스가 소유한다.
- 공장 `0x0800C040` 값은 PWM이 아닌 조도 임계값이다. V5.16 경로를 추적해 단계 10…100, 최종 99 clamp, TIM5 PWM1/high를 공용 BacklightPolicy에 연결했다. 보정 ±2, 단계 안정화·변화 제한·센서 오류 시 수동 fallback을 유지한다.
- 심층 HCI/조도 시험은 별도 Diagnostic 프로필로 분리했다. Product 파티션을 사용하되 역할 3/수신 대상 4/이미지 kind 1로 식별한다. Gate의 명시적인 Diagnostic 지원이 있어야 설치된다. 진단은 일반 Product 확정·설정·사진 초기화를 수행하지 않는다. 종료는 순정 복구를 안내하며, 재시작 시 독립 Gate에서 기다린다.

## 실기 검사에서 잡은 결함

Diagnostic을 Gate로 설치한 뒤 조기 복구 복귀가 발생했다. NOR 저널은 새 플래그 `16`을 허용했지만 SRAM `GateBootContext_Valid`가 이전 마스크 `15`만 허용한 것이 원인이었다. ABI enum에서 허용 비트를 구성하도록 수정했다. 실제 Diagnostic ELF의 조기 인계 함수에 정상 진단·일반 Product·trial·미지 플래그·세대 불일치·두 CRC 손상, 7개 경로를 실행해 검증했다.

수정 후 Gate가 Diagnostic 384KiB를 정확히 설치했다. 실기에서 저장소 감사/화면/식별 완료, fault 없음, 태스크 tick와 watchdog 진행을 확인했다. 재시작 후 PC가 독립 Gate 범위에 있고 Diagnostic APP이 그대로 보존되는 것도 확인했다. 이 시험은 SWD로 준비한 벤치 시험이며 Bluetooth 업로드 성공으로 간주하지 않는다.

벤치 SRAM 유지보수 도구도 기존 FreeRTOS `0xA5` 채움 값을 새 응답으로 오인하지 않도록 magic+ready를 함께 기다리게 했다. 제품 펌웨어의 변경은 아니다.

## 자원과 빌드

| 항목 | 사용/여유 |
|---|---:|
| Product Release APP | 326,960 / 66,256 B |
| Product Debug APP | 358,548 / 34,668 B |
| Release 일반 SRAM 여유 | 54,456 B |
| CCM 여유 | 16,320 B |
| Bootstrap | 453,300 B / 5,452 B 여유 |
| 독립 Gate | 22,740 B / 42,796 B 여유 |
| Diagnostic | 143,832 B / 249,384 B 여유 |

FreeRTOS 48KiB와 LVGL CCM 48KiB, 큐·스택 예산을 줄이지 않았다. Bootstrap의 순정 압축은 호스트 Zopfli 0.4.3/15회로 더 작게 만들며 실행부의 기존 raw-DEFLATE 디코더를 유지한다. 전체 순정 APP 복원, seek/손상 경계를 실제 ARM 디코더로 시험했다. 실행 시 압축기나 추가 힙을 넣지 않았다.

Product Release/Debug, Integrated, Graphics, Bootstrap, Gate, Diagnostic, Uninstall 빌드와 이미지 경계/자원 검사를 통과했다. Cube 재생성 계약 fixture는 통과했으며, 이번 턴에서 실제 GUI Generate Code를 누른 것은 아니다. 최종 빌드 프로필은 Product다. C 빌드의 기존 경고와 Android lint warning은 남아 있으며 오류 0이다.

## 검증 범위

- Android 221 tests, 실패/오류/skip 0, lint 오류 0. 기존 APK와 같은 서명.
- 실제 APK importer: 새 ZIP, 6.8.1 ZIP, 6.4 ZIP 허용. 변조한 Diagnostic ZIP 거부. ELF/BIN 일치 검사도 변조를 거부.
- Gate 기존 copy 중단 0…103, typed Diagnostic 중단, 저널/업데이터/복구 계약 ARM 모의시험. 추가 부팅 마스크 시험 1,175 assertions × O0/Os.
- 음악 타일 6,324,037 assertions × O0/Os/Oz. 큰 숫자 36개 raster 사례 × 3 최적화, 독립 원본과 픽셀 대조. PH9·밝기·GPS·전화 상태·설정·power 전환 모의시험.
- 벤치 Product 실기 화면: GPS 라이트/다크, HOME160, 연료 도형, 트립, 휴대폰 상단, 음악 제목/아티스트/재생바를 캡처. 일부는 표시용 임시 데이터이며 실제 UART/GPS/전화 연결의 증거가 아니다. 주입한 표시 상태는 복원했다.
- 정상 화면 초기 측정 평균 29.8fps, CPU 61.8%, RTOS 최소 여유 23,792B, CCM guard 오류 0. 캡처 1,696ms. 이는 단기 측정이며 8시간 부하 결과가 아니다.
- 최종 인계 수정본으로 벤치를 복원한 뒤 재측정: 29.9fps, CPU 60.6%, 캡처 1,690ms, 폴트/CCM guard 오류 0. 최종 내부 APP도 독립적으로 두 번 읽어 배포 이미지와 일치함을 확인했다.
- 내부 플래시를 두 번 읽어 설치 이미지와 대조하고 순정 하위 64KiB/공장 데이터와 FAT 메타데이터 보존을 확인했다. 벤치 유지보수는 기존 유효 은행과 복구본을 보존하는 한정된 쓰기 범위만 사용했다.

미완료인 실기 검증: 정상 무선 기판에서 SPP 전체 수명주기, 실제 폰 수신/발신/권한 철회/잠금 화면 음악, 물리 PH9×IGN 전 조합, OFF 사진 4설정 전 조합, 실제 전원 차단, 8시간 혼합 부하. 현재 벤치의 BT/조도 하드웨어 불량을 모의시험 통과로 대체하지 않았다. 실제 무선 업데이트 시간은 측정하지 않았다.

## 배포와 이관

APK 6.9.0과 대응 ZIP, 독립 Diagnostic 이미지를 함께 배포한다. 진단을 지원하지 않는 기존 Gate는 일반 Product 업데이트로 교체하지 않는다. **순정 복귀 → 새 Bootstrap → 새 Gate+CFW**를 사용한다. Diagnostic을 구 Gate에 Product로 위장해 넣는 경로는 없다. 이후 동일 Gate의 일반 CFW 업데이트는 Product만 전송한다. 새 원본 숫자 자산이 필요하므로 해당 자산이 없는 기기에 APP만 단독 설치하면 안 된다.

실제 게시 URL/SHA는 `github-release.json`, Latest/README 검증은 `github-latest.json`에 기록한다.

## 캡처

![반투명 실선 GPS](after-gps.png)
![원본 160px 숫자](after-home.png)
![음악 배치](after-music.png)
