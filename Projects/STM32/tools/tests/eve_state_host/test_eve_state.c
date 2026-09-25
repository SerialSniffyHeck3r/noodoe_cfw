/* 실제 upstream lv_eve.c와 production adapter 발췌를 호출하는 명령 스트림 시험.
 * SPI/RTOS/MCU 대신 EVE_cmd_dl_burst 경계만 기록한다. 모델은 clip/bitmap
 * 명령과 SAVE/RESTORE의 clip 상태만 표현하며 GPU 전체/시각 품질을 흉내 내지 않는다. */
#include <stdint.h>
#include <stddef.h>
#include "Graphics.h"
#include "Graphics_Viewport.h"
#include "src/draw/eve/lv_eve.h"
#include "src/draw/eve/lv_draw_eve_private.h"
#include "src/draw/lv_draw_private.h"
#include "src/misc/lv_area_private.h"
#include "src/libs/FT800-FT813/EVE_commands.h"

volatile uint32_t g_test_case;
volatile uint32_t g_test_failure_line;
volatile uint32_t g_test_assertions;
volatile uint32_t g_test_mock_error;
static uint32_t words[64];
static uint32_t count;
static uint32_t hw_xy, hw_size, saved_xy, saved_size;
static uint32_t hw_source, hw_bsize, hw_bsize_h, hw_layout, hw_layout_h, hw_handle;
static uint32_t available, searches, dispatches, dispatch_requests;
static uint32_t draw_fill_in_dispatch;
static uint32_t hw_primitive, last_vertex_primitive, vertex_count;
static lv_draw_fill_dsc_t fill_dsc;
/* A bounded state model covers the context registers touched by the viewport.
 * It validates command ordering/state restoration, not EVE pixel rasterization. */
static uint32_t context_state[64], saved_context_state[64];
static uint32_t point_state[64], rect_state[64], pre_viewport_state[64];
static uint32_t point_vertices, rect_vertices, stencil_clears, point_saw_clear;
static uint32_t point_vertex, rectangle_vertices[2];
static uint16_t target[4];
static lv_draw_task_t task;
static lv_draw_unit_t unit;
static lv_layer_t layer;
static uint32_t bitmap_baseline_pending;
void Graphics_EveApplyViewport(void);

static uint32_t IsContextCommand(uint32_t opcode)
{
    switch(opcode) {
        case DL_SCISSOR_XY: case DL_SCISSOR_SIZE: case DL_VERTEX_FORMAT:
        case DL_VERTEX_TRANSLATE_X: case DL_VERTEX_TRANSLATE_Y:
        case DL_ALPHA_FUNC: case DL_TAG_MASK: case DL_STENCIL_MASK:
        case DL_CLEAR_STENCIL: case DL_COLOR_MASK: case DL_STENCIL_FUNC:
        case DL_STENCIL_OP: case DL_POINT_SIZE: case DL_COLOR_RGB:
        case DL_COLOR_A: case DL_BLEND_FUNC: case DL_LINE_WIDTH:
            return 1U;
        default:return 0U;
    }
}

/* 순수 명령 sink. SAVE/RESTORE는 bitmap 속성을 되돌리지 않는다는 EVE 계약을
 * 보존한다. 전체 프레임 리셋에 대한 가정은 넣지 않는다. */
