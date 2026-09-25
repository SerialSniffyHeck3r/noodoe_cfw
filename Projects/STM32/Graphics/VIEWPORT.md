# 고정 원형 활성 영역

현재 활성 원은 **중심(239.5,239.5), 반경240px, 지름480px**다. 사용자의 후속 지시인 중앙 가로·세로480픽셀 전체 사용을 따른다. raster 경계는 그대로0..479이며 사각형 모서리는 활성 영역에 포함되지 않는다.

최초 기준은 사용자가 지정한 USB 원본 `album/1/8f3c6ec2bb08c71586b00c691a8192a7.jpg`였다. 원본은480×480이며 SHA256은 `ccc245855ac9df2532b168bbbc0fca5c93b9e4cc8bf7e32bb56c73da0225503b`다. [재현 가능한 분석](../../../analysis/2026-09-12-lvgl/viewport/VIEWPORT.md)의 fit은 중심(239.4974,239.4943), 반경239.5149px였고, 이전 구현은 반경239.5px를 사용했다. 이전 분석 파일은 이력을 보존한다. 현재240px는 사용자 지정 좌표 계약이며 JPEG나 실제 베젤에서 새로 측정한 값이 아니다.

`Graphics/Port/inc/Graphics_Viewport.h`가 유일한 좌표 기준이다. pixel 좌표는0..479이고,2배 정수 좌표에서 `(2*x-479)^2+(2*y-479)^2 <= 480^2`를 만족하는 부분만 활성이다. 중앙 두 행 `y=239,240`과 중앙 두 열 `x=239,240`은 모두0..479 전체를 포함한다. 원의 연속 좌표 범위는-0.5..479.5지만 raster 밖 좌표는 항상 거부한다. 위젯 여백은 원 자체를 축소하지 않고 도형별로 정한다.

## 상위 코드 계약

- 사각형 전체: `Graphics_IsAreaVisible`. 모든 label/content 배치에서 사용한다.
- 원형 도형 전체: `Graphics_IsCircleVisible`. 반경에는 stroke/안티앨리어싱 여유를 포함한다.
- 점/향후 좌표 입력: `Graphics_IsPointVisible`.
- 세로 구간의 최대 안전 사각형: `Graphics_GetSafeArea`.
- 내부 draw 제외: `Graphics_AreaIntersectsVisible`.

API signature와 정수 pixel 단위는 바뀌지 않는다. `Graphics_GetSafeArea(239,240)`은 `[0,239,479,240]`, 최상단·최하단 행은 x225..254를 반환한다. `(70,70)`은 안쪽, `(69,70)`은 바깥이고 네 사분면에 대칭이다. 원 전체가 담긴480×480 사각형은 `Graphics_IsAreaVisible`에서 여전히 거부한다.

`Graphics_IsCircleVisible`은 **원 전체 포함** 검사다. 정수 중심(239,239) 또는(240,240)의 반경239는 허용하지만 반경240는 중심의 half-pixel 차이 때문에 거부한다. LVGL 정수 중심(240,240)·반경240로 전체 viewport를 채우고 마지막 mask로 자르는 전용 도형은 이 함수의 완전포함 보장을 주장하면 안 된다. 일반 도형 API의 검사를 느슨하게 바꾸거나 조용히 반경을 줄이는 방식으로 해결하지 않는다.

원 바깥에 UI 객체/클릭 대상/독립적인 갱신 로직을 배치하지 않는다. 원형 canvas를 담는 투명480×480 부모는 좌표계일 뿐 그림 영역이 아니다. 일반 LVGL API로 만든 객체의 할당 자체를 가로채는 기능은 없으므로 상위도 이 배치 계약을 지켜야 한다.

## EVE 출력 경계

`EveDispatch`는 실제 렌더 경계(`_real_area`)와 clip의 교집합이 활성 원 바깥이면 task를 전송하지 않는다. 그 task의 glyph/image 업로드도 생략한다. 교차하는 task는 최종 출력에서 고정 원으로 자른다.

`graphics_eve_viewport.c`는 모든 LVGL draw 뒤 DISPLAY 앞에서 EVE native stencil을 초기화하고, 원 바깥만 고정 검정으로 제외한다. 기존 arc/triangle가 stencil을 자체 사용하기 때문에 프레임 시작에 마스크를 넣는 것만으로는 이 경계를 강제할 수 없다. 최종 제외는30word(120byte)의 고정 명령이며 mask 이미지, 전체 framebuffer, 행별 재렌더링을 추가하지 않는다. SAVE/RESTORE는 graphics 속성을 복원한다. BEGIN/END primitive는 context에 저장되지 않으므로 다음 draw의 primitive 일치는 기존 `EveDispatch` 동기화가 보장한다.

