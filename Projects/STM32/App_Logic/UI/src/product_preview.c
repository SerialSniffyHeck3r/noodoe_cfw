#include "Product_Preview.h"
#include "Graphics_Viewport.h"
#include "App_DataConfig.h"
#include <string.h>
volatile ProductPreviewMailbox g_product_preview;
#if GRAPHICS_DEV_VIEWPORT && DATA_DEBUG
static uint32_t seen,footer,ratio,valid;
static UiState demo;
#endif
/* MCU boot initializes the protocol; stale pre-reset requests cannot execute. */
void ProductPreview_Init(void)
{
    memset((void*)&g_product_preview,0,sizeof(g_product_preview));
    g_product_preview.magic=0x50565031U;g_product_preview.version=2U;
#if GRAPHICS_DEV_VIEWPORT && DATA_DEBUG
    seen=0U;
#endif
}
/* Actual button input always removes the presentation override immediately. */
void ProductPreview_Cancel(void){g_product_preview.active_id=0U;}

/* Execute a visual preview without dispatching a single product event/effect.
 * Test fractions do not enter published domain snapshots or persistent data. */
uint32_t ProductPreview_Apply(const UiState *state,uint32_t now,UiDashboardPresentation *out)
{
    g_product_preview.last_ms=now;
#if GRAPHICS_DEV_VIEWPORT && DATA_DEBUG
    uint32_t id=g_product_preview.request_id;
    if(id&&id!=seen){
        uint32_t f=g_product_preview.footer,r=g_product_preview.remaining_permille;
        uint32_t v=g_product_preview.valid,ttl=g_product_preview.ttl_ms;
        __sync_synchronize();if(id!=g_product_preview.request_id)return 0U;
        seen=id;g_product_preview.result=0U;
        if(f==UINT32_MAX)ProductPreview_Cancel();
        else if(f>=UI_FOOTER_COUNT||r>1000U||v>1U||ttl<1000U||ttl>180000U){
            g_product_preview.result=2U;ProductPreview_Cancel();
        }else{footer=f;ratio=r;valid=v;g_product_preview.active_id=id;g_product_preview.expires_ms=now+ttl;}
        __sync_synchronize();g_product_preview.ack_id=id;
    }
    if(g_product_preview.active_id&&(int32_t)(now-g_product_preview.expires_ms)>=0)ProductPreview_Cancel();
    if(!g_product_preview.active_id||!state||!out)return 0U;
    demo=*state;demo.power=UI_RUNNING;demo.menu=0U;demo.modal=0U;demo.warning=0U;
    demo.dashboard.card=UI_TRIP;
    demo.dashboard.footer=footer;
    UiDashboardDistances distances={0};distances.valid_mask=(1U<<UI_FOOTER_COUNT)-1U;
    for(uint32_t i=0;i<UI_FOOTER_COUNT;++i)distances.distance_mm[i]=123400000ULL;
    /* Reserve preview reuses ratio as tenths of a kilometre (0..100km).
     * This is labelled test data only; valid0 exercises the unknown trip. */
    if(footer==UI_RESV){distances.distance_mm[UI_RESV]=(uint64_t)ratio*100000ULL;
        if(!valid)distances.valid_mask&=~(1U<<UI_RESV);}
    UiDashboardMaintenance maintenance={.valid_mask=valid?7U:0U,.remaining_permille={ratio,ratio,ratio}};
    maintenance.days_valid_mask=valid?7U:0U;
    maintenance.elapsed_days[1]=maintenance.elapsed_days[2]=ratio;
    maintenance.oil_hours_valid=valid;maintenance.oil_ignition_ms=(uint64_t)ratio*360000ULL;
    UiDashboard_Present(&demo,&distances,36475U,1U,0U,0U,out);
    UiDashboard_PresentMaintenance(&maintenance,out);
    strcpy(out->title,"TEST DATA");strcpy(out->line,UiDashboard_FooterTitle(footer));
    strcpy(out->hint,"Preview only - not saved");return 1U;
#else
    uint32_t id=g_product_preview.request_id;
    if(id!=g_product_preview.ack_id){g_product_preview.active_id=0;g_product_preview.result=2;
        __sync_synchronize();g_product_preview.ack_id=id;}
    (void)state;(void)out;return 0U;
#endif
}
