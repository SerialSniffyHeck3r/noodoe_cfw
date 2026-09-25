# Noodoe LVGL / EVE 그래픽

LVGL 9.5.0의 EVE 명령 렌더러를 기존 Noodoe BSP에 연결한다. 소프트웨어 framebuffer 렌더러는 사용하지 않는다. LVGL 원본/출처는 `Middlewares/Third_Party/LVGL`, 보드 연결은 `Graphics/Port`, 기능별 시험은 `Graphics/UI`에 있다. 제품 상태 머신·버튼 정책·표시 모델은 `App_Logic/UI`에 두고, 이 계층은 렌더링과 그래픽 시험을 맡는다.

30fps 목표, CPU/FPS 실측 표시, 원형 표시 영역의 API·계측 의미는 [PERFORMANCE.md](PERFORMANCE.md)에 있다. 현재 시험은 정적 화면도30fps로 제출하는 연속 렌더 시험을 사용한다.

활성 영역은 사용자 지정 USB 사진의 원으로 고정했다. 좌표와 EVE 출력 제한은 [VIEWPORT.md](VIEWPORT.md)를 따른다. 현재 부팅하면 큰270° 호가4초 동안 채워지고4초 동안 비워지는 시험을 유지한다. DOWN을 누르면 기존41개 기능 순회를 재개한다.

## 실행과 소유권

main의 strong `StartDefaultTask`는 계속 `LCDTest();`만 호출한다. 기본 LCDTest는 Graphics 초기화 → 시험 초기화 → Graphics_Process/GraphicsTest_Process 반복을 실행한다. 기존 창·버튼 40×40 칸·25%/OFF 깜빡임은 `LCDTestLegacy()`에 보존했고 `LCD_TEST_USE_LEGACY=1`로 선택할 수 있다. LVGL 시험의 백라이트는 25% 고정이다.

LVGL과 Graphics API는 **초기화한 RTOS 태스크 하나**에서 호출한다. 다른 태스크는 메시지/큐로 변경 요청을 전달하고 ISR에서 LVGL, SPI, 대기 함수를 호출하지 않는다. 이 프로젝트는 RTOS 자체는 사용하지만 LVGL 내부 OS 설정은 NONE이며 단일 소유 태스크로 동기화한다.

이 포트는 Noodoe의480×480 display 하나를 소유한다. 상위 UI는 반환된 display/indev를 직접 삭제하거나 두 번째 display를 생성하지 않는다. 상위에서 생성한 화면·위젯은 상위가 소유하며, 포트 종료는 Graphics_Shutdown으로 수행한다.

종료 순서는 `GraphicsTest_Shutdown()` → `Graphics_Shutdown()`이다. 제품 UI도 자신이 소유한 객체/타이머/animation을 먼저 정리한다. Shutdown 뒤 예전 LVGL 객체 포인터는 사용할 수 없다.

## 상위 API

`Graphics.h`가 LVGL 공개 API도 포함하므로 `lv_label_*`, `lv_arc_*`, `lv_obj_*`, `lv_anim_*` 등을 그대로 쓸 수 있다. 기능별 API를 BSP에 다시 구현할 필요는 없다.

| 목적 | 공개 API |
|---|---|
| 수명·주기 | Graphics_Init, Process, Shutdown, IsReady |
| 지원 범위 | Graphics_GetCapabilities |
| 화면/객체 진입 | Graphics_GetDisplay, GetScreen, LoadScreen, Invalidate |
| 밝기/갱신 | Graphics_SetBrightnessPercent, GetBrightnessPercent, SetRefreshPeriodMs |
| 물리 버튼/포커스 | Graphics_GetInput, GetFocusGroup, SetInputEnabled, FocusNext, FocusPrevious, SetEditing |
| 버튼 시간 이벤트 | Graphics_SetButtonCallback |
| 이미지/폰트 준비 | Graphics_PreloadImage, PreloadText |
| GPU 자원 | Graphics_ClearAssetCache, GetAssetBytesUsed, GetAssetBytesFree |
| 계측 | Graphics_GetDiagnostics, CopyDiagnostics |
| 성능 | Graphics_GetPerformance, SetTargetFPS, SetContinuousRendering |
| 원형 배치 | Graphics_IsPointVisible, IsAreaVisible, IsCircleVisible, AreaIntersectsVisible, GetSafeArea |
| 호 시험 | GraphicsArcSweep_InitViewport, Init, Process, Destroy, GetDiagnostics |
| 시험 제어 | GraphicsTest_Init, Shutdown, Process, Select, Next, Previous, SetAutoAdvance, SetCasePinned, IsCasePinned |
| 시험 조회 | GraphicsTest_GetCaseCount, GetCaseInfo, GetResults, GetDiagnostics |

