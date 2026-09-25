#if NOODOE_PRODUCT
#include "PhoneVisual.h"
#include "MusicTiles.h"
#include "Photo_Jpeg.h"
#include "Photo_Store.h"
#include "BSP_RAM.h"
#include "noodoe_crc32.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#define INPUT_MAX 131072U
#define ART_BYTES (480U*480U*2U)
typedef struct {
 MusicTiles music;
 uint8_t input[INPUT_MAX],mask[PHONE_VISUAL_MASK_BYTES],work[4096],art[2][ART_BYTES];
 uint8_t panels[14][PHONE_PANEL_BYTES];uint8_t panel_height[14];uint32_t panel_key[14],panel_rev[14],panel_crc[14],panel_next,panel_keep[13];
 JDEC decoder;uint32_t expected_crc,crc_at,crc,offset,x,y,rst,rsc,store_id,mask_key,mask_revision,mask_crc;
 uint32_t art_key[2],art_revision[2],width[2],height[2],leases[2],front,decode_bank,session;
} Visual;
static Visual *v;
volatile PhoneVisualStatus g_phone_visual;
/* One completion boundary for CRC, decode and storage errors. Keeping it out
 * of callers avoids duplicating the same publication barrier in every branch. */
__attribute__((noinline)) static void Done(uint32_t result){g_phone_visual.result=result;__atomic_store_n(&g_phone_visual.state,result?PV_FAILED:PV_READY,__ATOMIC_RELEASE);}
void PhoneVisual_Session(uint32_t epoch)
{
 if(!v||v->session==epoch)return;
 v->session=epoch;memset(v->panel_key,0,sizeof(v->panel_key));v->mask_key=0;v->art_key[0]=v->art_key[1]=0;MusicTiles_Track(&v->music,0,0);
 /* Running physical photo writes drain in StorageTask; their accepted input
  * is not reused by a new connection. RAM work is discarded on completion. */
 if(g_phone_visual.state!=PV_PROCESSING)g_phone_visual.state=PV_IDLE;
}
uint32_t PhoneVisual_Begin(uint32_t epoch,uint32_t kind,uint32_t key,uint32_t bytes,uint32_t crc)
{
 if(!v)return CFW_BUSY;
 if(!epoch||epoch!=v->session)return CFW_EXPIRED;
 if(kind<PV_MUSIC||kind>PV_PACKED_MUSIC_TILE||!key||!bytes||bytes>INPUT_MAX||(kind==PV_MUSIC&&bytes!=PHONE_VISUAL_MASK_BYTES)||(kind==PV_PANEL&&bytes!=PHONE_PANEL_LEGACY_BYTES)||(kind==PV_MUSIC_TILE&&bytes!=MUSIC_TILE_BYTES))return CFW_ARGUMENT;
 /* A rendered key is immutable within the connection. Replacing the same
  * key in place could overwrite a texture referenced by the scanned frame. */
 if(kind==PV_MUSIC&&v->mask_key==key&&v->mask_crc!=crc)return CFW_ARGUMENT;
 if(kind==PV_PANEL||kind==PV_PACKED_PANEL)for(uint32_t i=0;i<14;i++)if(v->panel_key[i]==key&&v->panel_crc[i]!=crc)return CFW_ARGUMENT;
 if(g_phone_visual.state==PV_PROCESSING)return CFW_BUSY;
 /* BEGIN may replace an incomplete RAM-only upload, never a NOR operation. */
 v->expected_crc=crc;v->crc_at=0;v->crc=0xffffffffU;v->offset=0;v->store_id=0;
 g_phone_visual=(PhoneVisualStatus){PV_RECEIVING,kind,key,0,bytes,0,epoch,g_phone_visual.revision};return 0;
}
uint32_t PhoneVisual_Data(uint32_t epoch,uint32_t key,uint32_t offset,const void *data,uint32_t bytes)
{
 if(!v||g_phone_visual.state!=PV_RECEIVING||epoch!=v->session||epoch!=g_phone_visual.epoch||key!=g_phone_visual.key)return CFW_EXPIRED;
 if(!data||!bytes||bytes>960||offset!=g_phone_visual.received||offset>g_phone_visual.bytes||bytes>g_phone_visual.bytes-offset)return CFW_ARGUMENT;
 memcpy(v->input+offset,data,bytes);g_phone_visual.received+=bytes;return 0;
}
uint32_t PhoneVisual_Finish(uint32_t epoch,uint32_t key)
{
 if(!v||g_phone_visual.state!=PV_RECEIVING||epoch!=v->session||epoch!=g_phone_visual.epoch||key!=g_phone_visual.key)return CFW_EXPIRED;
 if(g_phone_visual.received!=g_phone_visual.bytes)return CFW_ARGUMENT;
 /* CRC and decoding belong to the worker, not the Bluetooth parser. */
 v->offset=0;__atomic_store_n(&g_phone_visual.state,PV_PROCESSING,__ATOMIC_RELEASE);return 0;
}
static size_t Input(JDEC *d,uint8_t *dst,size_t n)
{(void)d;uint32_t left=g_phone_visual.bytes-v->offset;if(n>left)n=left;if(dst)memcpy(dst,v->input+v->offset,n);v->offset+=n;return n;}
static int Output(JDEC *d,void *tile,JRECT *r)
{
 if(r->right>=d->width||r->bottom>=d->height||r->left>r->right||r->top>r->bottom)return 0;
 uint8_t *dest=v->art[v->decode_bank];const uint8_t *src=tile;uint32_t n=(r->right-r->left+1U)*2U;
 for(uint32_t y=r->top;y<=r->bottom;y++){PhotoJpeg_CopyRGB565(dest+(y*d->width+r->left)*2U,src,n/2U);src+=n;}return 1;
}
/* Shared bounded A4 byte-run decoder. Scratch never aliases compressed input;
 * exact decoded size is mandatory before publishing a tile or panel. */