void EVE_cmd_dl_burst(uint32_t command)
{
    if (count >= sizeof(words) / sizeof(words[0])) {
        g_test_mock_error = 1U;
        return;
    }
    words[count++] = command;
    const uint32_t opcode=command & 0xFF000000U;
    /* FT81x guide4.5/4.30: BEGIN/END are actions, not SAVE_CONTEXT state.
     * Keeping this separate exposes the upstream C cache's false restoration. */
    if(opcode==DL_BEGIN) hw_primitive=command;
    if(command==DL_END) hw_primitive=0U;
    if(IsContextCommand(opcode)) context_state[opcode>>24]=command;
    if(command==DL_SAVE_CONTEXT) for(uint32_t i=0;i<64;++i) saved_context_state[i]=context_state[i];
    if(command==DL_RESTORE_CONTEXT) for(uint32_t i=0;i<64;++i) context_state[i]=saved_context_state[i];
    if(command==(DL_CLEAR|CLR_STN)) ++stencil_clears;
    if((command&0xC0000000U)==0x40000000U) {
        ++vertex_count;last_vertex_primitive=hw_primitive;
        if(hw_primitive==(DL_BEGIN|EVE_POINTS)) {
            ++point_vertices;point_vertex=command;point_saw_clear=stencil_clears;
            for(uint32_t i=0;i<64;++i) point_state[i]=context_state[i];
        } else if(hw_primitive==(DL_BEGIN|EVE_RECTS)) {
            if(rect_vertices<2U) rectangle_vertices[rect_vertices]=command;
            ++rect_vertices;
            for(uint32_t i=0;i<64;++i) rect_state[i]=context_state[i];
        }
    }
    switch (command & 0xFF000000U) {
        case DL_SCISSOR_XY: hw_xy = command; break;
        case DL_SCISSOR_SIZE: hw_size = command; break;
        case DL_BITMAP_SOURCE: hw_source = command; break;
        case DL_BITMAP_SIZE: hw_bsize = command; break;
        case DL_BITMAP_SIZE_H: hw_bsize_h = command; break;
        case DL_BITMAP_LAYOUT: hw_layout = command; break;
        case DL_BITMAP_LAYOUT_H: hw_layout_h = command; break;
        case DL_BITMAP_HANDLE: hw_handle = command; break;
        case DL_SAVE_CONTEXT: saved_xy = hw_xy; saved_size = hw_size; break;
        case DL_RESTORE_CONTEXT: hw_xy = saved_xy; hw_size = saved_size; break;
        default: break;
    }
}

void lv_draw_dispatch_request(void) { ++dispatch_requests; }

/* 예상하지 못한 LVGL assert는 성공으로 바꾸지 않는다. 러너의 시간/명령 수
 * 제한이 종료시키며 failure_line도 남긴다. */
void Graphics_AssertFail(const char *file, uint32_t line)
{
    (void)file;
    g_test_failure_line = line;
    for (;;) { }
}

/* 실제 scheduler 대신 available task 유무를 공급한다. wrapper가 전달하는
 * layer/previous/id와 upstream 반환값을 별도로 확인한다. */
lv_draw_task_t *lv_draw_get_next_available_task(lv_layer_t *given_layer,
                                               lv_draw_task_t *previous,
                                               uint8_t draw_unit_id)
{
    ++searches;
    if (given_layer != &layer || previous != NULL || draw_unit_id != 9U) {
        g_test_mock_error = 2U;
    }
    return available ? &task : NULL;
}

void __wrap_lv_eve_scissor(uint16_t,uint16_t,uint16_t,uint16_t);
static int32_t MockDispatch(lv_draw_unit_t *given_unit, lv_layer_t *given_layer)
{
    ++dispatches;
    if (given_unit != &unit || given_layer != &layer) { g_test_mock_error = 3U; }
    if (available) {
        __wrap_lv_eve_scissor(target[0], target[1], target[2], target[3]);
        /* Actual unchanged upstream fill C includes its own save/draw/restore.
         * No fake primitive setter is substituted for this regression path. */
        if(draw_fill_in_dispatch) lv_draw_eve_fill(&task,&fill_dsc,&task.area);
    }
    return 117;
}

static int32_t (*upstream_dispatch)(lv_draw_unit_t *, lv_layer_t *) = MockDispatch;

/* 러너가 adapter에서 무수정 추출한 실제 wrapper와 bitmap 초기화 블록이다. */
#include "dispatch_from_production.inc"
#include "bitmap_seed_from_production.inc"

/* 테스트용 하드웨어 PDN을 모델링한다. upstream C static 변수에는 접근하지
 * 않으므로 과거 bitmap cache는 그대로 남는다. */
static void ResetHardwareBitmap(void)
{
    hw_source = BITMAP_SOURCE(0U);
    hw_bsize = BITMAP_SIZE(EVE_NEAREST, EVE_BORDER, EVE_BORDER, 0U, 0U);
    hw_bsize_h = BITMAP_SIZE_H(0U, 0U);
    hw_layout = BITMAP_LAYOUT(EVE_ARGB1555, 0U, 0U);
    hw_layout_h = BITMAP_LAYOUT_H(0U, 0U);
    hw_handle = BITMAP_HANDLE(0U);
}

static void SetBitmap(uint32_t source, uint16_t width, uint16_t height, uint16_t stride)
{
    lv_eve_bitmap_source(source);
    lv_eve_bitmap_size(EVE_NEAREST, EVE_BORDER, EVE_BORDER, width, height);
    lv_eve_bitmap_layout(EVE_L4, stride, height);
}

