# LVGL 9.5 / EVE UI 시험 목록

이 모듈은 UI 범위와 EVE 제약을 순서대로 관찰하는 시험 프로그램이다. **렌더링 PASS 또는 모든 기능 지원을 선언하는 프로그램이 아니다.** LVGL v9.5.0 `85aa60d18b3d5e5588d7b247abf90198f07c8a63`를 대상으로 한다. 일반 위젯을 단순 스타일로 만든 30개 화면, 위험 경로를 실행하지 않는 11개 SKIPPED 화면, 총 41개다.

## 실행과 판정

- 기본 자동 전환은 `GRAPHICS_TEST_CASE_MS=8000`ms다. 전체 한 바퀴는 약328초다. 화면 번호는1부터, public API의 id는0부터다.
- `Graphics_Init` 성공 → `GraphicsTest_Init` → 초기 raw LOW mask를 `GraphicsTest_SetBootHeldMask`에 전달 → 포트 버튼 observer 등록 순서를 사용한다. 모든 UI API는 LVGL을 소유한 같은 task에서 호출한다.
- 부모 task가 `Graphics_Process` 후 실제 완료된 frame sequence를 `GraphicsTest_NotifyRendered`에 전달하고 `GraphicsTest_Process`를 반복 호출한다. UI 자체는 render 성공 통지를 만들어내지 않는다.
- 종료는 반드시 `GraphicsTest_Shutdown()` → `Graphics_Shutdown()` 순서다. UI root/animation/공통 style을 LVGL allocator가 살아 있을 때 먼저 반환한다. UI 종료는 반복 호출해도 안전하며, 종료 후 새 Graphics/UI Init 세션을 만들 수 있다. UI 종료 없이 먼저 `lv_deinit`한 뒤 남은 UI 포인터를 재사용하는 순서는 지원하지 않는다.
- 자동 모드에서는 LVGL encoder를 차단하되 버튼 observer는 받는다. UP/DOWN SHORT가 이전/다음 화면으로 이동한다. ENTER를2001ms 이상 누른 뒤 놓으면 수동 모드로 바뀐다. 수동에서는 포트 encoder의 UP/DOWN focus/edit와 짧은 ENTER click을 사용한다. ENTER를 다시 길게 눌렀다 놓으면 자동 순환을 재개한다. public Select/Next/Previous도 항상 사용할 수 있다.
- boot-held 입력은 처음 놓을 때 block만 푼다. 같은 누름의 SHORT는 버리며 새 PRESS부터 허용한다. 현재 고장 난 UP의 고정 LOW가 자동 화면을 계속 이전으로 돌리거나 모드를 바꾸지 않는다.
- RUNNING은 객체 생성 및 실행 시작, RENDERED는 포트의 frame 완료 통지, SKIPPED는 명시적으로 미실행, ERROR는 UI에서 관찰한 할당 실패다. RENDERED는 화면 모양·위젯 기능·물리 버튼의 합격 판정이 아니다. LVGL 내부 assertion/포트 GPU 오류는 별도 포트·fault 진단으로 확인한다.
- `g_graphics_test`는18개uint32 진단필드를 유지한다. 비동기 SWD로 읽으면 전체 필드가 원자적 스냅샷인 것은 아니다. CLI는 여러 표본의 transition/frame 증가로 판단한다. `GetCaseInfo`는 지원 예상과 capability, `GetResults`는 방문/프레임/이벤트/실패 누계를 제공한다.
- 위젯 이벤트 수는 프로그램이 발생시킨 VALUE_CHANGED/CLICKED/FOCUSED도 포함한다. 실제 물리 입력 횟수로 해석하지 않는다.

## 화면과 메모리 제한

활성 원은 `Graphics_Viewport.h`와 `Graphics/VIEWPORT.md`의 USB 사진 기준으로 고정한다. 일반 suite content/chrome은 모두 이 원에 포함된다. UI service는33ms, 실제 frame 제출은30Hz 누산기를 사용한다. 차량 UART, 실제 RTC/ODO/BT 값을 읽거나 수정하지 않는다.

첫 화면은480x480/padding0/바깥반경240px의270° 속도 호다. `GraphicsArcSweep_InitViewport`로 활성 원 테두리까지 사용하고 최종 원형 mask가 경계를 자른다. 정수 smoothstep으로4초에0→10000,4초에10000→0을 반복하며 숫자0..160과 독립적으로 호를 갱신한다. 기본 부팅은 이 case를 고정하여 연속 동작을 관찰한다. DOWN으로 고정을 풀고 기존 suite 순회를 재개한다. `GraphicsTest_SetCasePinned`는 `SetAutoAdvance(0)`과 달리 합성 동작을 계속 실행한다. 원본 dashboard audio/palette 코드는 GT_00_Dashboard.c의 legacy 분기에 보존한다.

