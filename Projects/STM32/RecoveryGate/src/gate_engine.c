#include "gate_engine.h"
#include <string.h>
static uint32_t U(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void Fail(GateEngine *s,uint32_t error){s->state=GATE_ENGINE_FAILED;s->error=error;}
static uint32_t Vector(const uint8_t *p,const GateImageInfo *image)
{uint32_t msp=U(p),pc=U(p+4);return msp>0x20007000U&&msp<=GATE_MAILBOX_ADDRESS&&!(msp&7)&&
 (pc&1)&&(pc&~1U)>=GATE_PRODUCT_BASE&&(pc&~1U)<GATE_PRODUCT_BASE+GATE_PRODUCT_BYTES&&
 !memcmp(p+0x200,image->requirement,44)&&U(p+0x230)==0x3250554EU&&U(p+0x234)==2&&U(p+0x238)==(image->kind==GATE_IMAGE_DIAGNOSTIC?3U:2U)&&U(p+0x23c)==GATE_PRODUCT_BYTES;}
uint32_t GateEngine_Begin(GateEngine *s,const GateEngineIO *io,const GateJournalRecord *r,const GateImageInfo *i,uint32_t slot,uint32_t install)
{if(!s||!io||!r||!i||slot>1||!io->source||!io->read_flash||!io->erase||!io->program||!io->journal)return 0;
 memset(s,0,sizeof(*s));s->io=*io;s->record=*r;s->image=*i;s->slot=slot;s->install=!!install;
 if(i->kind>GATE_IMAGE_DIAGNOSTIC||!!(r->flags&GATE_F_DIAGNOSTIC)!=(i->kind==GATE_IMAGE_DIAGNOSTIC)){Fail(s,GATE_E_ARGUMENT);return 0;}
 if((install&&r->candidate!=slot)||(!install&&r->active!=slot)||memcmp(r->uid,i->uid,12)||
 (install?memcmp(r->candidate_sha,i->sha256,32)||r->candidate_generation!=i->generation:
 memcmp(r->active_sha,i->sha256,32)||r->active_generation!=i->generation)){Fail(s,GATE_E_ARGUMENT);return 0;}
 s->state=install?GATE_ENGINE_SOURCE:GATE_ENGINE_VERIFY;UpdateSha256_Init(&s->sha);return 1;}
/* One bounded IO operation per call. A durable COPYING record precedes S5
 * erase, and a durable READY record follows an independent internal readback.
 * Restart from any interrupted step rehashes the committed immutable source
 * before repeating only S5-S7; S4 and original BL metadata are never touched. */
void GateEngine_Process(GateEngine *s)
{if(!s)return;uint8_t digest[32];uint32_t error;
 switch(s->state){
 case GATE_ENGINE_SOURCE:case GATE_ENGINE_VERIFY:
  error=s->state==GATE_ENGINE_SOURCE?s->io.source(s->io.context,s->slot,s->offset,s->buffer,4096):s->io.read_flash(s->io.context,s->offset,s->buffer,4096);
  if(error){Fail(s,GATE_E_SOURCE);break;}
  if(!s->offset&&!Vector(s->buffer,&s->image)){Fail(s,GATE_E_VECTOR);break;}
  UpdateSha256_Feed(&s->sha,s->buffer,4096);s->offset+=4096;
  if(s->offset==GATE_PRODUCT_BYTES){UpdateSha256_Final(&s->sha,digest);if(memcmp(digest,s->image.sha256,32)){Fail(s,GATE_E_HASH);break;}
   if(s->state==GATE_ENGINE_SOURCE)s->state=GATE_ENGINE_INTENT;
   else s->state=s->install?GATE_ENGINE_FINISH:GATE_ENGINE_READY;}
  break;
 case GATE_ENGINE_INTENT:
  if(s->record.state!=GATE_J_COPYING){s->record.state=GATE_J_COPYING;++s->record.sequence;if(s->io.journal(s->io.context,&s->record)){Fail(s,GATE_E_JOURNAL);break;}}
  s->sector=5;s->state=GATE_ENGINE_ERASE;break;
 case GATE_ENGINE_ERASE:
  if(s->io.erase(s->io.context,s->sector)){Fail(s,GATE_E_FLASH);break;}
  if(++s->sector==8){s->offset=0;s->state=GATE_ENGINE_COPY;}break;
 case GATE_ENGINE_COPY:
  if(s->io.source(s->io.context,s->slot,s->offset,s->buffer,4096)){Fail(s,GATE_E_SOURCE);break;}
  if(s->io.program(s->io.context,GATE_PRODUCT_BASE+s->offset,s->buffer,4096)){Fail(s,GATE_E_FLASH);break;}
  s->offset+=4096;if(s->offset==GATE_PRODUCT_BYTES){s->offset=0;s->state=GATE_ENGINE_VERIFY;UpdateSha256_Init(&s->sha);}break;
 case GATE_ENGINE_FINISH:
  s->record.state=GATE_J_READY;s->record.active=s->slot;s->record.active_generation=s->image.generation;
  memcpy(s->record.active_sha,s->image.sha256,32);s->record.candidate=GATE_NO_SLOT;s->record.candidate_generation=0;memset(s->record.candidate_sha,0,32);
  s->record.attempts=0;++s->record.sequence;if(s->io.journal(s->io.context,&s->record)){Fail(s,GATE_E_JOURNAL);break;}s->state=GATE_ENGINE_READY;break;
 default:break;
 }}
