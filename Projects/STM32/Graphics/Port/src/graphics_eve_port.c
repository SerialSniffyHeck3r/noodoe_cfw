/* LVGL 9.5.0 EVE renderer용 Noodoe display adapter.
 * API 연결 순서는 upstream src/drivers/draw/eve/lv_draw_eve_display.c를 참고했다.
 * LVGL은 MIT, upstream 저작권/라이선스는 Middlewares/Third_Party/LVGL/LICENSE.txt.
 * 별도 adapter를 둔 이유: Noodoe BSP가 이미 수행한 panel/EVE 초기화를 인계하고,
 * upstream EVE_init의 PDN/GPIO/PWM 변경 및 무한 FIFO 대기를 다시 실행하지 않는다.
 * renderer/폰트/도형 구현은 그대로 upstream 것을 사용한다. */
#include "graphics_internal.h"
#include "Graphics_SubpixelArc.h"
#include "Graphics_BackgroundDraw.h"
#include "bsp_eve_bus.h"
#include "Graphics_EveTransport.h"
#include "BSP_Display.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include "src/draw/eve/lv_draw_eve.h"
#include "src/draw/eve/lv_draw_eve_private.h"
#include "src/draw/eve/lv_eve.h"
#include "src/draw/lv_draw_private.h"
#include "src/display/lv_display_private.h"
#include "src/misc/lv_text_private.h"
#include "src/misc/lv_area_private.h"
#include "src/libs/FT800-FT813/EVE_commands.h"
#include <string.h>
#if defined(NOODOE_PRODUCT) && NOODOE_PRODUCT
#include "Product_Theme.h"
#include "Graphics_LargeNumber.h"
#include "Gps_LineBatch.h"
#endif

#define EVE_FIFO_TIMEOUT_MS 250U
#define EVE_RENDER_TIMEOUT_MS 2500U
static uint32_t dummy_draw_buffer;
static uint32_t bitmap_baseline_pending;
static int32_t (*upstream_dispatch)(lv_draw_unit_t *, lv_layer_t *);
#if defined(NOODOE_PRODUCT) && NOODOE_PRODUCT
static int32_t (*upstream_evaluate)(lv_draw_unit_t *,lv_draw_task_t *);
/* Claim only our explicit subpixel arc tag; all normal tasks retain upstream
 * scoring. Evaluation must happen before LVGL can discard an unclaimed task. */
static int32_t EveEvaluate(lv_draw_unit_t *unit,lv_draw_task_t *task)
{
    if(Graphics_EvaluateSubpixelArc(task)||GraphicsBackground_Evaluate(task)||Graphics_LargeNumberEvaluate(task)||GpsLineBatch_Evaluate(task))return 0;
    return upstream_evaluate(unit,task);
}
#endif
void Graphics_EveApplyViewport(void);

/* Clip correctness is owned by graphics_eve_clip, including calls inside a
 * task. BEGIN is an action, not a saved context register: synchronize vendor
 * cache and actual primitive with one explicit baseline, without drawing any
 * vertex or manufacturing a second clip/primitive just to disturb the cache. */
static int32_t EveDispatch(lv_draw_unit_t *unit, lv_layer_t *layer)
{
    lv_draw_task_t *task=lv_draw_get_next_available_task(layer,NULL,9);
    if(task) {
        lv_area_t visible;
        if(!lv_area_intersect(&visible,&task->_real_area,&task->clip_area) ||
           !Graphics_AreaIntersectsVisible(&visible)) {
            /* 원 바깥task는glyph/image업로드도생략한다. 완료 상태로 바꿔
             * 후속LVGL task의의존관계가풀리도록dispatch를요청한다. */
            task->state=LV_DRAW_TASK_STATE_FINISHED;
            lv_draw_dispatch_request();
            return 1;
        }
        lv_eve_primitive(EVE_RECTS);
        EVE_cmd_dl_burst(DL_BEGIN|EVE_RECTS);
#if defined(NOODOE_PRODUCT) && NOODOE_PRODUCT
        if(Graphics_DispatchSubpixelArc(task)||GraphicsBackground_Dispatch(task)||GpsLineBatch_Dispatch(task)){
            task->state=LV_DRAW_TASK_STATE_FINISHED;
            lv_draw_dispatch_request();return 1;
        }
#endif
    }
    return upstream_dispatch(unit, layer);
}

/* HAL timeout 또는 잘못된 bus ownership을 숨기지 않는다. EVE upstream 콜백은
 * void라 오류를 반환할 통로가 없으므로 최초 오류 후 진단 가능한 task 정지로
 * 전환한다. EVE_busy에 가짜 성공 값을 반환하여 깨진 프레임을 통과시키지 않는다. */
