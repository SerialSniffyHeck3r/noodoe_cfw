#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "RAM codec requires little-endian target"
#endif
#include "MusicTiles.h"
#include <string.h>
static uint32_t U(const uint8_t *p){ uint32_t v;memcpy(&v,p,4);return v;}
static uint32_t Next(uint32_t n){return n==UINT32_MAX?1:n+1;}
static uint32_t Have(const MusicTiles *m,uint32_t i)
{return (m->valid&(1U<<(i&1U)))&&m->index[i&1U]==i;}
static uint32_t Ready(const MusicTiles *m,uint32_t offset)
{return m->active&&m->artist_valid&&Have(m,offset/MUSIC_TILE_WIDTH)&&Have(m,(offset+MUSIC_VIEW_WIDTH-1)/MUSIC_TILE_WIDTH);}
/* A new identity invalidates bytes without clearing18KiB inside an IRQ guard.
 * view is also changed on page exit: late transfers cannot revive old text. */
uint32_t MusicTiles_Track(MusicTiles *m,uint32_t key,uint32_t width)
{
 if((!key&&width)||(key&&(width<MUSIC_VIEW_WIDTH||width>32768)))return 0;
 if(m->key==key)return m->width==width;
 m->key=key;m->width=width;m->active=m->valid=m->artist_valid=0;
 m->view=Next(m->view);m->revision=Next(m->revision);return 1;
}
/* Time advances only with a complete window. Missing next tiles stop the
 * scroll at the last complete pixel; they never accumulate catch-up motion. */
void MusicTiles_Visible(MusicTiles *m,uint32_t key,uint32_t now)
{
 uint32_t active=key&&key==m->key;
 if(active!=m->active){m->active=active;m->view=Next(m->view);m->valid=m->artist_valid=0;
  m->offset=m->phase=m->wait_ms=m->fraction=0;m->revision=Next(m->revision);m->last_ms=now;}
 uint32_t dt=now-m->last_ms;m->last_ms=now;if(dt>100)dt=100;
 if(!active||!Ready(m,m->offset)||m->width<=MUSIC_VIEW_WIDTH)return;
 if(m->phase!=1){
  m->wait_ms+=dt;if(m->wait_ms<1000)return;m->wait_ms=0;
  if(m->phase==2){m->offset=0;m->phase=0;m->fraction=0;m->revision=Next(m->revision);return;}
  m->phase=1;return;
 }
 uint32_t next=m->offset+(m->fraction+dt*30U)/1000U,limit=m->width-MUSIC_VIEW_WIDTH;
 m->fraction=(m->fraction+dt*30U)%1000U;if(next>limit)next=limit;
 if(next!=m->offset&&Ready(m,next)){m->offset=next;m->revision=Next(m->revision);}
 if(m->offset==limit){m->phase=2;m->wait_ms=0;}
}
/* Payload after NDCP result: schema, key, page generation, active, two wanted
 * tile indices and presence bits. Prefetch one ahead of the visible origin. */
void MusicTiles_Status(const MusicTiles *m,uint32_t out[7])
{
 uint32_t a=m->offset/MUSIC_TILE_WIDTH,b=a+1,last=m->width?(m->width-1)/MUSIC_TILE_WIDTH:0;
 if(b>last)b=last;
 out[0]=1;out[1]=m->key;out[2]=m->view;out[3]=m->active;out[4]=a;out[5]=b;
 out[6]=(Have(m,a)?1U:0U)|(Have(m,b)?2U:0U);
}
/* CRC is checked by the existing binary worker before this function. Only
 * the two currently requested tiles may replace storage. Artist is immutable
 * within a track/page generation, as are already accepted title bytes. */
uint32_t MusicTiles_Accept(MusicTiles *m,const uint8_t *p,uint32_t bytes)
{
 if(bytes!=MUSIC_TILE_BYTES||U(p+12)!=1)return 7;
 uint32_t status[7];MusicTiles_Status(m,status);uint32_t i=U(p+8),b=i&1U;
 if(!m->active||U(p)!=m->key||U(p+4)!=m->view||(i!=status[4]&&i!=status[5]))return 9;
 if(m->artist_valid&&memcmp(m->artist,p+16+MUSIC_TITLE_BYTES,MUSIC_ARTIST_BYTES))return 7;
 if(Have(m,i))return memcmp(m->title[b],p+16,MUSIC_TITLE_BYTES)?7:0;
 memcpy(m->title[b],p+16,MUSIC_TITLE_BYTES);memcpy(m->artist,p+16+MUSIC_TITLE_BYTES,MUSIC_ARTIST_BYTES);
 m->index[b]=i;m->valid|=1U<<b;m->artist_valid=1;m->revision=Next(m->revision);return 0;
}
/* Compose only the visible304x72 alpha mask, including odd-pixel offsets.
 * Graphics copies into its retired bank before changing an EVE texture. */
uint32_t MusicTiles_Copy(const MusicTiles *m,uint32_t key,uint8_t *out,uint32_t *revision)
{
 if(!key||key!=m->key||!Ready(m,m->offset))return 0;
 if(*revision==m->revision)return 1;
 memset(out,0,MUSIC_VIEW_WIDTH*MUSIC_VIEW_HEIGHT/2U);
 /* Work a byte at a time. Adjacent nibbles across a tile edge are composed
  * explicitly; aligned rows use at most two memcpy calls. This bounds the
  * UI publication lock without a divide/modulo for every individual pixel. */
 uint32_t bank=(m->offset/MUSIC_TILE_WIDTH)&1U;
 uint32_t start=(m->offset%MUSIC_TILE_WIDTH)/2U;
 for(uint32_t y=0;y<MUSIC_TITLE_HEIGHT;y++){
  const uint8_t *a=m->title[bank]+y*MUSIC_TILE_WIDTH/2U;
  const uint8_t *b=m->title[bank^1U]+y*MUSIC_TILE_WIDTH/2U;
  uint8_t *dst=out+y*MUSIC_VIEW_WIDTH/2U;
  if(!(m->offset&1U)){
   uint32_t n=MUSIC_TILE_WIDTH/2U-start;if(n>MUSIC_VIEW_WIDTH/2U)n=MUSIC_VIEW_WIDTH/2U;
   memcpy(dst,a+start,n);memcpy(dst+n,b,MUSIC_VIEW_WIDTH/2U-n);
  }else for(uint32_t x=0;x<MUSIC_VIEW_WIDTH/2U;x++){
   uint32_t i=start+x;
   uint8_t hi=i<MUSIC_TILE_WIDTH/2U?a[i]:b[i-MUSIC_TILE_WIDTH/2U];++i;
   uint8_t lo=i<MUSIC_TILE_WIDTH/2U?a[i]:b[i-MUSIC_TILE_WIDTH/2U];
   dst[x]=(uint8_t)((hi<<4)|(lo>>4));
  }
 }
 memcpy(out+MUSIC_VIEW_WIDTH*MUSIC_TITLE_HEIGHT/2U,m->artist,MUSIC_ARTIST_BYTES);
 *revision=m->revision;return 1;
}
