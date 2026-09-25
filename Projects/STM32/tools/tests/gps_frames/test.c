#include "lvgl.h"
#include "Graphics.h"
#include "Graphics_Background.h"
#include "SpeedHome_View.h"
#include "Dashboard_PagesView.h"
#include "Product_Fonts.h"
#include "Resources.h"
#include "Gps_GridFade.h"
#include "Product_Theme.h"
#include "Product_ModeStrip.h"
#include "Product_StatusIconsView.h"
#include "Toast_View.h"
#include "Odometer_View.h"
#include "PhoneVisual.h"
#include "src/libs/FT800-FT813/EVE_commands.h"
#include <string.h>
/* Fixed test arena supplies libc only if a bounded formatting helper needs it. */
void *__wrap__sbrk(int n){(void)n;return (void*)-1;}
/* Actual firmware LVGL/layout/EVE code; only physical I/O, time, RAM arena and
 * resources are supplied by the harness. Not a hardware/FPS measurement. */
volatile uint32_t failure,words,max_words,frames,now,background_draws,blend_frames,test_key,test_offset;
volatile uint32_t test_panel_revision,test_fence_block,test_force_dl_bytes,test_swap_busy,texture_writes;
static lv_display_t *display;
static uint8_t arena[262144];static unsigned at;
void *__wrap_pvPortMalloc(size_t n){n=(n+7)&~7U;if(at+n>sizeof(arena))return 0;void *p=arena+at;at+=n;return p;}
void *__wrap_BSP_RAM_Allocate(size_t n){return __wrap_pvPortMalloc(n);}
uint32_t __wrap_HAL_GetTick(void){return now;}
uint32_t __wrap_osDelay(uint32_t ticks){now+=ticks;return 0;}
uint32_t __wrap_Resources_Get(uint32_t id,ResourceView *v){
 uint32_t *header=(uint32_t*)0xD0000000;unsigned count=header[3];
 for(unsigned i=0;i<count;i++){uint32_t *e=header+12+i*4;if(e[0]==id){v->data=(uint8_t*)header+4096+e[1];v->bytes=e[2];return 1;}}return 0;
}
uint32_t __wrap_EVE_memRead16(uint32_t a){(void)a;return 0xffc;}
uint32_t __wrap_EVE_memRead8(uint32_t a){return a==REG_DLSWAP?test_swap_busy:0;}
uint32_t __wrap_EVE_memRead32(uint32_t a){return a==REG_CMD_DL?(test_force_dl_bytes?test_force_dl_bytes:words*4):0;}
void __wrap_EVE_memWrite8(uint32_t a,uint32_t b){(void)a;(void)b;}
void __wrap_EVE_memWrite_flash_buffer(uint32_t a,const void *b,uint32_t n){(void)a;(void)b;if(n==PHONE_PANEL_BYTES)texture_writes++;}
void __wrap_EVE_start_cmd_burst(void){}
void __wrap_EVE_end_cmd_burst(void){}
void __wrap_EVE_cmd_scale_burst(int32_t a,int32_t b){(void)a;(void)b;}
void __wrap_EVE_cmd_dl_burst(uint32_t x){
 if(x==CMD_DLSTART){words=0;return;}
 if(x==CMD_SWAP){if(words>max_words)max_words=words;frames++;return;}
 if(x==CMD_SETMATRIX){words+=6;return;}
 if(x<0xffffff00U)words++;
}
void __wrap_BSP_Display_CaptureProcess(void){}
uint32_t __wrap_BSP_Display_IsSleeping(void){return 0;}
uint32_t __wrap_Graphics_FramePresentedAfter(uint32_t serial){(void)serial;return !test_fence_block;}
uint32_t __wrap_PhoneVisual_CopyPanel(uint32_t key,void *dst,uint32_t *revision){
 if(!key||!test_panel_revision)return 0;
 if(dst&&*revision!=test_panel_revision)memset(dst,0x5a,PHONE_PANEL_BYTES);
 *revision=test_panel_revision;return 144;
}
void __wrap_Graphics_AssertFail(const char *f,uint32_t line){(void)f;failure=line;for(;;){}}
extern lv_display_t *Graphics_EveCreateDisplay(void);
uint32_t Setup(void){
 if(!Product_TextFontsBind()||!Product_NumberFontsBind())return 1;
 lv_init();lv_tick_set_cb(__wrap_HAL_GetTick);display=Graphics_EveCreateDisplay();if(!display)return 2;
 lv_obj_t *screen=lv_display_get_screen_active(display);lv_obj_remove_style_all(screen);
 if(!SpeedHome_Create(screen))return 3;
 if(!OdometerView_Create(screen))return 4;
 BackgroundImage photo={.pixels=(void*)0xD0100000,.width=480,.height=480,.bytes=460800,.revision=1};
 GraphicsBackground_SetImage(&photo);GraphicsBackground_SetCenterBrightness(20);
 for(unsigned i=0;i<150;i++){now+=10;GraphicsBackground_Process();}
 return 0;
}
/* Exercise page slide at every20ms with a pathological47-leg route, both
 * directions and all headings. Long labels +full speed ring stay visible. */