static void EveIo(lv_display_t *display, lv_draw_eve_operation_t operation,
                  void *data, uint32_t length)
{
    (void)display;
    BSP_EVE_Status status = BSP_EVE_OK;
    if (g_graphics.render_busy && HAL_GetTick() - g_graphics.render_started_ms > EVE_RENDER_TIMEOUT_MS) {
        Graphics_RecordError(GRAPHICS_ERROR_GPU);
        Graphics_AssertFail(__FILE_NAME__, __LINE__);
    }
    switch (operation) {
        case LV_DRAW_EVE_OPERATION_CS_ASSERT: status = GraphicsEveTransport_Select(); break;
        case LV_DRAW_EVE_OPERATION_CS_DEASSERT: status = GraphicsEveTransport_Deselect(); break;
        case LV_DRAW_EVE_OPERATION_SPI_SEND: status = GraphicsEveTransport_Send(data, length); break;
        case LV_DRAW_EVE_OPERATION_SPI_RECEIVE: status = GraphicsEveTransport_Receive(data, length); break;
        default:
            /* Hardware reset은 BSP lifecycle의 책임이다. init 없는 인계 경로에서
             * renderer가 PDN 조작을 요구하면 잘못된 호출로 명시적으로 멈춘다. */
            Graphics_RecordError(GRAPHICS_ERROR_UNSUPPORTED);
            Graphics_AssertFail(__FILE_NAME__, __LINE__);
    }
    if (status != BSP_EVE_OK) {
        (void)GraphicsEveTransport_Deselect();
        Graphics_RecordError(GRAPHICS_ERROR_IO);
        Graphics_AssertFail(__FILE_NAME__, __LINE__);
    }
}

/* upstream EVE_execute_cmd()의 무한 polling 대신 제한된 대기를 사용한다.
 * coprocessor fault 시 자동 reset으로 실패를 감추지 않고 그대로 기록한다.
 * 이미 닫힌 command burst에서만 호출해야 REG_CMDB_SPACE 읽기가 완결된다. */
static Graphics_Status EveWaitIdle(void)
{
    const uint32_t started = HAL_GetTick();
    for (;;) {
        const uint32_t space = EVE_memRead16(REG_CMDB_SPACE);
        if ((space & 3U) != 0U) { return Graphics_RecordError(GRAPHICS_ERROR_GPU); }
        if (space == 0xFFCU) { return GRAPHICS_OK; }
        if (HAL_GetTick() - started >= EVE_FIFO_TIMEOUT_MS) {
            return Graphics_RecordError(GRAPHICS_ERROR_GPU);
        }
        osDelay(1U);
    }
}

/* 각 refresh는 EVE display list 전체를 다시 만든다. 픽셀 버퍼를 전송하지
 * 않는다. VERTEX_FORMAT(0)은 LVGL EVE renderer의 integer pixel 계약이다. */
static void EveRenderStart(lv_event_t *event)
{
    (void)event;
    g_graphics.render_busy = 1U;
    g_graphics.render_started_ms = HAL_GetTick();
    const uint32_t wait_started=HAL_GetTick();
    while(EVE_memRead8(REG_DLSWAP)){
        if(HAL_GetTick()-wait_started>=EVE_FIFO_TIMEOUT_MS){Graphics_RecordError(GRAPHICS_ERROR_GPU);Graphics_AssertFail(__FILE_NAME__,__LINE__);}
        osDelay(1U);
    }
    EVE_start_cmd_burst();
    EVE_cmd_dl_burst(CMD_DLSTART);
    /* 프레임 시작 시 CLEAR 범위를 항상 전체 화면으로 맞춘다. */
    lv_eve_scissor(0U, 0U, GRAPHICS_WIDTH - 1U, GRAPHICS_HEIGHT - 1U);
    /* Clock/footer intentionally exclude the photo and expose this clear.
     * Use the same palette as their ink; a raw black clear would make dark
     * light-theme text unreadable even though every LVGL color is mapped. */
#if defined(NOODOE_PRODUCT) && NOODOE_PRODUCT
    EVE_cmd_dl_burst(DL_CLEAR_COLOR_RGB | Theme_Color(0U));
#else
    EVE_cmd_dl_burst(DL_CLEAR_COLOR_RGB);
#endif
    EVE_cmd_dl_burst(DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG);
    EVE_cmd_dl_burst(VERTEX_FORMAT(0));
    /* BEGIN/END의 primitive는 SAVE_CONTEXT 대상이 아니다. 이전 frame의
     * raw 원형 제외 명령 뒤에도 실제 primitive와 vendor cache가 일치하도록
     * 서로 다른 유효 값을 보낸다. zero sentinel은 upstream에서 무시된다. */
    lv_eve_primitive(EVE_POINTS);
    lv_eve_primitive(EVE_RECTS);
    if (bitmap_baseline_pending) {
        /* BSP의 PDN reset 뒤에도 upstream C의 static bitmap cache는 남을 수
         * 있다. 서로 다른 두 값을 보내 기본/확장 비트까지 동기화한다. bitmap
         * 속성은 프레임 사이 유지되므로 수명 시작 때 한 번만 필요하다. */
        EVE_cmd_dl_burst(BITMAP_HANDLE(0));
        lv_eve_bitmap_source(4U);
        lv_eve_bitmap_source(0U);
        lv_eve_bitmap_size(EVE_NEAREST, EVE_BORDER, EVE_BORDER, 512U, 512U);
        lv_eve_bitmap_size(EVE_NEAREST, EVE_BORDER, EVE_BORDER, 1U, 1U);
        lv_eve_bitmap_layout(EVE_L4, 1024U, 512U);
        lv_eve_bitmap_layout(EVE_L4, 1U, 1U);
        bitmap_baseline_pending = 0U;
    }
}

