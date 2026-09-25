#ifndef NOODOE_INSTALL_SESSION_H
#define NOODOE_INSTALL_SESSION_H
#include <stdint.h>
/* Allocation-free policy shared by Bootstrap and Product. The storage owner
 * alone mutates it. Link state and physical IGN do not end an install lease.
 * Persisted boot journals, not this RAM presentation, authorize installation. */
enum { INSTALL_IDLE, INSTALL_CONNECT, INSTALL_CHECK, INSTALL_TRANSFER,
 INSTALL_CONFIRM, INSTALL_COMMIT, INSTALL_RESTART, INSTALL_HEALTH,
 INSTALL_DONE, INSTALL_DISCONNECTED, INSTALL_CANCELLING, INSTALL_CANCELLED,
 INSTALL_ERROR, INSTALL_UNKNOWN };
typedef struct {
 uint32_t id,sequence,state,started,now,changed,phase,file,files,position,total;
 uint32_t verified,error,connected,cancel_requested,released,pressed,press_ms;
} InstallSession;
static inline uint32_t InstallSession_Active(const InstallSession *s)
{return s&&s->state!=INSTALL_IDLE&&s->state!=INSTALL_DONE&&s->state!=INSTALL_CANCELLED;}
static inline void InstallSession_Begin(InstallSession *s,uint32_t now,uint32_t id)
{uint32_t seq=s->sequence+1;*s=(InstallSession){.id=id?id:seq,.sequence=seq,.state=INSTALL_CONNECT,.started=now,.now=now,.changed=now};}
/* Fresh release then 3 seconds, never a key held before entering the page. */
static inline void InstallSession_Button(InstallSession *s,uint32_t now,uint32_t down)
{
 if(!InstallSession_Active(s))return;
 if(!down){s->released=1;s->pressed=0;return;}
 if(!s->released||s->cancel_requested)return;
 if(!s->pressed){s->pressed=1;s->press_ms=now;}
 if(now-s->press_ms>=3000U)s->cancel_requested=1;
}
static inline void InstallSession_Observe(InstallSession *s,uint32_t now,uint32_t state,
 uint32_t phase,uint32_t file,uint32_t files,uint32_t position,uint32_t total,uint32_t verified,uint32_t error,uint32_t connected)
{
 if(!InstallSession_Active(s))return;
 s->now=now;s->connected=connected;
 if(s->cancel_requested&&state<INSTALL_COMMIT)state=INSTALL_CANCELLING;
 else if(!connected&&state<INSTALL_COMMIT)state=INSTALL_DISCONNECTED;
 if(s->state!=state||s->phase!=phase||s->file!=file||s->position!=position||s->verified!=verified||s->error!=error){s->changed=now;++s->sequence;}
 s->state=state;s->phase=phase;s->file=file;s->files=files;s->position=position;s->total=total;s->verified=verified;s->error=error;
}
static inline void InstallSession_End(InstallSession *s,uint32_t state)
{s->state=state;s->cancel_requested=0;++s->sequence;}
/* v1 progress, 20 little-endian words on Cortex-M4. Current bytes/verification
 * are device facts. Overall is stage-based, not a predicted time percentage. */
static inline void InstallSession_Snapshot(const InstallSession *s,uint32_t out[20])
{
 uint32_t stage=s->state==INSTALL_DONE?8:s->state==INSTALL_HEALTH?7:s->state==INSTALL_RESTART?6:
  s->state==INSTALL_COMMIT?5:s->state==INSTALL_CONFIRM?4:s->phase==100?4:s->files==1?3:s->files&&s->file<s->files?2:1;
 const uint32_t words[20]={0,1,s->id,s->sequence,s->state,stage,8,s->phase,s->file,s->files,
  s->position,s->total,s->verified,s->total?(s->position+4095)/4096:0,(s->total+4095)/4096,
  s->now-s->started,s->now-s->changed,s->state<INSTALL_COMMIT||s->state==INSTALL_DISCONNECTED||s->state==INSTALL_ERROR,
  s->error,s->connected};
 for(uint32_t i=0;i<20;++i)out[i]=words[i];
}
#endif