예: 같은 소유 태스크에서 `lv_label_create(Graphics_GetScreen())`, `lv_label_set_text(label, "120")`로 문자열을 그린다. 객체 생성·값 변경은 LVGL이 invalidate하며 반복 호출하는 `Graphics_Process()`가 화면에 제출한다. 디스플레이 초기화나 SPI 전송을 UI에서 직접 하지 않는다.

`Graphics_SetInputEnabled(0)`은 LVGL encoder 전달만 끈다. 버튼 관측 콜백은 계속 PRESS/RELEASE/SHORT/LONG/VERY_LONG과 ms 단위 시간을 전달한다. UP=PD12, DOWN=PI6, ENTER=PA15 매핑은 유지한다. 부팅 시 눌린 버튼은 한번 놓고 새로 눌러야 UI 명령을 낸다.

## 자원과 제약

- 480×480, SPI1/EVE FT81x generation2. 기존 BSP가 패널 타이밍·전원·백라이트를 초기화하며 upstream display factory의 추가 전원 reset/init은 실행하지 않는다.
- EVE RAM_G는 1MiB지만 현재 이미지/폰트 예산은512KiB다. 상단512KiB는 실제 화면 스냅샷용으로 예약하며 [CAPTURE.md](CAPTURE.md)에 계약을 기록한다. Display list는8KiB다. 위젯 수/문자 수/도형 복잡도가 한 화면의 제한을 결정한다. CPU framebuffer 전송이 없어도 무제한 위젯·layer를 지원하는 것은 아니다.
- LVGL heap64KiB와 FreeRTOS heap64KiB는 별개다. 그래픽 defaultTask는 RTOS heap에서12KiB 스택을 할당한다. 최초4KiB 설정은 실물 화면 전환 중 overflow가 관측돼 확대했다. IOC3072words와 생성 속성을 sync/check로 일치시킨다. LVGL heap/free/peak, GPU RAM_G, DL 크기, SPI 오류, 렌더 시간은 `g_graphics`, 태스크 최소 잔여 스택은 `g_bsp_bringup.stack_free_words`에 기록한다.
- 폰트는 EVE가 받는 4bpp Montserrat14/20/28/40이다. 현재 시험 문자열은 ASCII다. 한글·동적 폰트 로더는 이번 단계에 넣지 않았다.
- 이미지는 고정 주소의 raw `lv_image_dsc_t`를 사용한다. L8/RGB565를 직접 사용하며 RGB565A8/ARGB8888은 EVE ARGB4444로 변환한다. 파일 경로, symbol 이미지, 압축 이미지 디코더는 이 포트가 지원하지 않는다.
- `Graphics_PreloadImage`는 크기/stride/data_size/압축 여부를 확인한다. 직접 `lv_image_set_src`를 사용하는 코드도 같은 유효 descriptor 계약을 지켜야 한다. 실제 할당 크기를 C 포인터만으로 확인할 수는 없다.
- upstream EVE cache는 source data 주소를 키로 쓰고 자동 LRU 회수를 하지 않는다. source 내용은 해당 cache 수명 동안 불변이어야 한다. 시험은 소수의 정적 이미지를 재사용한다. 사진 교체 시 `Graphics_ClearAssetCache`로 전체 cache를 비우고 다음 refresh에서 다시 올릴 수 있다. 이 동작은 잠깐 검은 화면을 거치며 끊김 없는 배경 전환용 이중 buffer가 아니다.
- 그림자, 그라데이션 스타일, 임의 layer/객체 전체 opacity 합성, 둥근 child clipping, vector/canvas/file decoder 등은 EVE 경로에서 지원하지 않는다. 해당 시험은 SKIPPED로 남긴다. primitive/이미지 자체의 alpha와 객체 전체 layer opacity는 서로 다르다.
- 보드 전송 오류·FIFO fault/timeout은 최초 오류와 assert 위치를 남기고 백라이트를 끈 상태로 진단 가능한 정지에 들어간다. 자동 GPU reset으로 오류를 숨기거나 플래시를 쓰지 않는다.
- `graphics_eve_clip.c`가 `--wrap=lv_eve_scissor`로 클리핑을 소유한다. LVGL의 inclusive 영역을 정확한 XY/SIZE 두 명령으로 발행한다. 원본의 시작점/끝점 분리 캐시, SAVE/RESTORE 뒤 캐시 불일치, 경계를 넓히는 1px padding에 의존하지 않는다. 이전 화면 밖 dummy clip 우회는 제거했다. BEGIN은 saved context가 아니므로 dispatch에서 실제 primitive와 C 캐시를 RECTS로 동기화한다. 하드웨어 재초기화 후 첫 프레임에는 bitmap 기본/확장 속성도 동기화한다.
- `graphics_eve_transport.c`는 REG_CMDB_WRITE burst에만 FIFO 여유 공간 검사를 적용한다. 전송량을 credit 안으로 제한하고 부족하면 CS를 닫고 bounded wait 후 재개한다. timeout/fault는 오류로 남기며 자동 reset/명령 누락으로 감추지 않는다. 일반 RAM/레지스터 전송은 기존 경로를 유지한다. `g_eve_transport`로 wait/fault/최소 여유를 읽는다. 이번 실기기 표본에서는 FIFO 고갈이 관측되지 않았으므로 이것이 사용자에게 보인 찢어짐의 원인이라고 단정하지 않는다.