/* 최종 flush에서 DISPLAY/SWAP을 보내고 실제 command FIFO 완료를 확인한다.
 * LVGL 완료 통지는 성공한 전송 뒤에만 한다. CPU frame 수와 EVE scanout 수를
 * 분리해서 기록하며 빈 REFR_READY 이벤트를 렌더 프레임으로 세지 않는다. */
static void EveFlush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area;
    (void)pixels;
    if (lv_display_flush_is_last(display)) {
        Graphics_EveApplyViewport();
        EVE_cmd_dl_burst(DL_DISPLAY);
        EVE_end_cmd_burst();
        if (EveWaitIdle() != GRAPHICS_OK) { Graphics_AssertFail(__FILE_NAME__, __LINE__); }
        g_graphics.eve_cmd_dl = EVE_memRead32(REG_CMD_DL);
        if (g_graphics.eve_cmd_dl > 8192U) {
            Graphics_RecordError(GRAPHICS_ERROR_GPU);
            Graphics_AssertFail(__FILE_NAME__, __LINE__);
        }
        /* Validation precedes publication. An oversized/failed frame must
         * never replace the last complete scanout, even for one refresh. */
        EVE_start_cmd_burst();EVE_cmd_dl_burst(CMD_SWAP);EVE_end_cmd_burst();
        if(EveWaitIdle()!=GRAPHICS_OK){Graphics_AssertFail(__FILE_NAME__,__LINE__);}
        ++g_graphics.render_count;
        g_graphics.last_render_ms = HAL_GetTick();
        g_graphics.last_render_duration_ms = g_graphics.last_render_ms - g_graphics.render_started_ms;
        if (g_graphics.last_render_duration_ms > g_graphics.max_render_duration_ms) {
            g_graphics.max_render_duration_ms = g_graphics.last_render_duration_ms;
        }
    }
    lv_display_flush_ready(display);
}

uint32_t Graphics_FramePresentedAfter(uint32_t frame)
{
    return !Graphics_DisplaySleeping()&&!g_graphics.render_busy&&
        g_graphics.render_count!=frame&&EVE_memRead8(REG_DLSWAP)==0U;
}

static void EveRenderReady(lv_event_t *event)
{
    (void)event;
    g_graphics.render_busy = 0U;
    /* Only this graphics owner processes snapshot SPI. Idle requests cost RAM
     * checks only; chunk export reads a frozen GPU image while UI keeps running. */
    BSP_Display_CaptureProcess();
}

/* EVE 화면 회전 레지스터를 사용한다. 480x480이므로 회전 후 외형 치수는 같다.
 * 터치는 이 포트에 구현하지 않았으며 터치 좌표 변환을 지원한다고 하지 않는다. */
static void EveResolutionChanged(lv_event_t *event)
{
    lv_display_t *display = lv_event_get_target(event);
    const uint8_t map[4] = {0U, 2U, 1U, 3U};
    const uint32_t rotation = (uint32_t)lv_display_get_rotation(display);
    if (rotation < 4U) { EVE_memWrite8(REG_ROTATE, map[rotation]); }
}

