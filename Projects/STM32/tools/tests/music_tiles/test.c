#include "MusicTiles.h"
#include <string.h>
static uint32_t assertions;
#define T(x) do{++assertions;if(!(x))return __LINE__;}while(0)
uint32_t get_assertions(void){return assertions;}
static MusicTiles m;
static uint8_t packet[MUSIC_TILE_BYTES],pixels[304*72/2+8];
static void W(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=v>>(8*i);}
static void Packet(uint32_t i){
 W(packet,m.key);W(packet+4,m.view);W(packet+8,i);W(packet+12,1);
 for(unsigned y=0;y<36;y++)for(unsigned x=0;x<384;x+=2){unsigned n=(i*384+x+y)%15+1;
  packet[16+y*192+x/2]=(n<<4)|((i*384+x+1+y)%15+1);}
 memset(packet+16+MUSIC_TITLE_BYTES,0x5a,MUSIC_ARTIST_BYTES);
}
uint32_t test_windows(void){
 memset(&m,0,sizeof(m));T(MusicTiles_Track(&m,42,1000));MusicTiles_Visible(&m,42,0);
 Packet(0);T(!MusicTiles_Accept(&m,packet,sizeof(packet)));Packet(1);T(!MusicTiles_Accept(&m,packet,sizeof(packet)));
 for(unsigned offset=0;offset<=384;offset++){
  m.offset=offset;if(offset==384){Packet(2);T(!MusicTiles_Accept(&m,packet,sizeof(packet)));}
  uint32_t rev=0;memset(pixels,0xa5,sizeof(pixels));T(MusicTiles_Copy(&m,42,pixels,&rev));T(rev==m.revision);
  for(unsigned y=0;y<36;y++)for(unsigned x=0;x<304;x++){
   unsigned p=pixels[y*152+x/2];T(((x&1)?p&15:p>>4)==(offset+x+y)%15+1);}
  for(unsigned i=36*152;i<64*152;i++)T(pixels[i]==0x5a);
  for(unsigned i=64*152;i<72*152;i++)T(pixels[i]==0);
  for(unsigned i=72*152;i<sizeof(pixels);i++)T(pixels[i]==0xa5);
 }
 return 0;
}
uint32_t test_scroll_and_generations(void){
 memset(&m,0,sizeof(m));T(!MusicTiles_Track(&m,1,303));T(!MusicTiles_Track(&m,1,32769));T(MusicTiles_Track(&m,1,700));
 MusicTiles_Visible(&m,1,0);Packet(0);T(!MusicTiles_Accept(&m,packet,sizeof(packet)));
 for(unsigned t=100;t<=1000;t+=100)MusicTiles_Visible(&m,1,t);
 T(m.offset==0&&m.phase==1);
 MusicTiles_Visible(&m,1,1100);T(m.offset==3);
 for(unsigned t=1200;t<=10000;t+=100)MusicTiles_Visible(&m,1,t);
 T(m.offset<=80&&m.offset>=78);
 uint32_t old=m.offset;Packet(1);T(!MusicTiles_Accept(&m,packet,sizeof(packet)));
 MusicTiles_Visible(&m,1,10100);T(m.offset==old+3);
 for(unsigned t=10200;t<22000;t+=100)MusicTiles_Visible(&m,1,t);
 T(m.offset<=396);
 uint32_t view=m.view;Packet(0);MusicTiles_Visible(&m,0,22000);T(m.view!=view);T(MusicTiles_Accept(&m,packet,sizeof(packet))==9);
 MusicTiles_Visible(&m,1,22100);T(m.offset==0&&m.valid==0);T(MusicTiles_Accept(&m,packet,sizeof(packet))==9);
 Packet(0);T(!MusicTiles_Accept(&m,packet,sizeof(packet)));packet[16]^=1;T(MusicTiles_Accept(&m,packet,sizeof(packet))==7);
 T(!MusicTiles_Track(&m,1,701));T(MusicTiles_Track(&m,2,304));T(!m.active);MusicTiles_Visible(&m,1,22200);T(!m.active);
 MusicTiles_Visible(&m,2,22300);Packet(0);T(!MusicTiles_Accept(&m,packet,sizeof(packet)));
 for(unsigned t=22400;t<25000;t+=100)MusicTiles_Visible(&m,2,t);
 T(m.offset==0);
 m.last_ms=0xfffffff0U;MusicTiles_Visible(&m,2,17);T(m.offset==0);
 return 0;
}