## 시험

41개 case를 기본8초마다 순환한다. 자동 모드에서는 DOWN 다음/UP 이전, ENTER를2초 넘게 누른 뒤 놓으면 수동 모드다. 수동 모드에서는 방향 버튼으로 focus/edit, ENTER로 선택하며 ENTER2초 후 놓기로 자동 모드로 복귀한다. 수동 모드에서는 합성 값 갱신을 멈춰 사용자가 편집한 값을 덮지 않는다. LVGL 자체 spinner/animation은 계속 실행된다. 시간은 `Graphics_Test.h`의 `GRAPHICS_TEST_CASE_MS`로 변경한다.

전체 목록과 각 기능의 제한은 `UI/TESTS.md`를 본다. `RENDERED`는 해당 화면을 EVE에 제출했다는 뜻이며 육안 합격이 아니다. `SKIPPED`는 미지원 경로를 실행하지 않았다는 뜻이다. 실제 차량 값이 없는 대시보드/ODO/시간/오디오 정보는 DEMO 데이터다.

프로젝트 루트에서:

```powershell
.\tools\build.ps1 -Configuration Debug
.\tools\build.ps1 -Configuration Release
.\tools\bringup.ps1 -Configuration Release -SkipBuild -GraphicsTest -GraphicsPinnedCase -FrequencyKhz 50 -Cycles 1
```

bringup은 기존 APP 범위/벡터/백업A·B/쓰기 후 readback/순정 하위64KiB 보존 절차를 유지한다. `probe_graphics.py --plan --manifest Debug/app.manifest.json`은 장치 접근 없이 관측 계획만 출력한다. 실제 SWD 관측은 `--execute --serial ... --output ...`가 필요하다. Debug/Release 설정은 Cube 재생성 후 build가 실행하는 sync/check로 복원한다. 실제 GUI 재생성 시험과 설정 손실 복원 fixture는 별개의 검증이다.

`-GraphicsPinnedCase`는 현재 호 화면 고정을 명시하며 같은 case에서 실제 frame 증가를 요구한다. 이것을 빼면 기존 자동 case 전환도 검사한다. 현재 CFW처럼 flash에 쓰지 않는 코드임이 확인된 경우에만 `-LiveFlashReads`로 백업/검증 flash 읽기 중 CPU halt를 생략할 수 있다. 실제 program/reset과 원자적인 RAM snapshot에는 정지가 남으므로 육안 확인 중임을 미리 알린다. 디버거 정지를 펌웨어 hang으로 보고하지 않는다.
