# 원형 호 시험의 ARM 검증

`run.py`는 production `Graphics_ArcSweep.c`와 `graphics_geometry.c`를 그대로
ARM Cortex-M4 ELF로 빌드하여 Unicorn에서 실행한다. 실제 `Graphics_Viewport.h`를
사용하고 LVGL 객체 생성·삭제·속성 기록과 성능 수치 입력만 대체한다.

검증 대상은 인자/기하 실패, 각 할당 단계의 정리, 부모 삭제 callback, 재초기화,
0·2·4·6·8초 easing 값, 5ms 서비스의 단조성, km/h와 독립적인 arc 값,
경과 시간 wrap, HUD 1초 주기 및 실제 입력 표시, 종료 이후 무변경이다.

`NOODOE_INTEGRATED=0` 그래픽 프로필과 `=1` 통합 프로필을 각각 Debug와 Release
최적화 수준인 `-O0`, `-Os`로 실행한다. 현재 각각 1,781개와 1,769개 assertion이
통과한다. 통합 프로필에서도 실제 ArcSweep label 생성/기하 검사를 실행하고,
외부 BringupHUD의 초기화/처리만 명시적 stub으로 둔다. label 높이는 실제
LVGL font C에서 읽은 line height 16/44를 fixture에 주입한다.

통합 제목의 과거 실패 조건 `y=-68%, width=140%`을 output 폴더의 복사본에만
되돌리는 mutation 시험을 수행한다. 현재 `width=120%`은 통과하고, 과거 값은
두 최적화 모두 production Init의 기하 오류 2로 거부되어야 한다. 따라서
이 회귀 검사가 실제 발생한 화면 초기화 실패를 잡는지도 확인한다.

결과와 실제 소스 SHA-256은 `output/results.json`에 저장된다. 현재 프로젝트 Python 환경에서
이 폴더의 `run.py`를 실행한다. 장치에 연결하거나 펌웨어를 쓰지 않는다.

이 시험은 LVGL 실제 layout/draw 엔진이나 EVE/SPI 전송을 실행하지 않는다.
따라서 외부 BringupHUD 행 배치·렌더링 성능·호 끝단의 시각 품질·물리 LCD
표시 확인을 대신하지 않는다.

후속480px 테두리 시험은 `GraphicsArcSweep_InitViewport`의480x480 위치/크기,
padding0, 안쪽18px stroke와270도 범위, label의 활성 원 포함, easing,
두 Init API 전환/부모 삭제/각 할당 단계 실패 정리를 추가 검증한다.
일반 `Init(...,240)`은 계속 거부한다. 전용 경로의 반 pixel 차이는 실제 EVE
최종 mask와 캡처에서 확인하며, 이 객체 대역 시험으로 가장자리 pixel을 추정하지 않는다.