상위 화면은 `Graphics_GetScreen()` 아래의 UI root만 소유한다. case 전환 시 객체별 animation을 취소하고 body의 자식 트리를 삭제한다. 위젯 destructor가 chart series/dropdown/내부 animation 같은 소유 메모리를 반환한다. `lv_anim_delete_all`은 사용하지 않는다. 부모 포트 화면을 지우지 않는다. case 간 계속 남는 메모리는 진단 배열·공통 스타일·고정 이미지·UI chrome뿐이다.

simple theme와 shared flat style을 사용하여 shadow, gradient, whole-object opacity, corner clipping, whole-object transform을 끈다. keyboard는6개 ASCII 키의 compact map, calendar는 month grid만 사용한다. EVE display list는8KiB/2048word이므로 페이지를 작게 유지한다. **작게 설계했다는 사실은 실제 모든 페이지의 DL 용량 검증을 대체하지 않는다.** 포트 `eve_cmd_dl`, 오류, heap 및 RAM_G 사용량을 순환 중 함께 확인해야 한다.

공통 색상은 flat style과 분리한다. MAIN/ITEMS는 `GT_PANEL`의 어두운 배경과 밝은 글자를 사용하고, keyboard CHECKED 및 dropdown/roller SELECTED는 어두운 강조색을 사용한다. 배경 불투명도는 강제하지 않아 label/arc의 투명성을 유지한다. INDICATOR/KNOB는 원래 기능 대비를 보존한다. 같은 selector에서 builder가 설정한 local 색이 shared palette보다 앞서므로 대시보드 배경·Flex/Grid 상자·LED 강조색은 유지된다. 버튼, text area/spinbox, table, keyboard/buttonmatrix, dropdown/roller, calendar, list, window/menu/msgbox 및 기본 container가 이 대비 수정의 대상이다. dropdown list는 screen 아래의 별도 객체라 생성 직후 따로 palette를 적용하여 수동으로 먼저 열어도 밝은 배경이 남지 않게 한다. 세 공통 style은 UI Shutdown 때 모두 reset한다.

이미지는 정적 주소의32×32 RGB565 두 장과 ARGB8888 한 장을 재사용한다. 입력 총8KiB, EVE 업로드 후RGB565/ARGB4 합계6KiB다. 이미지 source 주소를 재방문마다 바꾸지 않아 이미지 cache key가 무한히 늘지 않는다. 폰트14/20/28/40은4bpp이고 ASCII 시험만 한다. 다수 문자열의 고정 glyph cache 상한은 포트 캐시 진단으로 확인한다. 모든 label/image를 file decoder로 바꾸지 않는다.

## 순차 case

BASIC도 위젯 전체 API의 합격 의미가 아니다. LIMITED는 명시한 단순 렌더링 부분만 시험한다. 화면별 구현은 `src/GT_NN_*.c`, 공통 제어·도움 함수는 `src/Graphics_Test.c`, 정적 이미지는 `Graphics_Test_Assets.c`다.

