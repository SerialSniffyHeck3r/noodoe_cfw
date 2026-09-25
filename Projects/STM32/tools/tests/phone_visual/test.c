#include "PhoneVisual.h"
#include "Photo_Store.h"
#include "BSP_RAM.h"
#include "noodoe_crc32.h"
#include <string.h>
#include "jpeg.inc"
#include "output/panel_fixture.h"
#include "output/music_fixture.h"
volatile uint32_t failure,checks;
static uint8_t arena[1800000],mask[PHONE_VISUAL_MASK_BYTES],copy[PHONE_VISUAL_MASK_BYTES];
static uint8_t panel[PHONE_PANEL_LEGACY_BYTES],panel_copy[PHONE_PANEL_BYTES];
static uint32_t allocations,photo_result,photo_accepts;
#define CHECK(x) do{checks++;if(!(x)){failure=__LINE__;return;}}while(0)
void *BSP_RAM_AllocateNamed(uint32_t owner,size_t n){allocations++;return owner==BSP_RAM_PHONE_VISUAL&&n<=sizeof(arena)?arena:0;}
uint32_t PhotoStore_RequestReplace(uint32_t slot,const void *p,uint32_t n,uint32_t *id){(void)p;if(slot>2||!n)return CFW_ARGUMENT;photo_accepts++;*id=7;photo_result=CFW_PENDING;return 0;}
uint32_t PhotoStore_GetResult(uint32_t id){return id==7?photo_result:CFW_EXPIRED;}
static uint32_t Start(uint32_t kind,uint32_t key,const uint8_t *p,uint32_t n,uint32_t corrupt){
 uint32_t r=PhoneVisual_Begin(1,kind,key,n,(Noodoe_Crc32Feed(0xffffffff,p,n)^0xffffffff)^corrupt);if(r)return r;
 for(uint32_t a=0;a<n;){uint32_t b=n-a;if(b>960)b=960;r=PhoneVisual_Data(1,key,a,p+a,b);if(r)return r;a+=b;}
 return PhoneVisual_Finish(1,key);
}
static void Pump(void){for(uint32_t i=0;i<1000&&g_phone_visual.state==PV_PROCESSING;i++)PhoneVisual_Process(i);}
void Test(void){
 PhoneVisual_Process(0);PhoneVisual_Session(1);memset(mask,0xA3,sizeof(mask));
 CHECK(allocations==1);CHECK(PhoneVisual_Begin(2,1,1,sizeof(mask),0)==CFW_EXPIRED);
 CHECK(PhoneVisual_Begin(1,1,1,sizeof(mask)-1,0)==CFW_ARGUMENT);
 CHECK(!PhoneVisual_Begin(1,1,1,sizeof(mask),0));CHECK(PhoneVisual_Finish(1,1)==CFW_ARGUMENT);
 CHECK(PhoneVisual_Data(1,1,1,mask,8)==CFW_ARGUMENT);
 CHECK(!Start(1,1,mask,sizeof(mask),1));Pump();CHECK(g_phone_visual.state==PV_FAILED&&g_phone_visual.result==CFW_CORRUPT);
 uint32_t rev=0;CHECK(!PhoneVisual_CopyMask(1,copy,&rev));
 CHECK(!Start(1,2,mask,sizeof(mask),0));CHECK(PhoneVisual_Begin(1,1,3,sizeof(mask),0)==CFW_BUSY);Pump();
 CHECK(g_phone_visual.state==PV_READY);CHECK(PhoneVisual_CopyMask(2,copy,&rev)&&rev&&!memcmp(copy,mask,sizeof(mask)));
 PhoneVisual_Session(2);CHECK(!PhoneVisual_CopyMask(2,copy,&rev));PhoneVisual_Session(1);
 CHECK(!Start(2,3,jpeg,sizeof(jpeg),0));Pump();CHECK(g_phone_visual.state==PV_READY);
 PhoneVisualImage im;CHECK(PhoneVisual_AcquireArt(3,&im));CHECK(im.width==16&&im.height==16&&im.bytes==512);
 /* Known JPEG is orange RGB(210,80,40), not blue BGR(40,80,210). */
 uint16_t pixel=(uint16_t)im.pixels[0]|((uint16_t)im.pixels[1]<<8);
 CHECK((pixel>>11)>=24&&(pixel>>11)<=28);CHECK(((pixel>>5)&63)>=17&&((pixel>>5)&63)<=23);CHECK((pixel&31)<=7);
 uint8_t old[512];memcpy(old,im.pixels,512);
 CHECK(!Start(2,4,jpeg,sizeof(jpeg),0));Pump();CHECK(g_phone_visual.state==PV_READY);
 CHECK(!memcmp(old,im.pixels,512));CHECK(!Start(2,5,jpeg,sizeof(jpeg),0));Pump();CHECK(g_phone_visual.state==PV_PROCESSING);
 PhoneVisual_ReleaseArt(im.bank);Pump();CHECK(g_phone_visual.state==PV_READY);CHECK(PhoneVisual_AcquireArt(5,&im));PhoneVisual_ReleaseArt(im.bank);
 CHECK(!Start(2,6,mask,512,0));Pump();CHECK(g_phone_visual.state==PV_FAILED);
 CHECK(!Start(1,7,mask,sizeof(mask),0));PhoneVisual_Process(0);PhoneVisual_Session(2);Pump();CHECK(g_phone_visual.result==CFW_EXPIRED);
 PhoneVisual_Session(1);CHECK(!Start(3,8,jpeg,sizeof(jpeg),0));Pump();CHECK(photo_accepts==1&&g_phone_visual.state==PV_PROCESSING);
 CHECK(PhoneVisual_Begin(1,1,9,sizeof(mask),0)==CFW_BUSY);photo_result=0;Pump();CHECK(g_phone_visual.state==PV_READY);
 for(uint32_t i=0;i<100;i++){CHECK(!Start(1,100+i,mask,sizeof(mask),0));Pump();CHECK(g_phone_visual.state==PV_READY);}

 uint32_t pins[12];memset(panel,0x65,sizeof(panel));
 CHECK(PhoneVisual_Begin(1,PV_PANEL,300,sizeof(panel)-1,0)==CFW_ARGUMENT);
 for(uint32_t i=0;i<12;i++){pins[i]=300+i;CHECK(!Start(PV_PANEL,pins[i],panel,sizeof(panel),0));Pump();CHECK(g_phone_visual.state==PV_READY);}
 PhoneVisual_KeepPanels(pins,12);
 for(uint32_t i=0;i<20;i++){CHECK(!Start(PV_PANEL,400+i,panel,sizeof(panel),0));Pump();CHECK(g_phone_visual.state==PV_READY);}
 for(uint32_t i=0;i<12;i++)CHECK(PhoneVisual_CopyPanel(pins[i],panel_copy,&rev)&&!memcmp(panel,panel_copy,sizeof(panel)));
 CHECK(!Start(PV_PANEL,500,panel,sizeof(panel),1));Pump();CHECK(g_phone_visual.state==PV_FAILED);
 PhoneVisual_Session(2);CHECK(!PhoneVisual_CopyPanel(300,panel_copy,&rev));
 CHECK(allocations==1);
 /* New sparse panels are lossless and immutable; malformed runs never
  * publish partial content or escape the original 128-row GPU bank. */
 PhoneVisual_Session(1);
 uint8_t packed[290]={128,2};for(uint32_t i=2;i<sizeof(packed);i+=2){packed[i]=255;packed[i+1]=0x34;}
 CHECK(!Start(PV_PACKED_PANEL,900,packed,sizeof(packed),0));Pump();CHECK(g_phone_visual.state==PV_READY);
 CHECK(PhoneVisual_CopyPanel(900,panel_copy,&rev)==192);
 for(uint32_t i=0;i<PHONE_PANEL_BYTES;i++)CHECK(panel_copy[i]==(i<PHONE_PANEL_LEGACY_BYTES?0x34:0));
 CHECK(!Start(PV_PACKED_PANEL,901,packed,sizeof(packed)-1,0));Pump();CHECK(g_phone_visual.result==CFW_CORRUPT);
 packed[0]=192;CHECK(!Start(PV_PACKED_PANEL,902,packed,sizeof(packed),0));Pump();CHECK(g_phone_visual.result==CFW_CORRUPT);
 packed[0]=128;packed[1]=3;CHECK(!Start(PV_PACKED_PANEL,903,packed,sizeof(packed),0));Pump();CHECK(g_phone_visual.result==CFW_CORRUPT);
 packed[1]=2;packed[2]=254;CHECK(!Start(PV_PACKED_PANEL,905,packed,sizeof(packed),0));Pump();CHECK(g_phone_visual.result==CFW_CORRUPT);
 CHECK(!PhoneVisual_CopyPanel(903,panel_copy,&rev));CHECK(PhoneVisual_CopyPanel(900,panel_copy,&rev)==192);
 CHECK(!Start(PV_PACKED_PANEL,904,packed_fixture,sizeof(packed_fixture),0));Pump();CHECK(g_phone_visual.state==PV_READY);
 CHECK(PhoneVisual_CopyPanel(904,panel_copy,&rev)==192);CHECK(!memcmp(panel_copy,mask_fixture,sizeof(mask_fixture)));
 /* Real phone-shaped CJK, negotiated kind9: decode matches raw kind7 bytes.
  * A truncated stream or stale page must not publish a partial title. */
 uint32_t status[7];CHECK(!PhoneVisual_MusicTrack(700,304,status));
 PhoneVisual_MusicVisible(700,1);CHECK(!PhoneVisual_MusicTrack(700,304,status));
 uint8_t tile[sizeof(music_rle)];memcpy(tile,music_rle,sizeof(tile));memcpy(tile+4,&status[2],4);
 CHECK(!Start(PV_PACKED_MUSIC_TILE,950,tile,sizeof(tile),0));Pump();CHECK(g_phone_visual.state==PV_READY);
 rev=0;CHECK(PhoneVisual_CopyMusic(700,copy,&rev));
 for(uint32_t y=0;y<36;y++)CHECK(!memcmp(copy+y*152,music_raw+16+y*192,152));
 CHECK(!memcmp(copy+36*152,music_raw+16+36*192,28*152));
 CHECK(!Start(PV_PACKED_MUSIC_TILE,951,tile,sizeof(tile)-1,0));Pump();CHECK(g_phone_visual.result==CFW_CORRUPT);
 PhoneVisual_MusicVisible(0,2);CHECK(!Start(PV_PACKED_MUSIC_TILE,952,tile,sizeof(tile),0));Pump();CHECK(g_phone_visual.result==CFW_EXPIRED);
 /* Header refresh never retransfers CJK bodies or alters reply/call pages. */
 PhoneVisual_Session(2);PhoneVisual_Session(1);
 memset(panel,0x22,sizeof(panel));CHECK(!Start(PV_PANEL,1001,panel,sizeof(panel),0));Pump();
 memset(panel,0x33,sizeof(panel));CHECK(!Start(PV_PANEL,1002,panel,sizeof(panel),0));Pump();
 memset(panel,0x44,sizeof(panel));CHECK(!Start(PV_PANEL,1003,panel,sizeof(panel),0));Pump();
 uint32_t live[4]={1001,1002,1002,1003};PhoneVisual_KeepPanels(live,4);
 rev=0;CHECK(PhoneVisual_CopyPanel(1003,panel_copy,&rev)==128);
 for(uint32_t i=0;i<sizeof(panel);i++)CHECK(panel_copy[i]==(i<3456?0x22:0x44));
 uint32_t old_revision=rev;
 memset(panel,0x55,sizeof(panel));CHECK(!Start(PV_PANEL,1004,panel,sizeof(panel),0));Pump();live[0]=1004;PhoneVisual_KeepPanels(live,4);
 CHECK(PhoneVisual_CopyPanel(1003,panel_copy,&rev)==128&&rev!=old_revision);
 for(uint32_t i=0;i<sizeof(panel);i++)CHECK(panel_copy[i]==(i<3456?0x55:0x44));
 rev=0;CHECK(PhoneVisual_CopyPanel(1002,panel_copy,&rev)==128);for(uint32_t i=0;i<sizeof(panel);i++)CHECK(panel_copy[i]==0x33);
 PhoneVisual_KeepPanels(0,0);rev=0;CHECK(PhoneVisual_CopyPanel(1003,panel_copy,&rev)==128);for(uint32_t i=0;i<sizeof(panel);i++)CHECK(panel_copy[i]==0x44);
 /* Negotiated large masks are native pixels, including timestamp/CJK. */
 PhoneVisual_Session(2);PhoneVisual_Session(1);
 CHECK(!Start(PV_PACKED_PANEL,1101,notification_large_rle,sizeof(notification_large_rle),0));Pump();CHECK(g_phone_visual.state==PV_READY);
 rev=0;CHECK(PhoneVisual_CopyPanel(1101,panel_copy,&rev)==144);CHECK(!memcmp(panel_copy,notification_large_a4,PHONE_PANEL_BYTES));
 uint32_t query=0;CHECK(PhoneVisual_CopyPanel(1101,0,&query)==144&&query==rev);
 CHECK(!Start(PV_PACKED_PANEL,1102,header_large_rle,sizeof(header_large_rle),0));Pump();
 uint32_t latest[4]={1102,0,0,1101};PhoneVisual_KeepPanels(latest,4);
 CHECK(PhoneVisual_CopyPanel(1101,panel_copy,&rev)==144);
 CHECK(!memcmp(panel_copy,header_large_a4,30*144));CHECK(!memcmp(panel_copy+30*144,notification_large_a4+30*144,PHONE_PANEL_BYTES-30*144));
 CHECK(!Start(PV_PACKED_PANEL,1103,replies_large_rle,sizeof(replies_large_rle),0));Pump();rev=0;
 CHECK(PhoneVisual_CopyPanel(1103,panel_copy,&rev)==162);CHECK(!memcmp(panel_copy,replies_large_a4,PHONE_PANEL_BYTES));
 CHECK(!Start(PV_PACKED_PANEL,1104,notification_large_rle,sizeof(notification_large_rle)-1,0));Pump();CHECK(g_phone_visual.result==CFW_CORRUPT);
 CHECK(!Start(PV_PACKED_PANEL,1105,notification_compact_sample_rle,sizeof(notification_compact_sample_rle),0));Pump();CHECK(g_phone_visual.state==PV_READY);
 rev=0;CHECK(PhoneVisual_CopyPanel(1105,panel_copy,&rev)==145);CHECK(!memcmp(panel_copy,notification_compact_sample_a4,PHONE_PANEL_BYTES));
 CHECK(!Start(PV_PACKED_PANEL,1106,header_compact_rle,sizeof(header_compact_rle),0));Pump();
 uint32_t compact_keep[4]={1106,0,0,1105};PhoneVisual_KeepPanels(compact_keep,4);
 CHECK(PhoneVisual_CopyPanel(1105,panel_copy,&rev)==145);
 CHECK(!memcmp(panel_copy,header_compact_a4,20*144));
 CHECK(!memcmp(panel_copy+20*144,notification_compact_sample_a4+20*144,PHONE_PANEL_BYTES-20*144));
 CHECK(allocations==1);
}
