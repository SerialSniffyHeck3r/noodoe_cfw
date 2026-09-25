# 30 FPS 및 원형 화면 계약

기준 해상도는 순정 V5.16의 HSIZE/VSIZE=480/480이다. `analysis/2026-09-12-lcd-bringup/eve-reference.md`에 원본 FLASH 주소와 값이 있다. SPI1은 초기10.5MHz에서 운용21MHz로 전환한다. 활성 영역은 사용자 지정 USB JPEG의 원형 형태를 유지하되, 후속 지시에 따라 중앙480px 전체를 쓰는 중심(239.5,239.5), 반경240px 원이다. 상수와 출력 제한은 [VIEWPORT.md](VIEWPORT.md)를 따른다.

## 프레임 주기

`Graphics_SetTargetFPS(30)`은 ms 누산기로1초에30개 frame 슬롯을 만든다. `33ms + 렌더 시간`을 매번 더하지 않아 처리 시간이 주기 오차로 누적되지 않는다. 서비스가 늦으면 지난 슬롯을 한꺼번에 그리지 않고 `frame_slots_missed`에 남긴다. LVGL refresh timer의 callback은 deferred 처리하고, 같은 소유 task에서 실제 마감 시각에 `lv_refr_now`를 호출한다. LVGL 원본 파일은 수정하지 않는다.

`Graphics_SetContinuousRendering(1)`은 정적 화면도 다시 그리는 시험 부하다. LCDTest는 이 모드로41개 화면을 순회한다. 제품 UI에서는0으로 꺼서 변경된 화면만 제출할 수 있다. 이때 낮은 제출 FPS는 유휴 화면의 정상 동작일 수 있다. EVE의 LCD scanout 빈도는 별개다.

합성 값은 `GRAPHICS_TEST_UPDATE_MS=33`에 맞춰 갱신하지만 속도/rotation/scroll 위치는 경과 시간을 사용한다. 기존8초 화면 전환,6.4초 속도 왕복,버튼 입력 시간과 애니메이션 기간은 유지한다. 제목/도움말은 전환 때, 시간은 초 변경 때, 진단 문자열은 필요한 주기에만 다시 설정한다.

## 화면의 실측값

하단 `29.9 FPS CPU 42.1%` 같은 표시는 예시이며 고정 문자열이 아니다. `Graphics_GetPerformance()` / `g_graphics_performance`의1초 관측값을 출력한다.

- `fps_tenths`: 완료된 LVGL→EVE 제출 frame 수를 실제 DWT cycle 창으로 나눈0.1fps 단위 값. 1초 창의 양 끝과 서비스 지연에 따른 변동이 있으므로 단일 숫자만으로 최악 성능을 보장하지 않는다.
- `cpu_tenths`: FreeRTOS idle task가 차지하지 않은 cycle 비율,0..1000=0.0..100.0%. 그래픽 task만의 사용률이 아니다.
- DWT counter는 reset하지 않고 enable하며, scheduler의 표준 `traceTASK_SWITCHED_IN` hook으로 idle cycle을 누적한다. config와 include는 `FreeRTOSConfig.h`의 USER CODE에만 둔다. vendor kernel과 generated 영역 바깥 코드는 수정하지 않는다.
- ISR 시간은 당시 실행 중이던 task에 귀속된다. 따라서 idle 중 ISR 부하는 idle 시간에 포함될 수 있는 **RTOS 비유휴율 추정치**이며, 모든 예외 시간을 분리한 cycle profiler는 아니다. FPU/IRQ/클록 설정을 계측 때문에 변경하지 않는다.
- DWT가 debugger halt 동안 정지하므로 장치를 멈춘 시간을0% CPU로 합산하지 않는다. 그래픽 service가25초 이상 호출되지 않거나 외부 debugger가 DWT를 reset하는 사용례는 이1초 주기 계측 계약 밖이다.

## 원형 영역

`Graphics_IsAreaVisible`은 네 꼭짓점으로 사각형 전체가 안전 원 안에 드는지 검사한다. `Graphics_GetSafeArea(top,bottom)`는 지정한 세로 band의 최대 안전 폭을 반환한다. 상위는 BSP나 SPI를 알 필요 없이 이 API로 배치할 수 있다.

일반 시험 content는(66,112),348×225이며 기존464×300 논리 좌표를3/4 크기로 배치한다. framebuffer/whole-widget 변환은 없고 글꼴·bitmap pixel은 유지한다. 큰 호 시험은 별도 투명 부모 아래480x480/padding0/바깥반경240px로 그린다. LVGL 정수 중심(240,240)과 활성 원 중심의 반 pixel 차이는 최종 mask로 제한하고 모든 label은 활성 원 안에서 검사한다. EVE 마지막 단계에서120byte의 고정 stencil 명령이 바깥을 검정으로 제외한다. 이는 LVGL의 임의 circular mask/layer 지원과 별개다.

## 검증

`tools/tests/graphics_performance_host/run.py`는 실제 CPU 계측·pacer·geometry C와 실제 viewport 상수를 ARM/Unicorn으로 실행한다. 전체480행/중앙축 검사를 포함하여 O0/Os 각각4033검사를 통과했다. 25%/0%/100% 스케줄,32bit cycle wrap,미측정 상태,5ms 서비스에서30frame,지연 시 burst 금지,반 pixel 원 경계와 safe band를 확인한다. EVE 상태/고정 제외 명령은 O0/Os 각각14사례223검사로 확인했다. 이것은 실물 부하·화질 측정을 대신하지 않는다.

최초 실물 결과는 `../../../analysis/2026-09-12-lvgl/README.md`에 보존한다. 전체480 호의 후속 검증은 [실기 기록](../../../analysis/2026-09-12-display-full480/README.md)에 있다. Release APP `67e08698...`에서9435ms/283frame=29.9947FPS, SPI/그래픽 오류0이었다. 실제 EVE 캡처 후에도30frame/1002ms와 CPU60.6%를 확인했다. 이 구간 결과는 모든 화면·입력·통신 부하의 최악 성능 보장이 아니다.
