#ifndef BSP_DISPLAY_H
#define BSP_DISPLAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 좌표 원점은 왼쪽 위다. x/y/w/h/thickness 단위는 pixel, 색은 0x00RRGGBB다. */
#define BSP_DISPLAY_WIDTH          480U
#define BSP_DISPLAY_HEIGHT         480U
#define BSP_DISPLAY_DL_CAPACITY    256U

/* HAL/EVE의 내부 enum을 상위 코드에 노출하지 않는다. 반드시 0만 성공으로 본다. */
typedef enum {
    BSP_DISPLAY_OK = 0,
    BSP_DISPLAY_ERROR_ARGUMENT = 1,
    BSP_DISPLAY_ERROR_STATE = 2,
    BSP_DISPLAY_ERROR_BUFFER_FULL = 3,
    BSP_DISPLAY_ERROR_CONTEXT = 4,
    BSP_DISPLAY_ERROR_BACKLIGHT = 5,
    BSP_DISPLAY_ERROR_PANEL = 6,
    BSP_DISPLAY_ERROR_EVE = 7
} BSP_Display_Status;

/*
 * 17개의 32bit word, 68byte다. last_result/backend_result는 마지막 API 결과이며
 * backend는 해당 HAL/EVE 원래 반환값이다. frame_error는 현재 프레임의 최초 오류다.
 * frames_presented는 완료한 제출 수, last_frames는 EVE scanout 조회값으로 서로 다르다.
 */
typedef struct {
    uint32_t magic, version, stage, last_result, backend_result;
    uint32_t initialized, width, height, frame_open, frame_error;
    uint32_t word_count, frames_presented, requested_brightness, actual_brightness;
    uint32_t last_frames, failures, failed_stage;
} BSP_Display_Diagnostics;

extern volatile BSP_Display_Diagnostics g_bsp_display;

/*
 * HAL tick/RTOS가 동작하는 단일 Thread/task가 전체 API를 직렬 사용한다.
 * Init은 기존 순정 호환 BSP의 백라이트0 -> 패널hold -> EVE -> 패널 init을 실행한다.
 * 동적 할당/외부 framebuffer/플래시 쓰기는 없다. 성공해도 밝기는 0%다.
 * 통신/장치 실패는 reset/off로 차단하며 Init을 다시 성공해야 사용을 재개할 수 있다.
 */
BSP_Display_Status BSP_Display_Init(void);

/*
 * 새 프레임을 MCU의 고정 DL buffer에 시작한다. 열린 이전 프레임은 버린다.
 * 여기서 실제 화면을 지우지 않는다. Present가 성공할 때 frame boundary에 바뀐다.
 * 잘못된 도형/용량 초과 이후에는 Present를 거부하고 다음 BeginFrame으로 회복한다.
 */
BSP_Display_Status BSP_Display_BeginFrame(uint32_t clear_rgb);

/*
 * [x,x+w) × [y,y+h)의 정수 pixel을 채운다. 40×40은 정확히 1600 pixel 영역이다.
 * w/h는 1 이상이고 사각형 전체가 화면 안에 있어야 한다. 자동 clipping은 하지 않는다.
 * GPU RECTS의 둥근 모서리/선 반경 대신 SCISSOR와 color CLEAR를 사용한다.
 */
BSP_Display_Status BSP_Display_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                      uint32_t rgb);

/*
 * 주어진 사각형 안쪽에 thickness pixel 테두리를 그린다. thickness>=1이어야 한다.
 * 양쪽 테두리가 만나거나 겹치면 전체 사각형을 채운다. 바깥 크기는 정확히 w×h다.
 */
BSP_Display_Status BSP_Display_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                      uint16_t thickness, uint32_t rgb);

/*
 * 두 끝 pixel의 중심을 연결하는 둥근 끝 선이다. width는 전체 선 두께 1..480 pixel이다.
 * 두 끝 좌표는 0..479이어야 한다. 선 두께가 화면 밖으로 나간 부분은 화면에서 clip된다.
 * EVE LINE_WIDTH가 반경 단위이므로 내부적으로 width*8(1/16 pixel 반경)을 사용한다.
 */
BSP_Display_Status BSP_Display_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                                      uint16_t width, uint32_t rgb);