lv_display_t *Graphics_EveCreateDisplay(void)
{
    static const lv_draw_eve_parameters_t parameters = {
        .hor_res = 480U, .ver_res = 480U,
        .hcycle = 550U, .hoffset = 37U, .hsync0 = 0U, .hsync1 = 4U,
        .vcycle = 505U, .voffset = 18U, .vsync0 = 0U, .vsync1 = 2U,
        .swizzle = 0U, .pclkpol = 0U, .cspread = 0U, .pclk = 3U,
        .has_crystal = true, .has_gt911 = false,
        .backlight_pwm = 0U, .backlight_freq = 500U
    };
    lv_display_t *display = lv_display_create(GRAPHICS_WIDTH, GRAPHICS_HEIGHT);
    if (!display) { return NULL; }
    /* EVE renderer는 draw buffer 픽셀에 접근하지 않는다. upstream factory와
     * 같은 dummy 등록으로 full-refresh 배치만 구성한다. SW renderer는 끈다. */
    lv_display_set_flush_cb(display, EveFlush);
    lv_display_set_buffers(display, &dummy_draw_buffer, NULL,
        GRAPHICS_WIDTH * GRAPHICS_HEIGHT * LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_NATIVE),
        LV_DISPLAY_RENDER_MODE_FULL);
    lv_draw_eve_set_display_data(display, &parameters, EveIo);
    upstream_dispatch = lv_draw_eve_unit_g->base_unit.dispatch_cb;
    lv_draw_eve_unit_g->base_unit.dispatch_cb = EveDispatch;
#if defined(NOODOE_PRODUCT) && NOODOE_PRODUCT
    upstream_evaluate=lv_draw_eve_unit_g->base_unit.evaluate_cb;
    lv_draw_eve_unit_g->base_unit.evaluate_cb=EveEvaluate;
#endif
    bitmap_baseline_pending = 1U;
    lv_display_add_event_cb(display, EveResolutionChanged, LV_EVENT_RESOLUTION_CHANGED, NULL);
    lv_display_add_event_cb(display, EveRenderStart, LV_EVENT_RENDER_START, NULL);
    lv_display_add_event_cb(display, EveRenderReady, LV_EVENT_RENDER_READY, NULL);
    if (EveWaitIdle() != GRAPHICS_OK) { lv_display_delete(display); return NULL; }
    return display;
}

Graphics_Status Graphics_EvePoll(void)
{
    if (g_graphics.render_busy || g_bsp_eve_bus.selected) { return GRAPHICS_OK; }
    g_graphics.eve_frames = EVE_memRead32(REG_FRAMES);
    g_graphics.eve_cmd_read = EVE_memRead16(REG_CMD_READ);
    g_graphics.eve_cmd_write = EVE_memRead16(REG_CMD_WRITE);
    g_graphics.eve_cmd_dl = EVE_memRead32(REG_CMD_DL);
    g_graphics.eve_cpu_reset = EVE_memRead8(REG_CPURESET);
    g_graphics.eve_pclk = EVE_memRead8(REG_PCLK);
    g_graphics.hardware_sample_ms = HAL_GetTick();
    g_graphics.ramg_used = lv_draw_eve_unit_g->ramg.ramg_addr_end;
    g_graphics.ramg_entries = lv_draw_eve_unit_g->ramg.hash_table_cells_occupied;
    g_graphics.spi_tx_bytes = g_bsp_eve_bus.tx_bytes;
    g_graphics.spi_rx_bytes = g_bsp_eve_bus.rx_bytes;
    g_graphics.spi_failures = g_bsp_eve_bus.failures;
    if (g_graphics.eve_cmd_read == 0xFFFU || g_graphics.eve_cpu_reset != 0U ||
        g_graphics.eve_pclk != 3U || g_graphics.eve_cmd_dl > 8192U) {
        return Graphics_RecordError(GRAPHICS_ERROR_GPU);
    }
    return GRAPHICS_OK;
}

void Graphics_EveReleaseCache(void)
{
    if (lv_draw_eve_unit_g) {
        lv_free(lv_draw_eve_unit_g->ramg.hash_table);
        memset(&lv_draw_eve_unit_g->ramg, 0, sizeof(lv_draw_eve_unit_g->ramg));
    }
}

/* 모든 asset을 회수하는 명시적 조작이다. 이전 DL이 참조하는 RAM_G를 덮기
 * 전에 blank frame으로 바꾼다. 다음 refresh에서 필요한 glyph/image를 다시
 * 올린다. 이 API는 LRU나 화면 중단 없는 이중 asset bank를 구현하지 않는다. */
