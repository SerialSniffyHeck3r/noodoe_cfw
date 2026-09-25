#include <stdint.h>
#include <string.h>
typedef struct {struct {uint32_t w,h,stride;} header;const void *data;} Image;
typedef struct {void *root;uint32_t panel,reply_index,reply_count,text_key,text_ready,text_revision,text_fence,text_wait;Image text_image;} MusicGraphic;
static struct {uint32_t render_count;} g_graphics;
static uint32_t key,revision,available,swapping,uploads,invalidates;
static uint8_t pixels[8];
volatile uint32_t failure,checks;
#define CHECK(x) do{checks++;if(!(x)){failure=__LINE__;return;}}while(0)
uint32_t Graphics_FramePresentedAfter(uint32_t frame){return !swapping&&g_graphics.render_count!=frame;}
uint32_t PhoneVisual_CopyMask(uint32_t k,void *out,uint32_t *rev){if(!available||key!=k)return 0;memset(out,key,8);*rev=revision;return 1;}
uint32_t PhoneVisual_CopyPanel(uint32_t k,void *out,uint32_t *rev){return PhoneVisual_CopyMask(k,out,rev);}
void Graphics_ImageChanged(const Image *im){(void)im;uploads++;}
void lv_obj_invalidate(void *root){(void)root;invalidates++;}
#include "actual_function.inc"
void Test(void){MusicGraphic v={.text_image={.data=pixels}};key=7;revision=1;
 CHECK(!MusicGraphic_Text(&v,7));CHECK(!uploads);
 available=1;CHECK(MusicGraphic_Text(&v,7));CHECK(uploads==1&&invalidates);
 for(int i=0;i<1000;i++)CHECK(MusicGraphic_Text(&v,7));CHECK(uploads==1);
 key=8;revision=2;CHECK(!MusicGraphic_Text(&v,8));CHECK(uploads==1&&!v.text_ready);
 g_graphics.render_count++;swapping=1;CHECK(!MusicGraphic_Text(&v,8));CHECK(uploads==1);
 swapping=0;CHECK(MusicGraphic_Text(&v,8));CHECK(uploads==2&&pixels[0]==8);
 key=9;revision=3;CHECK(!MusicGraphic_Panel(&v,9,1,6));CHECK(uploads==2);
 g_graphics.render_count++;CHECK(MusicGraphic_Panel(&v,9,1,6));CHECK(v.text_image.header.w==288&&v.text_image.header.h==128&&uploads==3);
 CHECK(!MusicGraphic_Text(&v,0));g_graphics.render_count++;CHECK(!MusicGraphic_Text(&v,0));CHECK(uploads==3);
}