/*
 * DISPLAY 종단을 붙여 EVE에 동기 제출하고 swap 완료를 확인한다. 실패 프레임은 제출하지
 * 않는다. 성공/실패 후 frame은 닫힌다. 소프트 오류는 이전 화면/밝기를 유지한다.
 */
BSP_Display_Status BSP_Display_Present(void);

/* 배경/창/제목띠/교차선을 위 API만으로 구성하는 선택적 기본 화면이다. 밝기는 바꾸지 않는다. */
BSP_Display_Status BSP_Display_DrawTestWindow(void);

/* 0..100%, 순정 제한으로 100은 실제 99%. 25와 0을 호출하면 기존 25%/off blink가 된다. */
BSP_Display_Status BSP_Display_SetBrightnessPercent(uint32_t percent);

/* EVE frame counter를 읽는다. NULL/실패 시 *frames를 바꾸지 않는다. */
BSP_Display_Status BSP_Display_ReadFrames(uint32_t *frames);

/* 대기 없이 패널/광원을 차단하고 열린 frame을 버린다. Init 전에도 사용할 수 있다. */
void BSP_Display_Shutdown(void);
/* Graphics-owner polling API:0 complete,1 pending,>=2 hardware failure.
 * Retains EVE RAM and UI objects. Wake settles oscillator30ms/panel120ms
 * through subsequent calls; never waits those delays inside the API. */
uint32_t BSP_Display_SetSleeping(uint32_t sleeping);
/* True while asleep OR unavailable during sleep/wake settling. Successful
 * SetSleeping(1)==0 is additionally required to acknowledge deep-sleep idle. */
uint32_t BSP_Display_IsSleeping(void);

/* 내부 장치에 접근하지 않는 진단 포인터다. 여러 필드의 동시 snapshot은 보장하지 않는다. */
const volatile BSP_Display_Diagnostics *BSP_Display_GetDiagnostics(void);

/* Real EVE raster capture, not a software reconstruction of the UI. The upper
 * RAM_G reservation holds24rows; a fixed SDRAM copy retains the480x480 result.
 * Host/task publishes request_seq last, and firmware publishes response_seq
 * last after payload+CRC. Exactly one requester is allowed at a time. */
#define BSP_DISPLAY_CAPTURE_RAM_G 0x000FE000U
#define BSP_DISPLAY_CAPTURE_STRIPE_BYTES (480U*BSP_DISPLAY_CAPTURE_ROWS*2U)
#define BSP_DISPLAY_CAPTURE_ROWS 8U
#define BSP_DISPLAY_CAPTURE_BYTES (480U*480U*2U)
#define BSP_DISPLAY_CAPTURE_CHUNK 4096U
typedef struct {
    uint32_t magic,version,request_seq,command,offset,length,expected_generation;
    uint32_t response_seq,status,generation,width,height,format,total_bytes;
    uint32_t payload_length,payload_crc32,duration_ms,eve_frames,requests,failures;
    uint8_t data[BSP_DISPLAY_CAPTURE_CHUNK];
} BSP_Display_CaptureMailbox;
extern volatile BSP_Display_CaptureMailbox g_bsp_capture;
/* No SPI here: any Thread caller queues a bounded request. New sequence must
 * be nonzero and differ from the prior response. Busy requests are rejected.
 * kind1=capture,2=copy chunk,3=release; a chunk requires matching generation.
 * kind4=capture plus the first4096bytes of the same RAM_DL in the response
 * payload, with CRC. Host must read that payload before requesting pixels.
 * All captures run only at the graphics owner's idle boundary. */
BSP_Display_Status BSP_Display_CaptureRequest(uint32_t sequence,uint32_t kind,
    uint32_t generation,uint32_t offset,uint32_t length);
/* Only graphics owner may call at a safe point after its command burst ended.
 * Idle cost is a few RAM reads. Twenty 24-row snapshots freeze one display
 * list for at most2s. Later chunk reads export the SDRAM copy while UI runs. */
void BSP_Display_CaptureProcess(void);
/* Graphics lifecycle only: invalidate a frozen image before EVE reset/init. */
void BSP_Display_CaptureInvalidate(void);
uint32_t BSP_Display_CaptureBusy(void);

#ifdef __cplusplus
}
#endif

#endif