__attribute__((noinline)) static uint32_t Unpack(const uint8_t *input,uint32_t n,uint8_t *pixels,uint32_t limit)
{
 uint32_t at=0,out=0;
 while(at<n){uint32_t control=input[at++],count=(control&127U)+1U;
  if(count>limit-out||((control&128U)?at>=n:count>n-at))return 0;
  if(control&128U)memset(pixels+out,input[at++],count);
  else {memcpy(pixels+out,input+at,count);at+=count;}out+=count;
 }
 return out==limit;
}
__attribute__((noinline)) void PhoneVisual_Process(uint32_t now)
{
 (void)now;
 if(!v){Visual *ready=BSP_RAM_AllocateNamed(BSP_RAM_PHONE_VISUAL,sizeof(*v));if(!ready)return;
  /* Publish only after initialization; BT may preempt this storage worker. */
  memset(ready,0,sizeof(*ready));__atomic_store_n(&v,ready,__ATOMIC_RELEASE);return;}
 if(__atomic_load_n(&g_phone_visual.state,__ATOMIC_ACQUIRE)!=PV_PROCESSING)return;
 if(v->store_id){uint32_t r=PhotoStore_GetResult(v->store_id);if(r!=CFW_PENDING){v->store_id=0;Done(r);}return;}
 if(g_phone_visual.epoch!=v->session){Done(CFW_EXPIRED);return;}
 if(v->crc_at<g_phone_visual.bytes){uint32_t n=g_phone_visual.bytes-v->crc_at;if(n>4096)n=4096;
  v->crc=Noodoe_Crc32Feed(v->crc,v->input+v->crc_at,n);v->crc_at+=n;return;}
 if(!v->offset){
  if((v->crc^0xffffffffU)!=v->expected_crc){Done(CFW_CORRUPT);return;}
  if(g_phone_visual.kind==PV_MUSIC_TILE||g_phone_visual.kind==PV_PACKED_MUSIC_TILE){
   uint8_t *tile=v->input;
   if(g_phone_visual.kind==PV_PACKED_MUSIC_TILE){
    uint32_t n=g_phone_visual.bytes;tile=v->input+65536;
    if(n<=16||n>32768||!Unpack(v->input+16,n-16,tile+16,MUSIC_TILE_BYTES-16)){Done(CFW_CORRUPT);return;}
    memcpy(tile,v->input,16);
   }
   uint32_t irq=__get_PRIMASK();__disable_irq();
   uint32_t r=g_phone_visual.epoch==v->session?MusicTiles_Accept(&v->music,tile,MUSIC_TILE_BYTES):CFW_EXPIRED;
   __set_PRIMASK(irq);Done(r);return;
  }
  if(g_phone_visual.kind==PV_MUSIC){
   if(v->mask_key==g_phone_visual.key){Done(0);return;}
   /* Copy under a bounded IRQ guard: graphics never sees half of a mask. */
   uint32_t irq=__get_PRIMASK();__disable_irq();if(g_phone_visual.epoch!=v->session){__set_PRIMASK(irq);Done(CFW_EXPIRED);return;}memcpy(v->mask,v->input,PHONE_VISUAL_MASK_BYTES);
   v->mask_key=g_phone_visual.key;v->mask_crc=v->expected_crc;v->mask_revision=++g_phone_visual.revision;__set_PRIMASK(irq);Done(0);return;
  }
  if(g_phone_visual.kind==PV_PANEL||g_phone_visual.kind==PV_PACKED_PANEL){
   uint8_t *pixels=v->input;uint32_t height=128;
   if(g_phone_visual.kind==PV_PACKED_PANEL){
    height=v->input[1]==5?145:v->input[1]==4?162:v->input[1]==3?144:v->input[1]==2?192:128;uint32_t n=g_phone_visual.bytes;
    pixels=v->input+65536;
    if(n<2||n>32768||v->input[0]!=(v->input[1]>=3?144:128)||v->input[1]<1||v->input[1]>5||
       !Unpack(v->input+2,n-2,pixels,v->input[1]>=3?PHONE_PANEL_BYTES:PHONE_PANEL_LEGACY_BYTES)){Done(CFW_CORRUPT);return;}
   }
   for(uint32_t i=0;i<14;i++)if(v->panel_key[i]==g_phone_visual.key){Done(0);return;}
   uint32_t b=0,irq=__get_PRIMASK();__disable_irq();
   /* Keep notification/header/reply/current call keys. The fourteenth slot
    * stages the next revision without evicting a still-selectable message. */
   for(uint32_t n=0;n<14;n++){b=v->panel_next++%14U;uint32_t pinned=0;
    for(uint32_t j=0;j<13;j++)if(v->panel_key[b]&&v->panel_keep[j]==v->panel_key[b])pinned=1;
    if(!pinned)break;}
   if(g_phone_visual.epoch!=v->session){__set_PRIMASK(irq);Done(CFW_EXPIRED);return;}
   memset(v->panels[b],0,PHONE_PANEL_BYTES);memcpy(v->panels[b],pixels,height==144||height==145||height==162?PHONE_PANEL_BYTES:PHONE_PANEL_LEGACY_BYTES);v->panel_height[b]=height;v->panel_key[b]=g_phone_visual.key;
   v->panel_crc[b]=v->expected_crc;v->panel_rev[b]=++g_phone_visual.revision;
   __set_PRIMASK(irq);Done(0);return;
  }
  if(g_phone_visual.kind>=PV_PHOTO0){uint32_t r=PhotoStore_RequestReplace(g_phone_visual.kind-PV_PHOTO0,v->input,g_phone_visual.bytes,&v->store_id);if(r==CFW_BUSY||r==CFW_PENDING)return;if(r)Done(r);return;}
  uint32_t bank=1U-v->front;if(v->leases[bank])return;
  v->decode_bank=bank;
  JRESULT r=jd_prepare(&v->decoder,Input,v->work,sizeof(v->work),v);
  if(r||!v->decoder.width||!v->decoder.height||v->decoder.width>480||v->decoder.height>480){Done(CFW_CORRUPT);return;}
  v->x=v->y=v->rst=v->rsc=0;v->decoder.scale=0;memset(v->decoder.dcv,0,sizeof(v->decoder.dcv));
 }
 JDEC *d=&v->decoder;
 for(uint32_t i=0;i<8;i++){
  JRESULT r=JDR_OK;if(d->nrst&&v->rst++==d->nrst){r=jd_restart(d,(uint16_t)v->rsc++);v->rst=1;}
  if(!r)r=jd_mcu_load(d);
  if(!r)r=jd_mcu_output(d,Output,v->x,v->y);
  if(r){Done(CFW_CORRUPT);return;}
  v->x+=d->msx*8U;if(v->x>=d->width){v->x=0;v->y+=d->msy*8U;}
  if(v->y>=d->height){uint32_t b=v->decode_bank,irq=__get_PRIMASK();__disable_irq();
   if(g_phone_visual.epoch!=v->session){__set_PRIMASK(irq);Done(CFW_EXPIRED);return;}
   v->width[b]=d->width;v->height[b]=d->height;v->art_key[b]=g_phone_visual.key;
   v->art_revision[b]=++g_phone_visual.revision;v->front=b;__set_PRIMASK(irq);Done(0);return;}
 }
}
uint32_t PhoneVisual_CopyMask(uint32_t key,void *dst,uint32_t *revision)
{
 if(!v||!key||!dst||!revision)return 0;
 uint32_t irq=__get_PRIMASK();__disable_irq();uint32_t ok=v->mask_key==key;
 if(ok&&*revision!=v->mask_revision){memcpy(dst,v->mask,PHONE_VISUAL_MASK_BYTES);*revision=v->mask_revision;}
 __set_PRIMASK(irq);return ok;
}
/* The storage worker publishes complete CRC-checked tiles; UI alone advances
 * the viewport. Metadata polling never changes scroll position or timing. */
