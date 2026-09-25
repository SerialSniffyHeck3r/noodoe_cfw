# 비동기 토스트

```c
#include "Popup_Notifications.h"   /* ProductUI.h에서도 포함 */

if (!ShowToastMessages("Messages", 3)) {
    /* UI 초기화 전, 잘못된 입력 또는 아직 소비되지 않은 요청이 있음. */
}
/* 필요할 때 */
HideToastMessages();
```

- 요청은 복사 후 즉시 반환한다. task 문맥 전용이며 ISR에서는 호출하지 않는다.
- 현재 폰트의 printable ASCII 1..63자, 표시 시간1..60초를 받는다. 빈 문자열/지원하지 않는 문자/범위 초과는0을 반환한다. 한 요청이 소비되기 전에는 다음 요청을0으로 거부한다. 임의 개수 큐/heap 할당은 없다.
- UI가 요청을 소비한 시점부터 시간을 잰다. 새 메시지는 기존 것을 교체하고 시간을 다시 시작한다. 호출자 버퍼는 반환 후 바꿔도 된다. 유효한 요청을 접수했다는1과 이미 화면에 그려졌다는 뜻을 혼동하지 않는다.
- 총 표시 시간 안에 양끝240ms Slow–Fast–Slow fade가 포함된다. 타이머는 실제 MCU 단조 시간이고 페이지 전환 프리뷰의 시간 고정에 영향받지 않는다. tick wrap을 처리한다.
- 패널은 X104/Y330/272×48, 한 줄 Lato24와1px 테두리다. 긴 메시지는 실제 폰트 폭으로 `...` 처리한다. LVGL static text에는 LONG_DOT이 지원되지 않으므로 자체 고정 버퍼를 쓰며 원문은 앱 모델에 남긴다. 기존 페이지 위에 잠깐 겹쳐지고 ring/clock/footer를 침범하지 않는다.
- RUNNING 상태의 일반 페이지에서 표시한다. warning/modal/menu가 열려 있으면 뒤의 토스트를 숨기며 별도 알림 큐나 일시정지 기능은 없다.

일반 페이지→팝업→일반 페이지의 원본 PNG/RGB565 픽셀을 대조했다. 초기 작업 중 미리보기에서 중앙 누락으로 판단했던 것은 원본 파일에서는 재현되지 않았다. 실제 파일의 픽셀과 단순 미리보기 인상을 구분한다. 테두리를 제거하는 우회는 최종 코드에 남기지 않는다.

`Popup_Notifications.c`는 요청/시간/우선순위와 진입 힌트를 소유한다. `product_ui.c`는 task 안전 공개 facade와 실제 페이지 문맥을 연결한다. `product_toast_view.c`는 좌표/폰트/primitive alpha만 소유한다. 향후 다른 팝업 정책은 같은 앱 기능에 추가하고 BSP로 옮기지 않는다.

TRIP A/B의 수동 초기화가 가능한 문맥에 들어오면 `Hold O to reset`을3초 표시한다. A↔B 변경/매 프레임마다 반복하지 않는다. 나가면 해당 힌트만 취소하고 호출자가 넣은 다른 메시지는 보존한다. TODAY/주유 후 기록에는 이 힌트를 표시하지 않는다. 가운데 ENTER 길게의 기존 리셋 확인창과 Cancel 기본값은 그대로다.

`g_popup_notifications`는184바이트 RAM 진단/mailbox다. version1, command1=show/2=hide. command/seconds/request_text 먼저, request_id 마지막으로 게시하고 ack_id를 기다린다. 텍스트 및 시간은 소비 측에서도 검증한다. source1=자동 TRIP 힌트,2=호출자. seq는 활성 표시 상태를 보호한다. 정적 검사는 tools/tests/popup_notifications, 실제 기기 검증은 Reversing/analysis/2026-09-14-carousel-toast에 보관한다.
