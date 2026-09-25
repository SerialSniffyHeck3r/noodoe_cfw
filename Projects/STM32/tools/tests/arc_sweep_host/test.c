#include "Graphics_ArcSweep.h"
#include "Graphics_BringupHUD.h"
#include "Graphics.h"
#include "font_metrics.h"
#include <stdarg.h>

/* freestanding ARM fixture에 필요한 libc 경계만 구현한다. production 함수를
 * 복사하거나 다른 easing 공식을 대신 실행하지 않는다. */
void *memset(void *p,int value,unsigned count)
{ uint8_t *b=p; for(unsigned i=0;i<count;++i)b[i]=(uint8_t)value; return p; }
static int Equal(const char *a,const char *b)
{ while(*a && *a==*b){++a;++b;}return *a==*b; }

const lv_font_t lv_font_montserrat_14={ARC_FONT14_HEIGHT},lv_font_montserrat_40={ARC_FONT40_HEIGHT};
static lv_obj_t parent,pool[32],*last_arc,*last_performance;
static uint32_t next_slot,alloc_calls,fail_at,live_count,invalid_calls;
static uint32_t text_calls,arc_calls,assertions;
static uint32_t external_hud_init,external_hud_process,external_hud_fail;
static Graphics_Performance performance;
static void Observe(lv_obj_t *o);

#if NOODOE_INTEGRATED
#define EXPECTED_OBJECTS 7U
#else
#define EXPECTED_OBJECTS 8U
#endif

/* Integrated status rows belong to another module. Keep this explicit stub
 * allocation-free: the production ArcSweep's own five labels, coordinates,
 * cleanup and call contract are the subject of this regression. */
uint32_t GraphicsBringupHUD_Init(lv_obj_t *p)
{ Observe(p); ++external_hud_init; return !external_hud_fail; }
void GraphicsBringupHUD_Process(uint32_t elapsed_ms)
{ (void)elapsed_ms; ++external_hud_process; }

/* 이미 삭제한 object를 쓰는 production 호출은 조용히 성공시키지 않고 별도
 * 위반 카운터에 기록한다. 시험 끝과 각 삭제 경로에서 반드시0인지 확인한다. */
static void Observe(lv_obj_t *o) { if(!o || !o->live) ++invalid_calls; }
static lv_obj_t *New(lv_obj_t *p,uint32_t kind)
{
    Observe(p);
    if(++alloc_calls==fail_at || next_slot>=32U) return NULL;
    lv_obj_t *o=&pool[next_slot++];
    memset(o,0,sizeof(*o));
    o->live=1;o->kind=kind;o->parent=p;o->next=p->child;p->child=o;
    o->flags=LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICK_FOCUSABLE;
    ++live_count;
    return o;
}
lv_obj_t *lv_obj_create(lv_obj_t *p) { return New(p,0); }
lv_obj_t *lv_arc_create(lv_obj_t *p) { last_arc=New(p,1);return last_arc; }
lv_obj_t *lv_label_create(lv_obj_t *p) { return New(p,2); }

/* LVGL의 root DELETE callback 이후 자식 회수 순서를 모사한다. parent 자체를
 * 삭제하거나 parent의 child만 지운 경우도 production callback이 처리하게 한다. */