Graphics_Status Graphics_EveResetCache(void)
{
    static const uint32_t blank[] = {DL_CLEAR_COLOR_RGB, DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG, DL_DISPLAY};
    if (g_graphics.render_busy || g_bsp_eve_bus.selected) { return GRAPHICS_ERROR_STATE; }
    if (EveWaitIdle() != GRAPHICS_OK) { return GRAPHICS_ERROR_GPU; }
    if (BSP_EVE_SubmitDisplayList(blank, 3U) != BSP_EVE_OK) { return Graphics_RecordError(GRAPHICS_ERROR_IO); }
    Graphics_EveReleaseCache();
    ++g_graphics.cache_resets;
    g_graphics.ramg_used = 0U;
    g_graphics.ramg_entries = 0U;
    return GRAPHICS_OK;
}

Graphics_Status Graphics_EvePreloadImage(const lv_image_dsc_t *image)
{
    if (!image || !lv_draw_eve_image_src_check(image)) { return GRAPHICS_ERROR_UNSUPPORTED; }
    /* upstream은 descriptor의 data_size를 확인하지 않고 stride*h만큼 읽는다.
     * 공개 API에서는 미압축 raw descriptor만 받고 alpha plane도 크기에 포함한다.
     * 포인터가 실제로 그 범위를 가리키는지까지 C에서 증명할 수는 없다. */
    if (!image->data || !image->header.w || !image->header.h) { return GRAPHICS_ERROR_ARGUMENT; }
    if (image->header.flags & LV_IMAGE_FLAGS_COMPRESSED) { return GRAPHICS_ERROR_UNSUPPORTED; }
    const uint32_t row_min = image->header.w * lv_color_format_get_size(image->header.cf);
    const uint32_t stride = image->header.stride ? image->header.stride : row_min;
    if (stride < row_min) { return GRAPHICS_ERROR_ARGUMENT; }
    uint64_t required = (uint64_t)stride * image->header.h;
    if (image->header.cf == LV_COLOR_FORMAT_RGB565A8) {
        if (stride & 1U) { return GRAPHICS_ERROR_ARGUMENT; }
        required += (uint64_t)(stride / 2U) * image->header.h;
    }
    if (required > image->data_size) { return GRAPHICS_ERROR_ARGUMENT; }
    /* EVE generation 2 bitmap dimension/layout fields are finite even when RAM_G
     * has space. Normal Noodoe assets (<=480x480) are comfortably inside them. */
    const uint32_t eve_stride = (image->header.cf == LV_COLOR_FORMAT_L8 ||
        image->header.cf == LV_COLOR_FORMAT_RGB565) ? stride : image->header.w * 2U;
    if (image->header.w > 2047U || image->header.h > 2047U || eve_stride > 4095U) {
        return GRAPHICS_ERROR_UNSUPPORTED;
    }
    if (g_graphics.render_busy || g_bsp_eve_bus.selected) { return GRAPHICS_ERROR_STATE; }
    const uint32_t address = lv_draw_eve_image_upload_image(false, image);
    if (address == LV_DRAW_EVE_RAMG_OUT_OF_RAMG) { return GRAPHICS_ERROR_MEMORY; }
    g_graphics.ramg_used = lv_draw_eve_unit_g->ramg.ramg_addr_end;
    g_graphics.ramg_entries = lv_draw_eve_unit_g->ramg.hash_table_cells_occupied;
    return GRAPHICS_OK;
}

Graphics_Status Graphics_EvePreloadText(const lv_font_t *font, const char *text)
{
    if (!font || !text) { return GRAPHICS_ERROR_ARGUMENT; }
    if (!lv_draw_eve_label_font_check(font)) { return GRAPHICS_ERROR_UNSUPPORTED; }
    if (g_graphics.render_busy || g_bsp_eve_bus.selected) { return GRAPHICS_ERROR_STATE; }
    for (uint32_t offset = 0U; text[offset];) {
        uint32_t current, next;
        lv_text_encoded_letter_next_2(text, &current, &next, &offset);
        lv_font_glyph_dsc_t glyph;
        if (lv_font_get_glyph_dsc_fmt_txt(font, &glyph, current, next)) {
            if (lv_draw_eve_label_upload_glyph(false, font->dsc, glyph.gid.index) == LV_DRAW_EVE_RAMG_OUT_OF_RAMG) {
                return GRAPHICS_ERROR_MEMORY;
            }
        }
    }
    g_graphics.ramg_used = lv_draw_eve_unit_g->ramg.ramg_addr_end;
    g_graphics.ramg_entries = lv_draw_eve_unit_g->ramg.hash_table_cells_occupied;
    return GRAPHICS_OK;
}
