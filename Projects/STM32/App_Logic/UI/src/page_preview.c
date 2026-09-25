#include "Page_Preview.h"
#include "Graphics_Viewport.h"
#include "App_DataConfig.h"
#include <string.h>
volatile PagePreviewMailbox g_page_preview;
#if GRAPHICS_DEV_VIEWPORT && DATA_DEBUG
static uint32_t seen,card,selection,flags,began;
static UiState demo;
#endif
void PagePreview_Init(void){memset((void*)&g_page_preview,0,sizeof(g_page_preview));g_page_preview.magic=0x50475031;g_page_preview.version=2;
#if GRAPHICS_DEV_VIEWPORT && DATA_DEBUG
    seen=0;
#endif
}
void PagePreview_Cancel(void){g_page_preview.active_id=0;}
uint32_t PagePreview_Apply(const UiState *s,const UiDashboardPresentation *shell,const DashboardPageFacts *facts,DashboardPage *p,uint32_t now)
{
    g_page_preview.now_ms=now;
#if GRAPHICS_DEV_VIEWPORT && DATA_DEBUG
    uint32_t id=g_page_preview.request_id;
    if(id&&id!=seen){
        uint32_t c=g_page_preview.card,n=g_page_preview.selection,f=g_page_preview.flags,ttl=g_page_preview.ttl_ms;
        __sync_synchronize();if(id!=g_page_preview.request_id)return now;
        seen=id;g_page_preview.result=0;
        if(c==UINT32_MAX)PagePreview_Cancel();
        else if(c>=UI_CARD_COUNT||!(UI_CARD_MASK&(1U<<c))||n>=8U||(f&~6U)||ttl<1000U||ttl>180000U){g_page_preview.result=2;PagePreview_Cancel();}
        else{card=c;selection=n;flags=f;began=now;g_page_preview.active_id=id;g_page_preview.expires_ms=now+ttl;}
        __sync_synchronize();g_page_preview.ack_id=id;
    }
    if(g_page_preview.active_id&&(int32_t)(now-g_page_preview.expires_ms)>=0)PagePreview_Cancel();
    if(!g_page_preview.active_id)return now;
    demo=*s;demo.dashboard.card=card;demo.dashboard.selection=selection;demo.dashboard.item_direction=(flags&4U)?2U:1U;
    demo.menu=demo.modal=demo.warning=0;demo.power=UI_RUNNING;
    UiDashboardPresentation base=*shell;
    DashboardPages_Present(&demo,&base,facts,p);
    p->key^=0x40000000U;
    return flags&2U&&(uint32_t)(now-began)>120U?began+120U:now;
#else
    uint32_t id=g_page_preview.request_id;
    if(id!=g_page_preview.ack_id){g_page_preview.active_id=0;g_page_preview.result=2;
        __sync_synchronize();g_page_preview.ack_id=id;}
    (void)s;(void)shell;(void)facts;(void)p;return now;
#endif
}
