#include <stdint.h>
#include <string.h>
typedef struct {int32_t x1,y1,x2,y2;} lv_area_t;
typedef struct {int32_t x,y;} Object;
typedef struct {lv_area_t _clip_area;} lv_layer_t;
typedef struct {Object p1,p2;uint32_t width,opa,color,round_start,round_end;} lv_draw_line_dsc_t;
typedef Object lv_point_precise_t;
static void lv_draw_line_dsc_init(lv_draw_line_dsc_t *d){memset(d,0,sizeof(*d));}
static void lv_draw_line(lv_layer_t *l,const lv_draw_line_dsc_t *d){(void)l;(void)d;}
typedef struct {uint32_t bg_opa,bg_color,radius;} lv_draw_rect_dsc_t;
typedef struct {const void *src;uint32_t opa,recolor,recolor_opa;Object pivot;} lv_draw_image_dsc_t;
typedef struct {struct {uint32_t h;} header;} Image;
typedef struct {uint32_t known,call_color,panel,reply_count,reply_index,alpha,text_ready,ratio;Object *root;Image text_image;int32_t slide_y;} MusicGraphic;
typedef struct {MusicGraphic *data;lv_layer_t *layer;} lv_event_t;
typedef struct {uint32_t kind,alpha;Object *root;MusicGraphic music;uint32_t trip,labels[12],used;Object *plot_obj;} PageSlot;
enum {UI_CALLS=1,UI_TRIP=2};
static lv_area_t drawn,draw_clip;static uint32_t draws;
static void *lv_event_get_user_data(lv_event_t *e){return e->data;}
static lv_layer_t *lv_event_get_layer(lv_event_t *e){return e->layer;}
static void lv_obj_get_coords(Object *o,lv_area_t *r){*r=(lv_area_t){o->x,o->y,480,480};}
static void lv_obj_set_pos(Object *o,int32_t x,int32_t y){o->x=x;o->y=y;}
static void lv_obj_invalidate(Object *o){(void)o;}
static void lv_draw_image_dsc_init(lv_draw_image_dsc_t *d){memset(d,0,sizeof(*d));}
static void lv_draw_rect_dsc_init(lv_draw_rect_dsc_t *d){memset(d,0,sizeof(*d));}
static const void *Product_CallIcon(void){return 0;}
static uint32_t lv_color_white(void){return 0xffffff;}
static uint32_t lv_color_hex(uint32_t x){return x;}
static void lv_draw_image(lv_layer_t *l,const lv_draw_image_dsc_t *d,const lv_area_t *a){(void)d;drawn=*a;draw_clip=l->_clip_area;draws++;}
static void lv_draw_rect(lv_layer_t *l,const lv_draw_rect_dsc_t *d,const lv_area_t *a){(void)l;(void)d;(void)a;}
static uint32_t lv_area_intersect(lv_area_t *o,const lv_area_t *a,const lv_area_t *b){lv_area_t r={a->x1>b->x1?a->x1:b->x1,a->y1>b->y1?a->y1:b->y1,a->x2<b->x2?a->x2:b->x2,a->y2<b->y2?a->y2:b->y2};*o=r;return r.x1<=r.x2&&r.y1<=r.y2;}
/* This test keeps alpha constant to exercise positioning independently of
 * opacity/style APIs. The real Alpha function returns at that boundary. */
#include "position.inc"
#include "progress.inc"
volatile uint32_t failure,checks;
#define CHECK(c) do{checks++;if(!(c)){failure=__LINE__;return;}}while(0)
void Test(void){
 Object root={0},inner={100,150};PageSlot s={.root=&root,.kind=UI_CALLS,.alpha=255,.music={.root=&inner,.panel=1,.text_ready=128,.alpha=255,.text_image={{128}}}};
 lv_layer_t layer={{0,0,479,479}};lv_event_t e={&s.music,&layer};
 Alpha(&s,255,0,40);CHECK(root.y==0&&s.music.slide_y==40);Progress(&e);
 CHECK(draws==1&&drawn.y1==196&&drawn.y2==323);
 CHECK(draw_clip.y1==156&&draw_clip.y2==283);CHECK(layer._clip_area.y1==0&&layer._clip_area.y2==479);
 Alpha(&s,255,0,-40);Progress(&e);CHECK(root.y==0&&drawn.y1==116&&draw_clip.y1==156&&draw_clip.y2==283);
 Alpha(&s,255,18,0);CHECK(root.x==18&&root.y==0&&s.music.slide_y==0);
 s.kind=UI_TRIP;Alpha(&s,255,0,31);CHECK(root.y==31&&s.music.slide_y==0);
 s.music.text_ready=192;uint32_t start=draws;Progress(&e);
 CHECK(draws==start+4&&draw_clip.y1==288&&draw_clip.y2==327&&drawn.y1==200&&drawn.y2==327);
 layer._clip_area=(lv_area_t){0,0,80,80};uint32_t before=draws;Progress(&e);CHECK(draws==before&&layer._clip_area.x2==80);
}