EVE `POINT_SIZE`는 지름이 아닌 반경이며 단위는1/16pixel이다(고정 vendor `EVE.h` 주석/명령 정의). `VERTEX_FORMAT(4)`에서 중심은 `VERTEX2F(3832,3832)`, 반경은 `POINT_SIZE(3840)`이다. scissor는 `(0,0,480,480)`으로 유지하고 stencil 원 밖만 검정 rectangle로 덮는다. 기존3832 반경을3840으로 바꾸는 것 외에 마스크 명령 수나 프레임 비용은 늘리지 않는다.

**바깥에 UI 내용이나 별도 갱신 작업을 두지 않는 것과 LCD 주사 자체를 멈추는 것은 다르다.** 이 EVE 포트는 매 프레임 display list를 만들고 하드웨어는480×480을 계속 주사한다. 프레임 clear와 고정 검정 제외 명령은 남는다. 부분 framebuffer 방식처럼 외곽 pixel을 한 번만 쓰고 이후에는 전혀 재생성하지 않는 구조라고 주장하지 않는다. 현재 구현은 외곽 UI 자원·draw 전송을 제한하고 화면 누출을 막는 계약이다.

JPEG의 모든 경계 밝기나 물리 베젤의0.5pixel 측정을 재현한다는 의미도 아니다. 최종 경계의 pixel 커버리지는 EVE primitive 규칙을 따른다.

`tools/tests/graphics_performance_host`는 production C의 모든480행 최대 폭·좌우/상하/전치 대칭, 중앙 전체 축, 모서리 제외, 원 전체 포함 검사와 기존 pacing/CPU 시험을 실행한다. `tools/tests/eve_state_host`는 실제 viewport C 명령 스트림의 중심·반경·scissor·stencil·상태 복원을 검사한다. 이 host 시험은 실제 EVE 안티앨리어싱 raster나 화면 품질을 시뮬레이션하지 않으며 실물 캡처 검증과 구분한다.

2026-09-12 [전체480 실기 캡처](../../../analysis/2026-09-12-display-full480/README.md)는 중앙 두 행의 좌우 끝 x0/479와 상단 중앙의 실제 호 표시를 확인했다. 하단 중앙은270도 호의 개방부여서 검정이다. 수학적 원 밖 pixel center647개에 얇은 AA 경계가 있으며 최대 반경240.9285, 반경241 밖 nonblack pixel은0이었다. 따라서 실제 raster의 경계 AA까지 pixel-center 식과 완전히 같은 hard cutoff라고 주장하지 않는다. 상위 배치/입력 계약은 계속 위 식을 따른다.


## 2026-09-13 집 개발용 바깥 색

사용자 후속 지시에 따라 `Graphics_Viewport.h`의 `GRAPHICS_DEV_VIEWPORT`를
기본1로 둔다. 이때 최종 바깥 제외 rectangle 색은 녹회색 **0x4A5952**다.
제품용 검정은 `GRAPHICS_DEV_VIEWPORT=0` 빌드로 복원한다. 앞의 검정 raster
기록은 당시 이미지의 역사적 측정이다. 현재 옵션은 COLOR_RGB 한 명령만
바꾸며 30word 비용·중심·반경·입력 판정·draw 제외 계약을 바꾸지 않는다.
바깥 영역에 widget/텍스트를 추가하거나 그 영역의 입력을 허용하지 않는다.

## 2026-09-13 최종 중앙 가변 영역

전체 원 좌표는 유지하고 중앙은 x72..407/y105..384(336×280)로 확정한다.
SpeedHome_GetContentRoot의 자식만 이 영역을 사용한다. 로컬0..335/0..279이며
overflow는 꺼져 있고 LVGL/EVE scissor가 경계를 제한한다. 시계와 위 사다리꼴을
현재 시계 baseline71/구분선 가로Y87이다. 시계 구분선은 기존 사선을 위로 연장해 속도 링의 안쪽 경계에서 끝낸다. 거리 위 구분선은 사선을 아래로 연장해 호 개방부의 원 경계에서 끝내고, 수평 덧붙임은 하지 않는다. 원 밖 부분은 기존 stencil로 제외하고 중앙 자식/일반 도형의 포함 검사는 그대로다. 거리 아래 직선과 숫자 배치는 유지한다.
현재 좌표·폰트·하단 바 계약은 [상태 머신 계약](../App_Logic/STATE_MACHINE.md)를 따른다.
개발 HUD는 기존 위치의 shell overlay이고 중앙 컨테이너를 확대하지 않는다.
