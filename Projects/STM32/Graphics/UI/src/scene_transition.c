#include "Scene_Transition.h"
#include "Page_Transition.h"
/* Time normalization keeps the existing240ms public page easing intact. */
void SceneTransition_Step(SceneTransition *s,uint32_t now,ScenePose *p)
{
    if(!s||!p)return;
    uint32_t dt=now-s->start_ms;
    if(s->active){
        if(dt>=SCENE_TRANSITION_MS){s->active=0;s->value=s->target;}
        else{s->value=(uint32_t)((int32_t)s->from+((int32_t)s->target-(int32_t)s->from)*(int32_t)PageTransition_Ease(dt*PAGE_TRANSITION_MS/SCENE_TRANSITION_MS)/1024);}
    }
    uint32_t q=s->value;
    /* Clock leaves above the glass; the footer leaves BELOW. Reversing this
     * pose brings ODO up from below, preserving its children's fixed X/Y. */
    *p=(ScenePose){q,240U+20U*q/1024U,255U*(1024U-q)/1024U,
        q>800U?(q-800U)*255U/224U:0U,-(int32_t)(100U*q/1024U),(int32_t)(100U*q/1024U)};
}
void SceneTransition_Request(SceneTransition *s,uint32_t settings,uint32_t now)
{
    if(!s)return;
    ScenePose pose;SceneTransition_Step(s,now,&pose);
    uint32_t target=settings?1024U:0U;if(s->target==target)return;
    s->from=s->value;s->target=target;s->start_ms=now;s->active=s->from!=target;
}