static SpeedHomeModel model;
static UiDashboardPresentation footer;
static DashboardPage page;
static PhoneTrail trail;
static uint32_t last_kind=UINT32_MAX,photo_toggle;
uint32_t __wrap_ButtonFeedback_Visibility(uint32_t time){(void)time;return 255;}
void Frame(uint32_t kind,uint32_t time,uint32_t heading,uint32_t theme){
 now=time;Theme_SetLight(theme,now);Theme_Tick(now);
 if(last_kind!=kind){last_kind=kind;photo_toggle^=1;
  BackgroundImage photo={.pixels=(void*)(0xD0100000+photo_toggle*0x80000),.width=480,.height=480,.bytes=460800,.revision=photo_toggle+1};
  GraphicsBackground_SetImage(&photo);
  for(unsigned i=0;i<40;i++)GraphicsBackground_Process();
 }
 GraphicsBackground_Process();
 if(g_background.phase==5)blend_frames++;
 SpeedHome_InitModel(&model,now,200);model.initialized=1;model.arc_value=10000;model.arc_color=0xff4040;
 model.session_valid=1;model.speed_valid=1;model.peak_ratio=9900;model.peak_color=0xFF4040;model.average_ratio=5000;
 strcpy(model.clock,"23:59");strcpy(footer.footer_title,"TRIP 1");strcpy(footer.footer_value,"99999.9");strcpy(footer.unit,"km");
 footer.remaining_valid=1;footer.remaining_permille=800;
 SpeedHome_Render(&model,&footer,now);
 memset(&page,0,sizeof(page));page.kind=kind;page.key=test_key?test_key:kind;page.known=1;
 if(kind==UI_PHONE_GPS){
  PhoneTrail_Init(&trail);PhoneTrail_FeedHeading(&trail,1000,1,375000000,1270000000,1,heading*1000);
  trail.display.lat+=test_offset;trail.display.lon+=test_offset*2;
  page.grid_count=PhoneTrail_ProjectGrid(&trail,page.grid,336,GPS_PLOT_HEIGHT,1,heading/60);
  page.plot_count=48;for(unsigned i=0;i<48;i++){page.plot[i][0]=i&1?333:2;page.plot[i][1]=4+i*4;}
  page.heading_north=1024;strcpy(page.numbers[0],"1000 m");
 }else{
  strcpy(page.title,kind==UI_TRIP?"SINCE REFUEL":"QUICK SETTINGS");
  for(unsigned i=0;i<6;i++){strcpy(page.lines[i],"abcdefghijkmnpqr");strcpy(page.numbers[i],"99999.9");}
  strcpy(page.numbers[0],"99:59");strcpy(page.note,"Hold O to open settings");page.ratio_permille=650;
 }
 if(kind==UI_NOTIFICATIONS&&test_panel_revision){page.visual_key=100;page.note[0]=page.lines[0][0]=0;}
 DashboardPagesView_Render(&page,now);
 ProductModeStrip_SetOpacity(1,now);ProductModeStrip_Update(kind,now);
 ProductStatusIcons_Render(1,0xffffff,0xffffff,1);
 PopupNotificationSnapshot toast={.visible=1,.elapsed_ms=1000,.duration_ms=3000,.revision=1,.text="Hold O to reset"};
 ToastView_Render(&toast);
 lv_refr_now(display);background_draws=g_background.draws;
}
void OdoModal(void){
 OdometerStatus status={.valid=1,.pending=1,.ready=1,.raw=999999,.display=999999,.revision=1};
 lv_obj_add_flag(SpeedHome_GetContentRoot(),LV_OBJ_FLAG_HIDDEN);
 OdometerView_Render(&status,1,now);lv_refr_now(display);
}