|화면(id)|항목|시험 범위|분류|제약|
|---|---|---|---|---|
|1 (0)|Arc sweep|270° speed ring / FPS / CPU|BASIC|4초 채움·4초 비움, 합성 속도; 기본 고정, DOWN으로 순회 재개.|
|2 (1)|Controls|Button / checkbox / switch|BASIC|Auto toggles; manual focus/click when paused.|
|3 (2)|Value meters|Slider / bar|BASIC|0..160 sweep; manual slider editing.|
|4 (3)|Chart|16-point line series|LIMITED|Small line chart; no area fill or custom masks.|
|5 (4)|Table|3 rows / 2 columns|BASIC|Small cells and changing numeric value.|
|6 (5)|Scale|Linear ticks / labels|LIMITED|Linear scale only; no rotated text or round mask.|
|7 (6)|Spinbox|Digits / encoder editing|BASIC|Synthetic numbers; pause for encoder editing.|
|8 (7)|Dropdown|Selection / popup|LIMITED|Three fixed options; no symbol image or fade.|
|9 (8)|Roller|Finite options / selection|LIMITED|Finite list, flat background; no gradient fade.|
|10 (9)|Text input|Textarea / compact keyboard|LIMITED|Compact ASCII map; no IME/popovers.|
|11 (10)|Button matrix|Six keys / selection|BASIC|Small map keeps display list bounded.|
|12 (11)|Tab view|Two tabs / page switch|LIMITED|Two small pages; animated slide disabled.|
|13 (12)|Tile view|Two tiles / clipping|LIMITED|Rectangular clipping; no transformed layers.|
|14 (13)|Window|Header / content|BASIC|Embedded window; no whole-window opacity.|
|15 (14)|Menu|Page / load-page event|LIMITED|One subpage; no sidebar transitions.|
|16 (15)|Message box|Title / text / footer button|LIMITED|Embedded box; no modal overlay or symbol image.|
|17 (16)|Spinner|Animated arc|BASIC|Object-owned animation is deleted on case exit.|
|18 (17)|Rich text spans|Multiple text colors / fonts|LIMITED|ASCII styled text; no per-span transforms.|
|19 (18)|Images|RGB565 / alpha / transform|LIMITED|Static variable images only; nearest scaling.|
|20 (19)|Animated image|Two immutable RGB565 frames|LIMITED|Same two cache keys reused on every visit.|
|21 (20)|Image button|Released / pressed sources|LIMITED|Native image size; no tiled nine-slice.|
|22 (21)|Line and LED|Polyline / indicator brightness|LIMITED|LED shadow/gradient deliberately disabled.|
|23 (22)|Calendar|Month grid / highlight|LIMITED|Small month only; no dropdown/arrow images.|
|24 (23)|List|Three rows / focus|BASIC|ASCII text rows; image icons excluded.|
|25 (24)|Flex layout|Three cells / wrap|BASIC|Layout only; no layer transform.|
|26 (25)|Grid layout|2 by 2 cells|BASIC|Fractional layout; no clipped rounded corners.|
|27 (26)|Scrolling|Rectangular viewport|LIMITED|Rectangular scissor only; corner masking skipped.|
|28 (27)|Events and focus|Click / state / focus border|BASIC|Auto click events are synthetic, not physical proof.|
|29 (28)|Animation|Position / reverse / repeat|LIMITED|Position animation only; whole-widget opacity/zoom skipped.|
|30 (29)|Labels|Fonts / wrap / alignment|BASIC|14/20/28/40 4bpp fonts; ASCII only.|
|31 (30)|Canvas|Canvas / framebuffer|SKIPPED / CONFIG_DISABLED|Canvas needs a framebuffer/render path not enabled in this EVE-only build.|
|32 (31)|Layers|Offscreen / whole-widget zoom|SKIPPED / UNSUPPORTED|Offscreen layers and whole-widget transforms have no validated EVE path.|
|33 (32)|Shadows|Blur / box shadow|SKIPPED / UNSUPPORTED|EVE draw tasks do not implement LVGL blur shadows. All tested pages use zero shadow.|
|34 (33)|Gradients|Linear / radial fill|SKIPPED / UNSUPPORTED|Gradient styles are not implemented by this EVE backend. Solid colors only.|
|35 (34)|Masks|Rounded clipping / arbitrary masks|SKIPPED / UNSUPPORTED|Rectangle scissor is tested. Circular clipping and arbitrary alpha masks are not.|
|36 (35)|SVG and vectors|SVG / vector paths|SKIPPED / CONFIG_DISABLED|SVG/vector decoders and vector drawing backend are disabled.|
|37 (36)|Lottie|Vector animation|SKIPPED / CONFIG_DISABLED|Lottie is disabled; no software/vector fallback is invoked.|
|38 (37)|File images and GIF|File decoder / GIF / symbols|SKIPPED / UNSUPPORTED|EVE accepts variable image descriptors only. File/GIF decoders and image symbols are excluded.|
|39 (38)|Arc label|Curved / rotated glyphs|SKIPPED / CONFIG_DISABLED|Arc label is disabled. Per-glyph rotation is not validated by this EVE text renderer.|
|40 (39)|3D texture|3D rendering / texture target|SKIPPED / CONFIG_DISABLED|The 3D texture widget is disabled; no 3D backend exists in this build.|
|41 (40)|Pinyin IME|Chinese input / candidate text|SKIPPED / CONFIG_DISABLED|Pinyin IME and CJK font assets are not configured. ASCII keyboard is tested separately.|

각 실행 case는 관련 `LV_USE_*`가 꺼진 빌드에서 같은 번호의 CONFIG_DISABLED 화면으로 바뀐다. 따라서 사용하지 않는 widget 코드 참조를 몰래 남기지 않는다. UI shell은 label·공개된4개폰트·기본 객체가 켜져 있는 포트 설정을 전제로 한다.

## 공식 위젯 인벤토리 대응

