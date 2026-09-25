#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "Phone_Trail.h"
#include "Gps_GridFade.h"
#include "Graphics_SubpixelArc.h"
#include "src/draw/eve/lv_eve.h"
#include "src/draw/eve/lv_draw_eve_private.h"
#include "src/draw/lv_draw_private.h"
#include "src/libs/FT800-FT813/EVE_commands.h"

volatile uint32_t failure,old_max,new_max,grid_max,cases,draws,bad_vertices,ring_mask_bytes;
static uint32_t words;
static lv_area_t window={72,149,407,384};
void EVE_cmd_dl_burst(uint32_t command){++words;(void)command;}
void Graphics_AssertFail(const char *file,uint32_t line){(void)file;failure=line;for(;;){}}
void lv_log_add(lv_log_level_t level,const char *file,int line,const char *func,const char *format,...)
{(void)level;(void)file;(void)line;(void)func;(void)format;failure=1;}
/* Replace only task allocation: execute the real unmodified EVE line backend
 * and project scissor wrapper, including the adapter's BEGIN baseline. */
void lv_draw_line(lv_layer_t *layer,const lv_draw_line_dsc_t *d)
{
    (void)layer;
    if(d->opa<=LV_OPA_MIN)return;
    int x1=d->p1.x<d->p2.x?d->p1.x:d->p2.x;
    int x2=d->p1.x>d->p2.x?d->p1.x:d->p2.x;
    int y1=d->p1.y<d->p2.y?d->p1.y:d->p2.y;
    int y2=d->p1.y>d->p2.y?d->p1.y:d->p2.y;
    if(x2<window.x1||x1>window.x2||y2<window.y1||y1>window.y2)return;
    if(x1< -16384||x2>16383||y1< -16384||y2>16383)++bad_vertices;
    lv_draw_task_t task={0};task.clip_area=window;
    ++draws;lv_eve_primitive(EVE_RECTS);EVE_cmd_dl_burst(DL_BEGIN|EVE_RECTS);
    lv_draw_eve_line(&task,d);
}
#include "old_grid.inc"
#include "new_grid.inc"
void lv_draw_arc_dsc_init(lv_draw_arc_dsc_t *d){memset(d,0,sizeof(*d));}
void lv_draw_arc(lv_layer_t *layer,const lv_draw_arc_dsc_t *d)
{
    (void)layer;lv_draw_task_t task={0};task.type=LV_DRAW_TASK_TYPE_ARC;
    task.draw_dsc=(void*)d;task.clip_area=(lv_area_t){0,0,479,479};
    Graphics_DispatchSubpixelArc(&task);
}
void Graphics_EveApplyViewport(void);
#define CHECK(c) do{if(!(c)){failure=__LINE__;return;}}while(0)

void Test(void)
{
    PhoneTrail trail;int16_t grid[PHONE_TRAIL_GRID_LINES][4],out[4];
    lv_draw_line_dsc_t d={0};d.width=1;d.color=(lv_color_t){247,245,242};d.round_start=d.round_end=1;
    /* Rotate and move the actual world grid at every zoom. Alternating both
     * visible sides forces the maximum48-point trail without losing any leg. */
    for(unsigned zoom=0;zoom<6;++zoom)for(unsigned angle=0;angle<360;angle+=10)for(unsigned offset=0;offset<3;++offset){
        PhoneTrail_Init(&trail);
        PhoneTrail_FeedHeading(&trail,1000,1,375000000,1270000000,1,angle*1000);
        trail.anchor.lat-=offset*137;trail.anchor.lon+=offset*193;
        unsigned n=PhoneTrail_ProjectGrid(&trail,grid,GPS_PLOT_WIDTH,GPS_PLOT_HEIGHT,1,zoom);
        uint32_t new_grid=0;
        for(unsigned version=0;version<2;++version){
            words=draws=0;d.width=1;d.color=(lv_color_t){247,245,242};
            for(unsigned i=0;i<n;++i){
                if(version)GridLine(0,&d,&window,grid[i],255);
                else OldGridLine(0,&d,&window,grid[i],255);
            }
            if(version){new_grid=draws;if(draws>grid_max)grid_max=draws;}
            d.width=3;d.opa=255;d.color=(lv_color_t){208,182,72};
            for(unsigned i=1;i<PHONE_TRAIL_POINTS;++i){
                int16_t line[4]={2,(int16_t)(i*4),333,(int16_t)(i*4+2)};
                if(version)PlotLine(0,&d,&window,line,2);
                else{d.p1=(lv_point_precise_t){74,150+i*4};d.p2=(lv_point_precise_t){405,152+i*4};lv_draw_line(0,&d);}
            }
            if(version){if(words*4>new_max)new_max=words*4;CHECK(draws<=new_grid+47);CHECK(words*4<=4800);}
            else if(words*4>old_max)old_max=words*4;
        }
        ++cases;
    }
    /* Isolated geometry comparison only. Whole-page transition budget now
     * has its own full-LVGL regression (gps_frames). Grid spacing is100px. */
    words=0;lv_layer_t layer={0};
    Graphics_DrawSubpixelArcOpacity(&layer,240,240,240,22,13500,40500,0x183440,255);
    Graphics_EveApplyViewport();ring_mask_bytes=words*4;
    CHECK(new_max<old_max);CHECK(bad_vertices==0);
    /* The old vertex at30000 wraps to-2768. Correct clipping must preserve
     * intersections, reject outside parallels and never join gap endpoints. */
    int16_t examples[][4]={{-30000,100,30000,100},{-30000,-30000,30000,30000},
        {30000,100,20000,100},{-30000,-10,30000,-10},{0,0,335,199},
        {20,20,20,20},{-30000,100,168,100},{168,100,30000,100}};
    const uint32_t visible[]={1,1,0,0,1,1,1,1};
    for(unsigned i=0;i<sizeof(examples)/sizeof(examples[0]);++i){
        CHECK(GpsLine_Clip(examples[i],2,out)==visible[i]);
        if(visible[i])for(int j=0;j<4;++j)CHECK(out[j]>=2&&out[j]<((j&1)?GPS_PLOT_HEIGHT-2:GPS_PLOT_WIDTH-2));
        PlotLine(0,&d,&window,examples[i],2);
    }
    CHECK(bad_vertices==0);
    /* Every page opacity and moved page origin retain bounded geometry. */
    for(unsigned alpha=0;alpha<256;++alpha){
        window.x1=20;window.x2=355;
        words=draws=0;
        for(unsigned i=0;i<22;++i)GridLine(0,&d,&window,grid[i],alpha);
        CHECK(draws<=88);CHECK(bad_vertices==0);
    }
}
