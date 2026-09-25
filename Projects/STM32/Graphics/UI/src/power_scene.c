#include "Power_Scene.h"
/* Seed from the last submitted pose, not a state-specific endpoint. A settings
 * exit can therefore become an IGN exit without snapping the clock/footer. */
void PowerScene_Seed(PowerScene *s,const ScenePose *p)
{
    uint32_t ring=p->progress>1024U?1024U:p->progress;
    uint32_t shell=p->footer_y<=0?0U:(uint32_t)p->footer_y*1024U/100U;
    if(shell>1024U)shell=1024U;
    s->ring=(SceneTransition){ring,ring,ring,0,0};
    s->shell=(SceneTransition){shell,shell,shell,0,0};
}
void PowerScene_Request(PowerScene *s,uint32_t ring,uint32_t shell,uint32_t now)
{SceneTransition_Request(&s->ring,ring,now);SceneTransition_Request(&s->shell,shell,now);}
/* Freeze the last displayed ring pose during the physical OFF delay, including
 * a half-finished ON animation. The shell remains independently reversible. */
void PowerScene_HoldRing(PowerScene *s)
{s->ring.from=s->ring.target=s->ring.value;s->ring.active=0;}
void PowerScene_Step(PowerScene *s,uint32_t now,ScenePose *out)
{
    ScenePose shell;SceneTransition_Step(&s->ring,now,out);
    SceneTransition_Step(&s->shell,now,&shell);out->clock_y=shell.clock_y;out->footer_y=shell.footer_y;
}