uint32_t PhoneVisual_MusicTrack(uint32_t key,uint32_t width,uint32_t out[7])
{
 if(!v)return CFW_BUSY;
 uint32_t irq=__get_PRIMASK();__disable_irq();uint32_t ok=MusicTiles_Track(&v->music,key,width);
 if(ok)MusicTiles_Status(&v->music,out);
 __set_PRIMASK(irq);return ok?0:CFW_ARGUMENT;
}
void PhoneVisual_MusicVisible(uint32_t key,uint32_t now)
{if(!v)return;uint32_t irq=__get_PRIMASK();__disable_irq();MusicTiles_Visible(&v->music,key,now);__set_PRIMASK(irq);}
uint32_t PhoneVisual_MusicRevision(uint32_t key)
{if(!v||!key||v->music.key!=key||!v->music.active)return 0;return v->music.revision;}
uint32_t PhoneVisual_CopyMusic(uint32_t key,void *dst,uint32_t *revision)
{if(!v||!dst||!revision)return 0;uint32_t irq=__get_PRIMASK();__disable_irq();uint32_t ok=MusicTiles_Copy(&v->music,key,dst,revision);__set_PRIMASK(irq);return ok;}
/* Fourteen fixed SDRAM snapshots: ten notifications, header/reply, one
 * selected call panel and one incoming revision. No growth across calls. */
