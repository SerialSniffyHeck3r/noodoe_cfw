
#include "Graphics.h"
#include "Graphics_Background.h"
#include "Scalar_Transition.h"
#include "Wallpaper_Runtime.h"
#include "Wallpaper.h"
#include "PhotoService.h"
#include "PhoneVisual.h"
uint32_t PhoneVisual_AcquireArt(uint32_t key,PhoneVisualImage *out){(void)key;(void)out;return 0;}
void PhoneVisual_ReleaseArt(uint32_t bank){(void)bank;}
#include "src/draw/eve/lv_eve.h"
#include <string.h>
volatile Graphics_Diagnostics g_graphics;
static uint32_t ticks,swap_pending,capture_busy,writes,bad_write,counts[3],provider_calls;
static int32_t provider_result=1;
uint32_t PhotoService_Get(uint32_t slot,PhotoImage *photo){(void)slot;(void)photo;return 0;}
const void *PhotoService_Retiring(uint32_t slot){(void)slot;return 0;}
void PhotoService_Release(uint32_t slot,const void *pixels){(void)slot;(void)pixels;}
void Graphics_ServiceCapture(void){capture_busy=0;}
uint32_t lv_tick_get(void){return ticks;}
uint32_t BSP_Display_CaptureBusy(void){return capture_busy;}
uint32_t Graphics_FramePresentedAfter(uint32_t frame){return g_graphics.render_count!=frame&&!swap_pending;}
uint8_t EVE_memRead8(uint32_t address){if(address!=REG_DLSWAP)bad_write=1;return swap_pending;}
void EVE_memWrite_flash_buffer(uint32_t address,const uint8_t *data,uint32_t count)
{
 if(address==BACKGROUND_SHADE_RAM){if(count!=480)bad_write=1;return;}
 uint32_t bank,base;
 if(address>=BACKGROUND_IMAGE_B_RAM){bank=1;base=BACKGROUND_IMAGE_B_RAM;}
 else if(address>=BACKGROUND_IMAGE_RAM){bank=0;base=BACKGROUND_IMAGE_RAM;}
 else {bank=2;base=BACKGROUND_SMALL_RAM;}
 uint32_t size=bank==2?2048:BACKGROUND_IMAGE_BYTES;
 if(!count||count>BACKGROUND_DIRECT_UPLOAD_CHUNK||address<base||address-base+count>size)bad_write=2;
 for(uint32_t i=0;i<count;++i)if(data[i]!=(uint8_t)(address-base+i))bad_write=3;
 counts[bank]+=count;++writes;
}
static int32_t Read(void *context,uint32_t offset,uint8_t *dst,uint32_t count)
{(void)context;++provider_calls;if(provider_result==1)for(uint32_t i=0;i<count;++i)dst[i]=(uint8_t)(offset+i);return provider_result;}
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
static void Init(void){ticks=0;GraphicsBackground_Init();memset((void*)&g_graphics,0,sizeof(g_graphics));swap_pending=capture_busy=writes=bad_write=provider_calls=0;memset(counts,0,sizeof(counts));provider_result=1;}
static BackgroundImage Photo(uint32_t revision){BackgroundImage a={.width=480,.height=480,.bytes=BACKGROUND_IMAGE_BYTES,.revision=revision,.read=Read};return a;}
static void Step(uint32_t time){ticks=time;++g_graphics.render_count;GraphicsBackground_Process();}
static void Upload(void){for(uint32_t i=0;i<460;++i)Step(ticks+1);}
uint32_t test_stream(void)
{
 Init();BackgroundImage a=Photo(1);CHECK(GraphicsBackground_SetImage(&a));
 capture_busy=1;GraphicsBackground_Process();CHECK(!writes&&!provider_calls);capture_busy=0;
 g_graphics.render_busy=1;GraphicsBackground_Process();CHECK(!writes);g_graphics.render_busy=0;
 provider_result=0;GraphicsBackground_Process();CHECK(!writes&&provider_calls==1);provider_result=1;
 Upload();Step(ticks+240);CHECK(g_background.ready&&g_background.alpha==255&&counts[0]==BACKGROUND_IMAGE_BYTES&&!bad_write);
 uint32_t before=writes;CHECK(GraphicsBackground_SetCenterBrightness(40));uint32_t at=ticks;
 Step(at+120);CHECK(g_background.display_percent==70);Step(at+240);CHECK(g_background.display_percent==40&&writes==before);
 CHECK(!GraphicsBackground_SetCenterBrightness(101));
 CHECK(GraphicsBackground_SetImage(0));Step(ticks+240);CHECK(!g_background.ready&&!g_background.alpha);
 CHECK(GraphicsBackground_SetImage(&a));Step(ticks+120);
 CHECK(g_background.ready&&g_background.alpha>0&&g_background.alpha<255&&writes==before);
 Step(ticks+120);CHECK(g_background.ready&&g_background.alpha==255&&writes==before);return 0;
}
uint32_t test_first_upload_fades_after_ready(void)
{
 Init();BackgroundImage a=Photo(1);CHECK(GraphicsBackground_SetImage(&a));
 for(uint32_t i=0;i<449;++i)Step(ticks+1);
 CHECK(!g_background.ready);Step(ticks+1);CHECK(g_background.ready&&g_background.alpha==0);
 Step(ticks+120);CHECK(g_background.alpha>100&&g_background.alpha<155);
 Step(ticks+120);CHECK(g_background.alpha==255);return 0;
}
/* Both large banks remain intact through a blend. A third latest request may
 * start upload only after that blend completes; no fade-through-black stage. */
