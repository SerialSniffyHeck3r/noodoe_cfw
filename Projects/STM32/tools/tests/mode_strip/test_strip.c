#include "Product_ModeStrip.h"
#include "Product_ModeIcons.h"
static uint32_t assertions,creates,draws;
static lv_obj_t obj;static lv_layer_t layer;
static void (*callback)(lv_event_t *);
static lv_image_dsc_t masks[8];
static lv_area_t coords[8];static lv_draw_image_dsc_t images[8];
static uint32_t drawn[8];
static int root_x,root_y,root_w,root_h;
#define CHECK(x) do{++assertions;if(!(x))return __LINE__;}while(0)
uint32_t get_assertions(void){return assertions;}
lv_obj_t *lv_obj_create(lv_obj_t *p){(void)p;++creates;return &obj;}
void lv_obj_remove_style_all(lv_obj_t *p){(void)p;}
void lv_obj_set_size(lv_obj_t *p,int w,int h){(void)p;root_w=w;root_h=h;}
void lv_obj_set_pos(lv_obj_t *p,int x,int y){(void)p;root_x=x;root_y=y;}
void lv_obj_remove_flag(lv_obj_t *p,uint32_t f){(void)p;(void)f;}
void lv_obj_add_event_cb(lv_obj_t *p,void (*cb)(lv_event_t *),uint32_t e,void *u){(void)p;(void)e;(void)u;callback=cb;}
void lv_obj_invalidate(lv_obj_t *p){(void)p;}
void lv_obj_get_coords(lv_obj_t *p,lv_area_t *a){(void)p;*a=(lv_area_t){root_x,root_y,root_x+root_w-1,root_y+root_h-1};}
lv_layer_t *lv_event_get_layer(lv_event_t *e){(void)e;return &layer;}
void lv_draw_image(lv_layer_t *l,const lv_draw_image_dsc_t *d,const lv_area_t *a){(void)l;int n=(const lv_image_dsc_t *)d->src-masks;if(n>=0&&n<8){images[n]=*d;coords[n]=*a;drawn[n]=1;}++draws;}
const lv_image_dsc_t *Product_ModeIcon(uint32_t n){return n<8?&masks[n]:NULL;}
static void paint(void){draws=0;memset(drawn,0,sizeof(drawn));callback(NULL);}
uint32_t test_navigation_motion(void)
{
    CHECK(!ProductModeStrip_Create(NULL));CHECK(ProductModeStrip_Create(&obj));
    CHECK(root_x==120&&root_y==97&&root_w==240&&root_h==40);
    paint();CHECK(draws==0);
    ProductModeStrip_Update(0,1000);paint();
    CHECK(draws==5&&creates==1);CHECK(g_mode_strip.weights[0]==255&&g_mode_strip.weights[1]==0);
    CHECK(images[0].scale_x==384&&images[1].scale_x==235);
    CHECK(images[0].recolor==0xFFFFFF&&images[1].recolor==0x666666);
    CHECK(g_mode_strip.centers_x[0]==240&&g_mode_strip.centers_x[1]==288);
    /* All eight forward steps, including7->0, move left exactly one pitch.
     * Shapes crossing the viewport edges are clipped, never wrapped onscreen. */
    const uint32_t modes[6]={0,1,2,3,6,7};
    for(uint32_t ordinal=0;ordinal<6;++ordinal){uint32_t step=modes[ordinal],next=modes[(ordinal+1)%6],start=1000+ordinal*300,previous=0;int prior_x=240;
      ProductModeStrip_Update(next,start);
      for(uint32_t dt=0;dt<240;++dt){ProductModeStrip_Update(next,start+dt);
        CHECK(g_mode_strip.weights[next]>=previous);previous=g_mode_strip.weights[next];
        CHECK(g_mode_strip.centers_x[step]<=prior_x&&g_mode_strip.centers_x[step]>=192);prior_x=g_mode_strip.centers_x[step];
        CHECK(g_mode_strip.centers_x[next]-g_mode_strip.centers_x[step]==48);
        paint();CHECK(draws>=5&&draws<=6&&!drawn[4]&&!drawn[5]);
        if(dt==120){CHECK(g_mode_strip.centers_x[step]==216&&g_mode_strip.centers_x[next]==264);
            CHECK(g_mode_strip.weights[step]==128&&g_mode_strip.weights[next]==128);
            CHECK(images[step].scale_x==309&&images[next].scale_x==309);}
        for(uint32_t n=0;n<8;++n)if(drawn[n]){int size=(24*images[n].scale_x+255)/256;
            CHECK(coords[n].x1<360&&coords[n].x1+size>120);CHECK(coords[n].y1>=97&&coords[n].y1+size<=137);}
      }
      ProductModeStrip_Update(next,start+240);paint();CHECK(draws==5);
      CHECK(g_mode_strip.current==next&&!g_mode_strip.active&&g_mode_strip.centers_x[next]==240);
      CHECK(g_mode_strip.weights[next]==255&&images[next].recolor==0xFFFFFF);
    }
    /* Direct category jumps remain forward; only adjacent navigation is one pitch. */
    ProductModeStrip_Update(7,3500);ProductModeStrip_Update(7,3620);
    CHECK(g_mode_strip.centers_x[0]>=118&&g_mode_strip.centers_x[0]<=122&&g_mode_strip.centers_x[7]-g_mode_strip.centers_x[0]==240);
    ProductModeStrip_Update(7,3740);paint();CHECK(images[7].scale_x==384&&images[7].recolor==0xFFFFFF);
    /* Same mode never starts again. Rapid requests coalesce after completion. */
    ProductModeStrip_Update(7,3800);CHECK(!g_mode_strip.active);
    ProductModeStrip_Update(0,3900);ProductModeStrip_Update(1,3950);
    ProductModeStrip_Update(UINT32_MAX,4140);CHECK(g_mode_strip.current==0&&g_mode_strip.target==1&&g_mode_strip.active);
    ProductModeStrip_Update(UINT32_MAX,4380);CHECK(g_mode_strip.current==1&&!g_mode_strip.active);
    ProductModeStrip_Update(99,4500);CHECK(g_mode_strip.current==1&&creates==1);
    /* MCU tick wrap cannot turn a240ms movement into a long jump. */
    ProductModeStrip_Update(2,0xFFFFFFC0U);ProductModeStrip_Update(2,56U);
    CHECK(g_mode_strip.elapsed_ms==120&&g_mode_strip.centers_x[1]==216&&g_mode_strip.centers_x[2]==264);
    ProductModeStrip_Update(2,176U);CHECK(g_mode_strip.current==2&&!g_mode_strip.active);
    ProductModeStrip_SetOpacity(0,5000);ProductModeStrip_SetOpacity(0,5120);paint();
    CHECK(images[2].opa>100&&images[2].opa<155);
    ProductModeStrip_SetOpacity(1,5120);paint();CHECK(images[2].opa>100&&images[2].opa<155);
    ProductModeStrip_SetOpacity(1,5360);paint();CHECK(images[2].opa==255);
    ProductModeStrip_SetOpacity(0,5400);ProductModeStrip_SetOpacity(0,5640);paint();CHECK(!draws);
    return 0;
}