void PhoneVisual_KeepPanels(const uint32_t *keys,uint32_t count)
{if(!v)return;if(count>13)count=13;uint32_t irq=__get_PRIMASK();__disable_irq();memset(v->panel_keep,0,sizeof(v->panel_keep));if(keys)memcpy(v->panel_keep,keys,count*4);__set_PRIMASK(irq);}
uint32_t PhoneVisual_CopyPanel(uint32_t key,void *dst,uint32_t *revision)
{
 if(!v||!key||!revision)return 0;
 uint32_t irq=__get_PRIMASK(),ok=0;__disable_irq();
 /* Keep-list layout is header/replies/call/ten messages. Composite the
  * newest completed status strip only into a notification's destination copy;
  * cached bodies/outgoing GPU pages stay immutable. Battery changes therefore
  * need one small status upload, not ten repeated CJK notification transfers. */
 uint32_t header=14;
 for(uint32_t k=3;k<13;k++)if(v->panel_keep[k]==key){
  for(uint32_t h=0;h<14;h++)if(v->panel_key[h]&&v->panel_key[h]==v->panel_keep[0]){header=h;break;}
  break;
 }
 for(uint32_t i=0;i<14;i++)if(v->panel_key[i]==key){
  uint32_t version=v->panel_rev[i];
  if(header<14&&(int32_t)(v->panel_rev[header]-version)>0)version=v->panel_rev[header];
  if(dst&&*revision!=version){
   memcpy(dst,v->panels[i],PHONE_PANEL_BYTES);
   if(header<14&&v->panel_height[header]==(v->panel_height[i]==145?145:v->panel_height[i]==144?144:128))memcpy(dst,v->panels[header],(v->panel_height[i]==145?20U:v->panel_height[i]==144?30U:24U)*144U);
  }*revision=version;ok=v->panel_height[i];break;
 }
 __set_PRIMASK(irq);return ok;
}
uint32_t PhoneVisual_AcquireArt(uint32_t key,PhoneVisualImage *out)
{
 if(!v||!key||!out)return 0;
 uint32_t irq=__get_PRIMASK();__disable_irq();uint32_t b=v->front,ok=v->art_key[b]==key;
 if(ok){v->leases[b]++;*out=(PhoneVisualImage){v->art[b],v->width[b],v->height[b],v->width[b]*v->height[b]*2U,v->art_revision[b],key,b};}
 __set_PRIMASK(irq);return ok;
}
void PhoneVisual_ReleaseArt(uint32_t bank)
{if(v&&bank<2){uint32_t irq=__get_PRIMASK();__disable_irq();if(v->leases[bank])v->leases[bank]--;__set_PRIMASK(irq);}}
#endif