void lv_obj_delete(lv_obj_t *o)
{
    Observe(o);
    if(o->deleted){lv_event_t e={o};o->deleted(&e);}
    while(o->child)lv_obj_delete(o->child);
    if(o->parent){
        lv_obj_t **link=&o->parent->child;
        while(*link && *link!=o)link=&(*link)->next;
        if(*link)*link=o->next;
    }
    o->live=0;
    if(o!=&parent)--live_count;
}
void lv_obj_add_event_cb(lv_obj_t *o,void(*cb)(lv_event_t*),uint32_t code,void *user)
{Observe(o);(void)code;(void)user;o->deleted=cb;}
lv_obj_t *lv_event_get_target(lv_event_t *e){return e->target;}
void lv_obj_remove_style_all(lv_obj_t *o){Observe(o);}
void lv_obj_update_layout(lv_obj_t *o){Observe(o);}
void lv_obj_get_content_coords(lv_obj_t *o,lv_area_t *a)
{Observe(o);*a=(lv_area_t){o->x,o->y,o->x+o->w-1,o->y+o->h-1};}
void lv_obj_set_pos(lv_obj_t *o,int32_t x,int32_t y){Observe(o);o->x=x;o->y=y;}
void lv_obj_set_size(lv_obj_t *o,int32_t w,int32_t h){Observe(o);o->w=w;o->h=h;}
void lv_obj_remove_flag(lv_obj_t *o,uint32_t f){Observe(o);o->flags&=~f;}
void lv_obj_set_style_text_font(lv_obj_t *o,const lv_font_t *f,uint32_t p)
{Observe(o);(void)p;o->font=f;}
#define STYLE_NOOP(n) void lv_obj_set_style_##n(lv_obj_t *o,int32_t v,uint32_t p){Observe(o);(void)v;(void)p;}
STYLE_NOOP(text_color) STYLE_NOOP(text_align) STYLE_NOOP(text_opa)
STYLE_NOOP(arc_color) STYLE_NOOP(arc_opa)
STYLE_NOOP(border_width) STYLE_NOOP(shadow_width) STYLE_NOOP(outline_width)
void lv_obj_set_style_arc_width(lv_obj_t *o,int32_t v,uint32_t p){Observe(o);o->width[p]=(uint32_t)v;}
void lv_obj_set_style_pad_all(lv_obj_t *o,int32_t v,uint32_t p){Observe(o);o->pad[p]=v;}
void lv_obj_set_style_arc_rounded(lv_obj_t *o,int32_t v,uint32_t p){Observe(o);o->rounded[p]=(uint32_t)v;}
void lv_obj_set_style_bg_opa(lv_obj_t *o,int32_t v,uint32_t p){Observe(o);o->bg_opa[p]=(uint32_t)v;}
void lv_label_set_long_mode(lv_obj_t *o,uint32_t mode){Observe(o);(void)mode;}
void lv_label_set_text(lv_obj_t *o,const char *s)
{
    Observe(o);uint32_t i=0;
    while(s[i] && i<sizeof(o->text)-1){o->text[i]=s[i];++i;}o->text[i]=0;
    ++text_calls;
    if(Equal(s,"FPS -- CPU --"))last_performance=o;
}
/* 사용 중인 %lu/%% 형식만 받는 기록기다. printf/newlib 전체를 링크하지 않고
 * 실제 production 코드가 전달한 숫자/형식으로 최종 HUD 문자열을 확인한다. */
static uint32_t AppendNumber(char *out,uint32_t index,unsigned long v)
{
    char reverse[12];uint32_t count=0;
    do{reverse[count++]=(char)('0'+v%10U);v/=10U;}while(v);
    while(count)out[index++]=reverse[--count];
    return index;
}
void lv_label_set_text_fmt(lv_obj_t *o,const char *fmt,...)
{
    char out[100];uint32_t i=0;va_list args;va_start(args,fmt);
    while(*fmt){
        if(*fmt=='%' && fmt[1]=='l' && fmt[2]=='u'){
            i=AppendNumber(out,i,va_arg(args,unsigned long));fmt+=3;
        }else if(*fmt=='%' && fmt[1]=='%'){out[i++]='%';fmt+=2;}
        else out[i++]=*fmt++;
    }
    out[i]=0;va_end(args);lv_label_set_text(o,out);
}
void lv_arc_set_mode(lv_obj_t *o,uint32_t m){Observe(o);(void)m;}
void lv_arc_set_range(lv_obj_t *o,int32_t min,int32_t max){Observe(o);o->min=min;o->max=max;}
void lv_arc_set_bg_angles(lv_obj_t *o,int32_t start,int32_t end){Observe(o);o->start=start;o->end=end;}
void lv_arc_set_value(lv_obj_t *o,int32_t value){Observe(o);o->value=value;++arc_calls;}
const volatile Graphics_Performance *Graphics_GetPerformance(void){return &performance;}

/* 시나리오마다 production Destroy를 먼저 거친다. 남은 가짜 heap이 있는지
 * 별도 CHECK를 둔 뒤 다음 시험을 시작하므로 fixture 초기화가 누수를 숨기지 않는다. */
static void Reset(void)
{
    GraphicsArcSweep_Destroy();
    memset(&parent,0,sizeof(parent));memset(pool,0,sizeof(pool));
    parent.live=1;parent.w=480;parent.h=480;
    next_slot=alloc_calls=fail_at=live_count=invalid_calls=text_calls=arc_calls=0;
    external_hud_init=external_hud_process=external_hud_fail=0;
    last_arc=last_performance=NULL;memset(&performance,0,sizeof(performance));
}
#define CHECK(x) do{++assertions;if(!(x))return __LINE__;}while(0)