#define CHECK(expression) do { ++g_test_assertions; if (!(expression)) { \
    g_test_failure_line = __LINE__; return 1; } } while (0)
#define CLEAR_RECORD() do { count = 0U; } while (0)

/* 14개 case를 순서대로 실행한다. 순서는 C 캐시와 모의 GPU 상태를 의도적으로
 * 갈라 놓기 위해 고정한다. 실패 시 마지막 case/줄을 ELF 심벌에서 읽는다. */
int EveState_TestMain(void)
{
    g_test_case = 1U;
    /* 회귀 재현: 오른쪽/아래 끝이 같은데 x1만 바꾸면 원본은 SIZE를 생략한다. */
    lv_eve_scissor(1U, 1U, 2U, 2U);
    lv_eve_scissor(0U, 0U, 479U, 479U);
    CHECK(hw_size == SCISSOR_SIZE(482U, 482U));
    CLEAR_RECORD();
    lv_eve_scissor(100U, 0U, 479U, 479U);
    CHECK(count == 1U);
    CHECK(words[0] == SCISSOR_XY(99U, 0U));
    CHECK(hw_size != SCISSOR_SIZE(382U, 482U));

    g_test_case = 2U;
    /* 실제 adapter wrapper는 그 동일 clip을 올바른 XY+SIZE로 복구한다. */
    available = 1U;
    task.area=task._real_area=task.clip_area=(lv_area_t){0,0,479,479};
    target[0] = 100U; target[1] = 0U; target[2] = 479U; target[3] = 479U;
    CLEAR_RECORD();
    CHECK(EveDispatch(&unit, &layer) == 117);
    CHECK(count == 4U);
    CHECK(words[0] == (DL_BEGIN|EVE_RECTS));
    CHECK(words[1] == (DL_BEGIN|EVE_RECTS));
    CHECK(words[2] == SCISSOR_XY(100U, 0U));
    CHECK(words[3] == SCISSOR_SIZE(380U, 480U));
    CHECK(hw_xy == SCISSOR_XY(100U, 0U));
    CHECK(hw_size == SCISSOR_SIZE(380U, 480U));

    g_test_case = 3U;
    /* border와 같은 SAVE -> 새 clip -> RESTORE 순서. C scissor cache는
     * restore되지 않아 같은 clip의 다음 draw에서 명령이 모두 생략된다. */
    lv_eve_scissor(1U, 1U, 2U, 2U);
    lv_eve_scissor(0U, 0U, 479U, 479U);
    lv_eve_save_context();
    lv_eve_scissor(10U, 20U, 200U, 200U);
    lv_eve_restore_context();
    CHECK(hw_size == SCISSOR_SIZE(482U, 482U));
    CLEAR_RECORD();
    lv_eve_scissor(10U, 20U, 200U, 200U);
    CHECK(count == 0U);
    CHECK(hw_xy != SCISSOR_XY(9U, 19U));
    target[0] = 10U; target[1] = 20U; target[2] = 200U; target[3] = 200U;
    CHECK(EveDispatch(&unit, &layer) == 117);
    CHECK(count == 3U);
    CHECK(hw_xy == SCISSOR_XY(10U, 20U));
    CHECK(hw_size == SCISSOR_SIZE(191U, 181U));

    g_test_case = 4U;
    /* idle 호출은 upstream dispatch를 그대로 호출하지만 DL에는 쓰지 않는다. */
    available = 0U;
    CLEAR_RECORD();
    CHECK(EveDispatch(&unit, &layer) == 117);
    CHECK(count == 0U);
    CHECK(searches == 3U && dispatches == 3U);
    CHECK(g_test_mock_error == 0U);

    g_test_case = 5U;
    /* 작은 bitmap을 사용한 뒤 EVE만 reset. 같은 C setter는 아무것도 보내지
     * 않아서 새 하드웨어의 bitmap 상태를 복구하지 못함을 먼저 재현한다. */
    SetBitmap(0x1000U, 10U, 12U, 5U);
    ResetHardwareBitmap();
    CLEAR_RECORD();
    SetBitmap(0x1000U, 10U, 12U, 5U);
    CHECK(count == 0U);
    CHECK(hw_source != BITMAP_SOURCE(0x1000U));
    bitmap_baseline_pending = 1U;
    SeedBitmapBaseline();
    CHECK(bitmap_baseline_pending == 0U);
    CHECK(count == 11U);
    CHECK(hw_handle == BITMAP_HANDLE(0U));
    CHECK(hw_source == BITMAP_SOURCE(0U));
    CHECK(hw_bsize == BITMAP_SIZE(EVE_NEAREST, EVE_BORDER, EVE_BORDER, 1U, 1U));
    CHECK(hw_bsize_h == BITMAP_SIZE_H(1U, 1U));
    CHECK(hw_layout == BITMAP_LAYOUT(EVE_L4, 1U, 1U));
    CHECK(hw_layout_h == BITMAP_LAYOUT_H(1U, 1U));
    CLEAR_RECORD();
    SetBitmap(0x1000U, 10U, 12U, 5U);
    CHECK(count == 3U);
    CHECK(hw_source == BITMAP_SOURCE(0x1000U));
    CHECK(hw_bsize == BITMAP_SIZE(EVE_NEAREST, EVE_BORDER, EVE_BORDER, 10U, 12U));
    CHECK(hw_layout == BITMAP_LAYOUT(EVE_L4, 5U, 12U));

    g_test_case = 6U;
    /* 높은 비트가 0이 아닌 cache도 lifecycle에서 0으로 맞춰야 한다. 이전
     * high word가 seed 중간 값과 같아 생략되어도 마지막 값은 반드시 복구한다. */
    SetBitmap(0x2000U, 700U, 600U, 1600U);
    CHECK(hw_bsize_h == BITMAP_SIZE_H(700U, 600U));
    CHECK(hw_layout_h == BITMAP_LAYOUT_H(1600U, 600U));
    ResetHardwareBitmap();
    bitmap_baseline_pending = 1U;
    CLEAR_RECORD();
    SeedBitmapBaseline();
    CHECK(hw_source == BITMAP_SOURCE(0U));
    CHECK(hw_bsize_h == BITMAP_SIZE_H(1U, 1U));
    CHECK(hw_layout_h == BITMAP_LAYOUT_H(1U, 1U));
    CLEAR_RECORD();
    SetBitmap(0x2000U, 700U, 600U, 1600U);
    CHECK(count == 5U);
    CHECK(hw_source == BITMAP_SOURCE(0x2000U));
    CHECK(hw_bsize == BITMAP_SIZE(EVE_NEAREST, EVE_BORDER, EVE_BORDER, 700U, 600U));
    CHECK(hw_bsize_h == BITMAP_SIZE_H(700U, 600U));
    CHECK(hw_layout == BITMAP_LAYOUT(EVE_L4, 1600U, 600U));
    CHECK(hw_layout_h == BITMAP_LAYOUT_H(1600U, 600U));

    g_test_case = 7U;
    /* 초기화 flag가 내려가면 다음 프레임에 bitmap 재초기화를 반복하지 않는다. */
    CLEAR_RECORD();
    SeedBitmapBaseline();
    CHECK(count == 0U);
    CHECK(hw_source == BITMAP_SOURCE(0x2000U));
    CHECK(hw_bsize_h == BITMAP_SIZE_H(700U, 600U));

    g_test_case = 8U;
    /* zero setter로 cache를 무효화할 수 없다는 upstream 계약을 확인한다.
     * 실제 다른 유효 primitive로 바뀔 때만 BEGIN이 발행된다. */
    CLEAR_RECORD();
    lv_eve_primitive(LV_EVE_PRIMITIVE_ZERO_VALUE);
    CHECK(count == 0U);
    lv_eve_primitive(LV_EVE_PRIMITIVE_BITMAPS);
    CHECK(count == 1U && words[0] == (DL_BEGIN | LV_EVE_PRIMITIVE_BITMAPS));
    CHECK(g_test_mock_error == 0U);

    g_test_case = 9U;
    /* Entirely outside content is FINISHED without entering the renderer. */
    available=1U;task.state=LV_DRAW_TASK_STATE_WAITING;
    task.area=task._real_area=(lv_area_t){0,0,69,70};
    task.clip_area=(lv_area_t){0,0,479,479};
    uint32_t prior_dispatches=dispatches, prior_requests=dispatch_requests;
    CLEAR_RECORD();
    CHECK(EveDispatch(&unit,&layer)==1);
    CHECK(task.state==LV_DRAW_TASK_STATE_FINISHED && count==0);
    CHECK(dispatches==prior_dispatches && dispatch_requests==prior_requests+1U);
    /* Visible geometry with a disjoint clip also has no draw/upload. */
    task.area=task._real_area=(lv_area_t){220,220,260,260};
    task.clip_area=(lv_area_t){0,0,69,70};task.state=LV_DRAW_TASK_STATE_WAITING;
    CHECK(EveDispatch(&unit,&layer)==1);
    CHECK(task.state==LV_DRAW_TASK_STATE_FINISHED && count==0 && dispatches==prior_dispatches);

    g_test_case = 10U;
    /* Rendered extents, not nominal object bounds, determine culling. */
    task.area=(lv_area_t){0,0,69,70};task._real_area=(lv_area_t){0,0,90,90};
    task.clip_area=(lv_area_t){0,0,479,479};task.state=LV_DRAW_TASK_STATE_WAITING;
    target[0]=0;target[1]=0;target[2]=90;target[3]=90;
    CLEAR_RECORD();CHECK(EveDispatch(&unit,&layer)==117);
    CHECK(dispatches==prior_dispatches+1U && count==4U);
    /* The same real area clipped to its outside corner must be dropped. */
    task.clip_area=(lv_area_t){0,0,69,70};CLEAR_RECORD();
    CHECK(EveDispatch(&unit,&layer)==1 && count==0U);
    /* Radius240 at center239.5 includes both central rows/columns through
     * the raster's extreme pixels. Radius239.5 culled all eight point tasks.
     * The last point checks the adjacent diagonal boundary:69,70 is outside
     * as tested above, but70,70 now reaches the production dispatch path. */
    const uint16_t visible_points[][2]={
        {0U,239U},{0U,240U},{479U,239U},{479U,240U},
        {239U,0U},{240U,0U},{239U,479U},{240U,479U},{70U,70U}
    };
    for(uint32_t i=0;i<sizeof(visible_points)/sizeof(visible_points[0]);++i){
        task.area=task._real_area=(lv_area_t){visible_points[i][0],visible_points[i][1],
                                            visible_points[i][0],visible_points[i][1]};
        task.clip_area=(lv_area_t){0,0,479,479};task.state=LV_DRAW_TASK_STATE_WAITING;
        target[0]=target[2]=visible_points[i][0];target[1]=target[3]=visible_points[i][1];
        prior_dispatches=dispatches;prior_requests=dispatch_requests;
        CLEAR_RECORD();CHECK(EveDispatch(&unit,&layer)==117);
        CHECK(dispatches==prior_dispatches+1U && count==3U);
        CHECK(task.state==LV_DRAW_TASK_STATE_WAITING && dispatch_requests==prior_requests);
    }
    /* Expanding that circle must retain corner culling. A point task has no
     * rendered extent that could reach back into the visible disk. */
    const uint16_t corner_points[][2]={{0U,0U},{479U,0U},{0U,479U},{479U,479U}};
    for(uint32_t i=0;i<4U;++i){
        task.area=task._real_area=(lv_area_t){corner_points[i][0],corner_points[i][1],
                                            corner_points[i][0],corner_points[i][1]};
        task.state=LV_DRAW_TASK_STATE_WAITING;
        prior_dispatches=dispatches;prior_requests=dispatch_requests;
        CLEAR_RECORD();CHECK(EveDispatch(&unit,&layer)==1);
        CHECK(task.state==LV_DRAW_TASK_STATE_FINISHED && count==0U);
        CHECK(dispatches==prior_dispatches && dispatch_requests==prior_requests+1U);
    }

    g_test_case = 11U;
    /* Run the actual viewport C against dirty preceding GPU state. */
    lv_eve_scissor(7U,9U,170U,180U);
    const lv_color_t test_color={.red=0x12U,.green=0x34U,.blue=0x56U};
    lv_eve_color(test_color);
    EVE_cmd_dl_burst(VERTEX_FORMAT(0U));
    EVE_cmd_dl_burst(VERTEX_TRANSLATE_X(123));EVE_cmd_dl_burst(VERTEX_TRANSLATE_Y(456));
    EVE_cmd_dl_burst(ALPHA_FUNC(EVE_LESS,99U));EVE_cmd_dl_burst(COLOR_A(23U));
    EVE_cmd_dl_burst(STENCIL_MASK(3U));EVE_cmd_dl_burst(CLEAR_STENCIL(77U));
    EVE_cmd_dl_burst(STENCIL_FUNC(EVE_EQUAL,5U,15U));
    EVE_cmd_dl_burst(STENCIL_OP(EVE_INVERT,EVE_ZERO));
    EVE_cmd_dl_burst(COLOR_MASK(1U,0U,1U,0U));EVE_cmd_dl_burst(TAG_MASK(1U));
    EVE_cmd_dl_burst(LINE_WIDTH(320U));EVE_cmd_dl_burst(BLEND_FUNC(EVE_ZERO,EVE_ONE));
    for(uint32_t i=0;i<64;++i) pre_viewport_state[i]=context_state[i];
    point_vertices=rect_vertices=stencil_clears=point_saw_clear=0U;
    CLEAR_RECORD();Graphics_EveApplyViewport();
    CHECK(count==30U && count*4U==120U);
    CHECK(words[0]==DL_SAVE_CONTEXT && words[count-1U]==DL_RESTORE_CONTEXT);
    CHECK(point_vertices==1U && rect_vertices==2U && stencil_clears==1U && point_saw_clear==1U);
    CHECK(point_vertex==VERTEX2F(3832U,3832U));
    CHECK(point_state[DL_POINT_SIZE>>24]==POINT_SIZE(3840U));
    CHECK(point_state[DL_SCISSOR_XY>>24]==SCISSOR_XY(0U,0U));
    CHECK(point_state[DL_SCISSOR_SIZE>>24]==SCISSOR_SIZE(480U,480U));
    CHECK(point_state[DL_VERTEX_FORMAT>>24]==VERTEX_FORMAT(4U));
    CHECK(point_state[DL_VERTEX_TRANSLATE_X>>24]==VERTEX_TRANSLATE_X(0));
    CHECK(point_state[DL_VERTEX_TRANSLATE_Y>>24]==VERTEX_TRANSLATE_Y(0));
    CHECK(point_state[DL_CLEAR_STENCIL>>24]==CLEAR_STENCIL(0U));
    CHECK(point_state[DL_STENCIL_MASK>>24]==STENCIL_MASK(255U));
    CHECK(point_state[DL_COLOR_MASK>>24]==COLOR_MASK(0U,0U,0U,0U));
    CHECK(point_state[DL_TAG_MASK>>24]==TAG_MASK(0U));
    CHECK(point_state[DL_ALPHA_FUNC>>24]==ALPHA_FUNC(EVE_ALWAYS,0U));
    CHECK(point_state[DL_COLOR_A>>24]==COLOR_A(255U));
    CHECK(point_state[DL_STENCIL_FUNC>>24]==STENCIL_FUNC(EVE_ALWAYS,1U,255U));
    CHECK(point_state[DL_STENCIL_OP>>24]==STENCIL_OP(EVE_REPLACE,EVE_REPLACE));
    CHECK(rect_state[DL_SCISSOR_XY>>24]==SCISSOR_XY(0U,0U));
    CHECK(rect_state[DL_SCISSOR_SIZE>>24]==SCISSOR_SIZE(480U,480U));
    CHECK(rect_state[DL_STENCIL_FUNC>>24]==STENCIL_FUNC(EVE_NOTEQUAL,1U,255U));
    CHECK(rect_state[DL_STENCIL_OP>>24]==STENCIL_OP(EVE_KEEP,EVE_KEEP));
    CHECK(rect_state[DL_COLOR_MASK>>24]==COLOR_MASK(1U,1U,1U,1U));
#if GRAPHICS_DEV_VIEWPORT
    CHECK(rect_state[DL_COLOR_RGB>>24]==COLOR_RGB(0x4AU,0x59U,0x52U));
#else
    CHECK(rect_state[DL_COLOR_RGB>>24]==COLOR_RGB(0U,0U,0U));
#endif
    CHECK(rect_state[DL_COLOR_A>>24]==COLOR_A(255U));
    CHECK(rect_state[DL_BLEND_FUNC>>24]==BLEND_FUNC(EVE_ONE,EVE_ZERO));
    CHECK(rect_state[DL_LINE_WIDTH>>24]==LINE_WIDTH(16U));
    CHECK(rectangle_vertices[0]==VERTEX2F(0U,0U));
    CHECK(rectangle_vertices[1]==VERTEX2F(480U*16U,480U*16U));
    for(uint32_t i=0;i<64;++i) CHECK(context_state[i]==pre_viewport_state[i]);

    g_test_case = 12U;
    /* Raw viewport commands must not corrupt the upstream C-state cache. */
    CLEAR_RECORD();lv_eve_scissor(7U,9U,170U,180U);lv_eve_color(test_color);
    CHECK(count==0U);
    CHECK(hw_xy==SCISSOR_XY(6U,8U) && hw_size==SCISSOR_SIZE(166U,174U));
    CHECK(context_state[DL_COLOR_RGB>>24]==COLOR_RGB(0x12U,0x34U,0x56U));
    Graphics_EveApplyViewport();CHECK(count==30U && count*4U==120U);
    CHECK(g_test_mock_error==0U);

    g_test_case = 13U;
    /* Legacy label-like SAVE/BEGIN(BITMAPS)/RESTORE leaves real EVE in BITMAPS
     * although vendor ct again says RECTS. The next actual upstream rectangle
     * fill suppresses BEGIN and emits its corners as BITMAP vertices. This is
     * an expected failing rendering sequence, not a simulated cache assignment. */
    CLEAR_RECORD();
    lv_eve_primitive(EVE_POINTS);lv_eve_primitive(EVE_RECTS);
    lv_eve_save_context();lv_eve_primitive(EVE_BITMAPS);lv_eve_restore_context();
    CHECK(hw_primitive==(DL_BEGIN|EVE_BITMAPS));
    task.area=task._real_area=(lv_area_t){220,220,260,240};
    task.clip_area=(lv_area_t){0,0,479,479};
    fill_dsc.radius=0;fill_dsc.opa=255;fill_dsc.color=test_color;
    CLEAR_RECORD();uint32_t previous_vertices=vertex_count;
    lv_draw_eve_fill(&task,&fill_dsc,&task.area);
    CHECK(vertex_count==previous_vertices+2U);
    CHECK(last_vertex_primitive==(DL_BEGIN|EVE_BITMAPS));
    for(uint32_t i=0;i<count;++i) CHECK((words[i]&0xFF000000U)!=DL_BEGIN);

    g_test_case = 14U;
    /* The production dispatch boundary repairs exactly the same dirty state.
     * An arc-like POINTS ending and raw END are included as other legal previous
     * tasks. Each case uses actual upstream fill, and the repair adds no vertex. */
    available=draw_fill_in_dispatch=1U;
    target[0]=target[1]=0U;target[2]=target[3]=479U;
    const uint32_t preceding[]={EVE_BITMAPS,EVE_POINTS,EVE_EDGE_STRIP_R};
    for(uint32_t kind=0;kind<3U;++kind){
        CLEAR_RECORD();
        lv_eve_primitive(EVE_POINTS);lv_eve_primitive(EVE_RECTS);
        lv_eve_save_context();lv_eve_primitive(preceding[kind]);lv_eve_restore_context();
        if(kind==2U)EVE_cmd_dl_burst(DL_END);
        previous_vertices=vertex_count;CLEAR_RECORD();
        CHECK(EveDispatch(&unit,&layer)==117);
        CHECK(vertex_count==previous_vertices+2U);
        CHECK(last_vertex_primitive==(DL_BEGIN|EVE_RECTS));
        uint32_t begins=0U;
        for(uint32_t i=0;i<count;++i)if((words[i]&0xFF000000U)==DL_BEGIN)++begins;
        CHECK(begins==1U && hw_primitive==(DL_BEGIN|EVE_RECTS));
    }
    draw_fill_in_dispatch=0U;
    CHECK(g_test_mock_error==0U);
    g_test_case=15U;
    /* Move either edge, restore another context, then request the same clip.
     * Hardware must equal the inclusive LVGL rectangle with no adjacent bleed. */
    for(uint16_t y=144;y<=172;++y){
        CLEAR_RECORD();__wrap_lv_eve_scissor(72,y,407,397);
        CHECK(count==2U&&hw_xy==SCISSOR_XY(72,y)&&hw_size==SCISSOR_SIZE(336,398-y));
        EVE_cmd_dl_burst(DL_SAVE_CONTEXT);
        __wrap_lv_eve_scissor(80,y+1,300,398);EVE_cmd_dl_burst(DL_RESTORE_CONTEXT);
        CLEAR_RECORD();__wrap_lv_eve_scissor(80,y+1,300,398);
        CHECK(count==2U&&hw_xy==SCISSOR_XY(80,y+1)&&hw_size==SCISSOR_SIZE(221,398-y));
    }
    return 0;
}