uint32_t test_two_large_and_latest(void)
{
 Init();BackgroundImage a=Photo(1),b=Photo(2),c=Photo(3);
 CHECK(GraphicsBackground_SetImage(&a));Upload();Step(ticks+240);
 CHECK(GraphicsBackground_SetImage(&b));Upload();CHECK(g_background.phase==5&&g_background.ready&&g_background.alpha==255);
 uint32_t before=writes,at=ticks;CHECK(GraphicsBackground_SetImage(&c));Step(at+100);CHECK(writes==before);
 Step(at+240);CHECK(writes>before&&g_background.alpha==255);Upload();Step(ticks+240);
 CHECK(g_background.image_uploads==3&&counts[1]==BACKGROUND_IMAGE_BYTES&&counts[0]==2*BACKGROUND_IMAGE_BYTES&&!bad_write);
 return 0;
}
uint32_t test_music_cache(void)
{
 Init();BackgroundImage a=Photo(1),m=Photo(2);m.width=m.height=32;m.bytes=2048;
 CHECK(GraphicsBackground_SetImage(&a));Upload();Step(ticks+240);
 CHECK(GraphicsBackground_SetImage(&m));Step(ticks+1);Step(ticks+1);CHECK(counts[2]==2048);
 uint32_t before=writes;Step(ticks+100);CHECK(GraphicsBackground_SetImage(&a));Step(ticks+240);
 CHECK(g_background.alpha==255&&g_background.resident_width==480&&writes==before);
 for(uint32_t i=0;i<1000;++i){CHECK(GraphicsBackground_SetImage(i&1?&a:&m));Step(ticks+240);Step(ticks+240);CHECK(g_background.ready&&g_background.alpha==255&&writes==before);}
 CHECK(g_background.image_uploads==2&&!bad_write);return 0;
}
uint32_t test_failed_inactive_preserves_current(void)
{
 Init();BackgroundImage a=Photo(1),b=Photo(2);CHECK(GraphicsBackground_SetImage(&a));Upload();Step(ticks+240);
 uint32_t before=writes;provider_result=-1;CHECK(GraphicsBackground_SetImage(&b));Step(ticks+1);
 CHECK(g_background.read_error&&g_background.ready&&g_background.alpha==255&&writes==before);
 uint32_t calls=provider_calls;for(uint32_t i=0;i<100;++i)Step(ticks+1);CHECK(calls==provider_calls);
 return 0;
}
uint32_t test_scalar_transition(void)
{
    ScalarTransition a={0};ScalarTransition_Request(&a,255,240,0xfffffff0U);
    uint32_t previous=0;
    for(uint32_t i=0;i<=240;++i){uint32_t now=0xfffffff0U+i;
        ScalarTransition_Request(&a,255,240,now);uint32_t v=ScalarTransition_Value(&a,now);
        CHECK(v>=previous&&v<=255);previous=v;}
    CHECK(previous==255&&!a.duration_ms);
    ScalarTransition_Request(&a,0,240,500);uint32_t midway=ScalarTransition_Value(&a,600);
    ScalarTransition_Request(&a,255,240,600);CHECK(a.value==midway);
    CHECK(ScalarTransition_Value(&a,840)==255);
    ScalarTransition_Request(&a,20,0,841);CHECK(ScalarTransition_Value(&a,841)==20);
    return 0;
}
/* The entire provisional second must have zero GPU writes, including shade.
 * Dim starts only on the subsequent confirmed shutdown request. */