/* Exercise the shipping draw event, not just its pure model helper. Model
 * updates skipped between actual LVGL renders cannot advance the peak. */
static uint32_t marker_draws,marker_peak,marker_color;
void __real_SpeedHome_RecordDisplayed(SpeedHomeModel *m);
void __wrap_SpeedHome_RecordDisplayed(SpeedHomeModel *m){
 __real_SpeedHome_RecordDisplayed(m);marker_draws++;marker_peak=m->peak_ratio;marker_color=m->peak_color;
}
#define MCHECK(c) do{if(!(c)){failure=__LINE__;return;}}while(0)
void MarkerFrames(void){
 SpeedHome_InitModel(&model,now,200);model.session_valid=model.speed_valid=1;model.session_generation=99;
 model.arc_value=3000;model.arc_color=0x997700;SpeedHome_Render(&model,&footer,now);lv_refr_now(display);
 MCHECK(marker_peak==3000&&marker_color==0x997700);
 uint32_t before=marker_draws;
 model.arc_value=9000;model.arc_color=0xFF4040;SpeedHome_Render(&model,&footer,now);
 model.arc_value=4000;model.arc_color=0xAAAA00;SpeedHome_Render(&model,&footer,now);
 MCHECK(marker_draws==before&&marker_peak==3000);
 lv_refr_now(display);MCHECK(marker_draws>before&&marker_peak==4000&&marker_color==0xAAAA00);
 model.arc_value=1000;model.arc_color=0x0000FF;SpeedHome_Render(&model,&footer,now);lv_refr_now(display);
 MCHECK(marker_peak==4000&&marker_color==0xAAAA00);
 model.arc_value=10000;model.arc_override=1;SpeedHome_Render(&model,&footer,now);lv_refr_now(display);
 MCHECK(marker_peak==4000&&marker_color==0xAAAA00);
 model.session_generation++;model.arc_override=0;model.arc_value=0;model.arc_color=0x38A8FF;
 SpeedHome_Render(&model,&footer,now);lv_refr_now(display);MCHECK(marker_peak==0&&marker_color==0x38A8FF);
}

#include "src/draw/eve/lv_draw_eve_private.h"
#include "Product_ModeIcons.h"
#include "Product_MusicIcons.h"
#include "Product_TripIcons.h"
#include "Product_StatusIcons.h"
#include "Product_CallIcon.h"
#include "Settings_Icons.h"
#include "Product_Icons.h"
#include "src/draw/eve/lv_draw_eve_ram_g.h"
uint32_t CacheAll(void){
 const unsigned text_sizes[]={16,20,24,32},number_sizes[]={20,32,36,40,48,64};
 char ascii[96];for(unsigned i=0;i<95;i++)ascii[i]=(char)(32+i);ascii[95]=0;
 extern Graphics_Status Graphics_EvePreloadText(const lv_font_t *,const char *);
 for(unsigned i=0;i<4;i++)if(Graphics_EvePreloadText(Product_TextFont(text_sizes[i]),ascii))return 1;
 for(unsigned i=0;i<6;i++)if(Graphics_EvePreloadText(Product_NumberFont(number_sizes[i]),ascii))return 2;
 for(unsigned i=0;i<PRODUCT_ICON_COUNT;i++)if(Graphics_EvePreloadText(Product_IconFont(24),Product_IconText(i)))return 3;
 /* Exactly the current selectable set: no removed OBD/remote bitmap or old
  * raster fuel warning (fuel warning now uses vector geometry). */
 for(unsigned group=0;group<6;group++)for(unsigned i=0;i<(group==0?8:group==1?6:group==2?6:group==3?4:group==4?2:1);i++){
  if(group==0&&(i==4||i==5))continue;
  const lv_image_dsc_t *im=group==0?Product_ModeIcon(i):group==1?Product_MusicIcon(i):group==2?Settings_Icon(i):group==3?Product_TripIcon(i):group==4?Product_StatusIcon(i+1):Product_CallIcon();
  uint32_t addr;lv_draw_eve_ramg_get_addr(&addr,(uintptr_t)im->data,im->data_size,1);
  if(addr==LV_DRAW_EVE_RAMG_OUT_OF_RAMG)return 4;
 }
 return lv_draw_eve_unit_g->ramg.ramg_addr_end;
}