[공식 LVGL9.5 위젯 목록](https://lvgl.io/docs/open/9.5/widgets/)의 독립 위젯을 기준으로 대응한다. 소스의 property/template 디렉터리는 별도 시각 위젯으로 세지 않는다.

|공식 위젯|화면 번호 / 처리|
|---|---|
|Base object|모든 화면;25 Flex,26 Grid,27 Scroll|
|3D texture|40 SKIPPED|
|Animated image|20|
|Arc|1;17 Spinner|
|Arc label|39 SKIPPED|
|Bar|3;6|
|Button|2;28|
|Button matrix|11;10 Keyboard;23 Calendar|
|Calendar|23|
|Canvas|31 SKIPPED|
|Chart|4|
|Checkbox|2|
|Dropdown|8|
|GIF|38 SKIPPED|
|Image|19|
|Image button|21|
|Pinyin IME|41 SKIPPED|
|Keyboard|10|
|Label|30;모든 chrome|
|LED|22, glow/shadow 제외|
|Line|22|
|List|24|
|Lottie|37 SKIPPED|
|Menu|15;기본 image symbol은 ASCII label로 교체|
|Message box|16;modal overlay 제외|
|Roller|9;normal finite mode|
|Scale|6;linear scale|
|Slider|3|
|Span|18|
|Spinbox|7|
|Spinner|17|
|Switch|2|
|Table|5|
|Tab view|12|
|Text area|10|
|Tile view|13|
|Window|14|

## UI 기능 범위와 미검증 부분

- Flex/grid, rectangular scroll clipping, focus border, clicked/value-changed/focus events, state 변경, 숫자 갱신, object-position animation/reverse/repeat를 실행한다.
- RGB565/ARGB8888 variable image, EVE의 ARGB4 변환, 이미지 자체 scale/rotation을 시험한다. arbitrary widget layer transform과 이미지 transform은 별도 경로다. nearest scaling의 화질은 육안 확인 대상이다.
- shadow blur, gradient, arbitrary mask/circular clip, offscreen layer, whole-page zoom/opacity, file/symbol image, SVG/vector, Lottie, 3D, GIF, CJK/IME는 시험했다고 표시하지 않는다.
- calendar의 연도/월 selector, 무한 roller, tab/tile slide animation, chart의 모든 plot 타입/area fill, full keyboard 배열/popover, modal dim layer 등 변형 전체는 미검증이다.
- case 전환 때 재사용 body의 style/layout을 제거하고 배경·불투명도·위치·여백을 기준값으로 되돌린다. 대시보드 palette가 다음 화면에 남지 않는다.
- 2026-09-12 호/고정마스크 추가 전30fps Release에서30개 case의 제출과11개 SKIPPED를 실행 중 live read로 모두 확인했다. 관측 GPU/SPI/할당 오류0, 구간 평균약29.98fps였다. 이는 이후 추가된 큰 호/최종 마스크의 실물 검증과 구분한다. 각 이미지 해시와 실물 기록은 `../../../../analysis/2026-09-12-lvgl/README.md`에 있다. 모든 화면의 육안 품질 합격을 의미하지 않는다.

## 근거와 라이선스

- LVGL v9.5.0 [공식 소스](https://github.com/lvgl/lvgl/tree/v9.5.0), MIT [LICENSE](https://github.com/lvgl/lvgl/blob/v9.5.0/LICENCE.txt). vendor의 원본 라이선스를 유지한다. 이 UI 시험 소스는 공식 public API를 이용해 새로 작성했고 외부 데모 전체를 복제하지 않았다.
- 로컬 `Middlewares/Third_Party/LVGL/src/draw/eve/lv_draw_eve.c`의 task 처리 범위와 `lv_draw_eve_image.c`의 `lv_draw_eve_image_src_check`/`lv_draw_eve_image_upload_image`를 기준으로 variable-only 및 L8/RGB565/RGB565A8/ARGB8888 제한을 확인했다. renderer의 조용한 skip을 PASS로 해석하지 않는다.
- `src/widgets/menu/lv_menu.c`는 back icon을 `lv_image_set_src(...,LV_SYMBOL_LEFT)`로 만든다. EVE variable-image 제한과 맞지 않아 시험에서는 해당 아이콘 자식만 ASCII label로 교체하며 button/page event는 유지한다.
- simple theme 소스에는 shadow/gradient/layer/transform 설정이 없다. dropdown은 symbol=NULL, keyboard는 compact ASCII map, calendar는 header 미생성, window/msgbox는 icon 버튼 미생성으로 기본 symbol-image 경로를 차단했다. roller 소스의 `layer->_clip_area` 변경은 기존 draw layer에 대한 직사각 clip이며 별도 offscreen layer 생성과 구분한다.
- imagebutton의 middle source는 내부적으로 draw descriptor의 `tile=1`을 사용한다. 시험은 source/object 모두32×32여서 추가 반복 타일이 필요 없는 한 장 범위만 사용한다. renderer의 일반 tiled image나 확대 nine-slice 지원을 증명하지 않는다.