/* This entry is also run against an output-only copy with the old integrated
 * title width restored. That copy must fail at Init with geometry error 2. */
uint32_t test_integrated_geometry(void)
{
    Reset(); CHECK(GraphicsArcSweep_Init(&parent,240,240,220));
    CHECK(GraphicsArcSweep_GetDiagnostics()->error==0);
    for(uint32_t i=0;i<next_slot;++i)if(pool[i].kind==2){
        int32_t xs[2]={pool[i].x-240,pool[i].x+pool[i].w-1-240};
        int32_t ys[2]={pool[i].y-240,pool[i].y+pool[i].h-1-240};
        lv_area_t a={pool[i].x,pool[i].y,pool[i].x+pool[i].w-1,pool[i].y+pool[i].h-1};
        CHECK(Graphics_IsAreaVisible(&a));
        for(unsigned x=0;x<2;++x)for(unsigned y=0;y<2;++y)
            CHECK(xs[x]*xs[x]+ys[y]*ys[y]<=220*220);
        if(Equal(pool[i].text,"DEMO SPEED")){
#if NOODOE_INTEGRATED
            CHECK(pool[i].w==264 && pool[i].y==82);
#else
            CHECK(pool[i].w==308 && pool[i].y==124);
#endif
        }
    }
    CHECK(external_hud_init==(NOODOE_INTEGRATED?1U:0U));
    GraphicsArcSweep_Destroy(); CHECK(live_count==0 && invalid_calls==0);
    return 0;
}
uint32_t get_last_error(void){return GraphicsArcSweep_GetDiagnostics()->error;}

/* The border-specific API deliberately fills the480x480 widget. It must not
 * weaken generic containment, shrink with stroke width, or move text outside
 * the active circle. Real raster clipping is verified separately on EVE. */
uint32_t test_viewport_geometry(void)
{
    const volatile GraphicsArcSweep_Diagnostics *d=GraphicsArcSweep_GetDiagnostics();
    Reset();CHECK(!GraphicsArcSweep_InitViewport(NULL));CHECK(d->error==1 && !live_count);
    parent.x=1;CHECK(!GraphicsArcSweep_InitViewport(&parent));CHECK(d->error==1);
    parent.x=0;parent.h=479;CHECK(!GraphicsArcSweep_InitViewport(&parent));CHECK(d->error==1);
    parent.h=480;
    CHECK(!GraphicsArcSweep_Init(&parent,240,240,240));CHECK(d->error==1 && !live_count);
    CHECK(GraphicsArcSweep_InitViewport(&parent));CHECK(d->initialized && d->error==0);
    CHECK(last_arc->x==0 && last_arc->y==0 && last_arc->w==480 && last_arc->h==480);
    CHECK(last_arc->pad[LV_PART_MAIN]==0 && last_arc->pad[LV_PART_INDICATOR]==0);
    CHECK(last_arc->width[LV_PART_MAIN]==18 && last_arc->width[LV_PART_INDICATOR]==18);
    CHECK(last_arc->rounded[LV_PART_MAIN] && last_arc->rounded[LV_PART_INDICATOR]);
    CHECK(last_arc->start==135 && last_arc->end==405);
    for(uint32_t i=0;i<next_slot;++i)if(pool[i].kind==2){
        lv_area_t a={pool[i].x,pool[i].y,pool[i].x+pool[i].w-1,pool[i].y+pool[i].h-1};
        CHECK(Graphics_IsAreaVisible(&a));
    }
    GraphicsArcSweep_Process(2000);CHECK(d->value==5000 && d->speed==80);
    GraphicsArcSweep_Process(4000);CHECK(d->value==10000 && d->speed==160);
    GraphicsArcSweep_Process(8000);CHECK(d->value==0 && d->cycles==1);
    /* Switching between APIs must delete the previous tree exactly once. */
    CHECK(GraphicsArcSweep_Init(&parent,240,240,220));CHECK(live_count==EXPECTED_OBJECTS);
    CHECK(GraphicsArcSweep_InitViewport(&parent));CHECK(live_count==EXPECTED_OBJECTS);
    lv_obj_delete(parent.child);CHECK(!d->initialized && !live_count && !invalid_calls);
    for(uint32_t failure=1;failure<=EXPECTED_OBJECTS;++failure){
        Reset();fail_at=failure;CHECK(!GraphicsArcSweep_InitViewport(&parent));
        CHECK(d->error==3 && !d->initialized && !live_count && !parent.child && !invalid_calls);
    }
    GraphicsArcSweep_Destroy();return 0;
}