uint32_t test_hold_frame(void)
{
 Init();BackgroundImage a=Photo(1),b=Photo(2),c=Photo(3);
 CHECK(GraphicsBackground_SetImage(&a));Upload();Step(ticks+240);
 CHECK(GraphicsBackground_SetImage(&b));Upload();
 CHECK(GraphicsBackground_SetCenterBrightness(40));Step(ticks+80);
 CHECK(g_background.phase==5);
 uint32_t before=writes,requests=g_background.requests,alpha=g_background.alpha,shades=g_background.shade_uploads,light=g_background.display_percent;
 GraphicsBackground_HoldFrame(1);
 CHECK(g_background.display_percent==light&&g_background.shade_uploads==shades);
 CHECK(!GraphicsBackground_SetImage(&c)&&!GraphicsBackground_SetCenterBrightness(100));
 Step(ticks+999);GraphicsBackground_HoldFrame(1);
 CHECK(writes==before&&g_background.requests==requests&&g_background.alpha==alpha&&g_background.phase==5);
 CHECK(g_background.shade_uploads==shades&&g_background.display_percent==light);
 GraphicsBackground_HoldFrame(0);Step(ticks);
 CHECK(g_background.phase==5); /* The1000ms pause did not finish the240ms blend. */
 CHECK(GraphicsBackground_SetCenterBrightness(20));Step(ticks);
 CHECK(g_background.display_percent==light); /* No instantaneous jump. */
 Step(ticks+120);CHECK(g_background.display_percent>20&&g_background.display_percent<light);
 Step(ticks+120);CHECK(g_background.phase==0&&g_background.display_percent==20);
 CHECK(GraphicsBackground_SetCenterBrightness(10));Step(ticks+240);
 GraphicsBackground_HoldFrame(1);Step(ticks+999);CHECK(g_background.display_percent==10);
 GraphicsBackground_HoldFrame(0);
 CHECK(GraphicsBackground_SetCenterBrightness(100));Step(ticks+240);
 shades=g_background.shade_uploads;capture_busy=1;GraphicsBackground_HoldFrame(1);
 CHECK(g_background.shade_uploads==shades&&writes==before);
 Step(ticks+50);capture_busy=0;GraphicsBackground_HoldFrame(1);Step(ticks+949);
 CHECK(g_background.shade_uploads==shades&&writes==before&&g_background.display_percent==100);
 GraphicsBackground_HoldFrame(0);CHECK(!bad_write);return 0;
}
/* Exercise the actual App runtime as well as the renderer: cancelling a
 * provisional OFF must resume the music source, not request the selected photo
 * merely because the top-level state is still IGN_STARTING. */
uint32_t test_runtime_warm_reversal(void)
{
 Init();Wallpaper_Init();BackgroundImage a=Photo(1);CHECK(Wallpaper_SetPhoto(0,&a));
 CHECK(GraphicsBackground_SetImage(&a));Upload();Step(ticks+240);
 static uint8_t art[PHONE_ART_BYTES];for(uint32_t i=0;i<sizeof(art);++i)art[i]=(uint8_t)i;
 UiState state={.power=IGN_ON};DashboardPage page={.kind=UI_MUSIC,.known=1,.art_valid=1,.art=art};
 WallpaperRuntime_Process(&page,&state);Step(ticks+240);Step(ticks+240);
 CHECK(g_background.width==32&&g_background.resident_width==32);
 uint32_t requests=g_background.requests;
 WallpaperRuntime_HoldShutdown(1);Step(ticks+500);
 CHECK(g_background.display_percent==32&&g_background.requests==requests);
 WallpaperRuntime_HoldShutdown(0);state.power=IGN_STARTING;state.welcome_active=0;
 WallpaperRuntime_Process(&page,&state);CHECK(g_background.requests==requests&&g_background.width==32);
 Step(ticks+240);CHECK(g_background.display_percent==32);
 state.power=IGN_STOPPING;state.off_phase=UI_OFF_DELAY;
 WallpaperRuntime_HoldShutdown(1);WallpaperRuntime_Process(&page,&state);Step(ticks+999);
 CHECK(g_background.requests==requests&&g_background.display_percent==32);
 WallpaperRuntime_HoldShutdown(0);state.off_phase=UI_OFF_SUMMARY;
 WallpaperRuntime_Process(&page,&state);CHECK(g_background.display_percent==32);
 Step(ticks+120);CHECK(g_background.display_percent>20&&g_background.display_percent<32);
 Step(ticks+120);
 CHECK(g_background.width==480&&g_background.resident_width==480&&g_background.display_percent==20&&!bad_write);
 return 0;
}