uint32_t test_main(void)
{
    const volatile GraphicsArcSweep_Diagnostics *d=GraphicsArcSweep_GetDiagnostics();
    /* 잘못된 인자/원점/표시 범위는 자식 할당 전에 실패해야 한다. */
    Reset();CHECK(!GraphicsArcSweep_Init(NULL,240,240,220));CHECK(d->error==1 && live_count==0);
    CHECK(!GraphicsArcSweep_Init(&parent,240,240,127));CHECK(d->error==1 && live_count==0);
    CHECK(!GraphicsArcSweep_Init(&parent,240,240,240));CHECK(d->error==1 && live_count==0);
    parent.x=1;CHECK(!GraphicsArcSweep_Init(&parent,240,240,220));CHECK(d->error==1);
    parent.x=0;parent.w=479;CHECK(!GraphicsArcSweep_Init(&parent,240,240,220));CHECK(d->error==1);
    parent.w=480;CHECK(!GraphicsArcSweep_Init(&parent,0,240,220));CHECK(d->error==2 && live_count==0);

    /* root/arc and every profile-specific label must unwind allocation failure. */
    for(uint32_t failure=1;failure<=EXPECTED_OBJECTS;++failure){
        Reset();fail_at=failure;
        CHECK(!GraphicsArcSweep_Init(&parent,240,240,220));
        CHECK(d->error==3 && d->initialized==0);
        CHECK(live_count==0 && parent.child==NULL && invalid_calls==0);
        GraphicsArcSweep_Process(1234);CHECK(d->process_count==0);
    }

    /* 실제 계획의480 부모/220 반경에서 객체 수, 범위, round cap과 입력 차단을 본다. */
    Reset();CHECK(GraphicsArcSweep_Init(&parent,240,240,220));
    CHECK(d->magic==GRAPHICS_ARC_SWEEP_DIAGNOSTIC_MAGIC && d->version==1 && d->initialized);
    CHECK(live_count==EXPECTED_OBJECTS && last_arc->min==0 && last_arc->max==10000);
    CHECK(last_arc->start==135 && last_arc->end==405 && last_arc->value==0);
    CHECK(last_arc->width[LV_PART_MAIN]==18 && last_arc->width[LV_PART_INDICATOR]==18);
    CHECK(last_arc->rounded[LV_PART_MAIN] && last_arc->rounded[LV_PART_INDICATOR]);
    CHECK(last_arc->bg_opa[LV_PART_KNOB]==0 && last_arc->flags==0);
    for(uint32_t i=0;i<next_slot;++i)if(pool[i].kind==2){
        lv_area_t a={pool[i].x,pool[i].y,pool[i].x+pool[i].w-1,pool[i].y+pool[i].h-1};
        CHECK(Graphics_IsAreaVisible(&a));CHECK(pool[i].font!=NULL);
    }
    CHECK(d->value==0 && d->speed==0 && d->direction==0 && d->cycles==0);
    GraphicsArcSweep_Process(2000);CHECK(d->value==5000 && d->speed==80 && d->direction==0);
    GraphicsArcSweep_Process(4000);CHECK(d->value==10000 && d->speed==160 && d->direction==1);
    GraphicsArcSweep_Process(6000);CHECK(d->value==5000 && d->speed==80 && d->direction==1);
    GraphicsArcSweep_Process(8000);CHECK(d->value==0 && d->speed==0 && d->direction==0 && d->cycles==1);

    /* 1km/h로 arc를 양자화하지 않았는지 실제 setter 인자 변화를 검사한다. */
    CHECK(GraphicsArcSweep_Init(&parent,240,240,220));CHECK(live_count==EXPECTED_OBJECTS && d->cycles==0);
    GraphicsArcSweep_Process(1000);uint32_t old_value=d->value,old_speed=d->speed;
    uint32_t old_arc_calls=arc_calls,old_text_calls=text_calls;
    GraphicsArcSweep_Process(1001);
    CHECK(d->value>old_value && d->speed==old_speed && arc_calls==old_arc_calls+1);
    CHECK(text_calls==old_text_calls);
    CHECK((uint32_t)last_arc->value==d->value && last_arc->max==10000);
    old_arc_calls=arc_calls;old_text_calls=text_calls;
    GraphicsArcSweep_Process(1001);CHECK(arc_calls==old_arc_calls && text_calls==old_text_calls);

    /* 5ms/불규칙 service에서도 경과 시간으로 정해진 곡선은 단조이며 경계가 정확하다. */
    Reset();CHECK(GraphicsArcSweep_Init(&parent,240,240,220));uint32_t previous=0;
    for(uint32_t ms=5;ms<=4000;ms+=5){
        GraphicsArcSweep_Process(ms);CHECK(d->value>=previous && d->value<=10000);
        previous=d->value;
    }
    CHECK(d->value==10000);
    for(uint32_t ms=4005;ms<=8000;ms+=5){
        GraphicsArcSweep_Process(ms);CHECK(d->value<=previous);previous=d->value;
    }
    CHECK(d->value==0 && d->cycles==1);
    GraphicsArcSweep_Process(16000);CHECK(d->value==0 && d->cycles==2);

    /* 유효한 실제 입력만 HUD에 쓰고, 1초 미만 호출은 새 문자열을 만들지 않는다. */
    Reset();CHECK(GraphicsArcSweep_Init(&parent,240,240,220));
    GraphicsArcSweep_Process(1000);CHECK(Equal(last_performance->text,"FPS -- CPU --"));
    CHECK(d->hud_updates==1);
    performance.valid=1;performance.fps_tenths=300;performance.cpu_tenths=273;
    GraphicsArcSweep_Process(1999);CHECK(d->hud_updates==1);
    GraphicsArcSweep_Process(2000);CHECK(d->hud_updates==2);
    CHECK(Equal(last_performance->text,"30.0 FPS CPU 27.3%"));

    /* uint32 누적 ms가 wrap해도+32ms로 해석해야 한다. */
    Reset();CHECK(GraphicsArcSweep_Init(&parent,240,240,220));
    GraphicsArcSweep_Process(UINT32_MAX-15U);
    uint32_t before_phase=d->phase_ms,before_cycles=d->cycles;
    GraphicsArcSweep_Process(16U);
    CHECK(d->phase_ms==(before_phase+32U)%8000U);
    CHECK(d->cycles==before_cycles+(before_phase+32U)/8000U);

    /* 작은 반경에서 text 경계와 비례 stroke를 확인한다. */
    Reset();CHECK(GraphicsArcSweep_Init(&parent,240,240,128));
    CHECK(last_arc->width[LV_PART_MAIN]==11);
    GraphicsArcSweep_Destroy();CHECK(!d->initialized && live_count==0 && invalid_calls==0);
    old_arc_calls=arc_calls;old_text_calls=text_calls;uint32_t before_process=d->process_count;
    GraphicsArcSweep_Process(5000);GraphicsArcSweep_Destroy();
    CHECK(arc_calls==old_arc_calls && text_calls==old_text_calls && d->process_count==before_process);
    CHECK(invalid_calls==0);

    /* parent 외부 삭제도 DELETE callback으로 안전하게 종료돼야 한다. */
    Reset();CHECK(GraphicsArcSweep_Init(&parent,240,240,220));
    lv_obj_delete(&parent);CHECK(!d->initialized && live_count==0);
    GraphicsArcSweep_Process(6000);GraphicsArcSweep_Destroy();CHECK(invalid_calls==0);
    /* lv_obj_clean(parent)처럼 부모를 살려 두고 자식만 지워도 동일하게 종료한다. */
    Reset();CHECK(GraphicsArcSweep_Init(&parent,240,240,220));
    lv_obj_delete(parent.child);CHECK(parent.live && !parent.child && !d->initialized);
    GraphicsArcSweep_Process(3000);CHECK(live_count==0 && invalid_calls==0);
    CHECK(GraphicsArcSweep_Init(&parent,240,240,220));CHECK(d->initialized && live_count==EXPECTED_OBJECTS);
    GraphicsArcSweep_Destroy();CHECK(live_count==0 && invalid_calls==0);
#if NOODOE_INTEGRATED
    Reset();external_hud_fail=1;
    CHECK(!GraphicsArcSweep_Init(&parent,240,240,220));
    CHECK(external_hud_init==1 && !d->initialized && !live_count && !parent.child && !invalid_calls);
#endif
    return 0;
}
uint32_t get_assertions(void){return assertions;}
