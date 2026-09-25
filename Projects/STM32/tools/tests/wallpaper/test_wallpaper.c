#include "Ui_State.h"
#include "Wallpaper.h"
#include <string.h>
static uint32_t assertions;
static int32_t DummyRead(void *context,uint32_t offset,uint8_t *dst,uint32_t count)
{(void)context;(void)offset;(void)dst;(void)count;return 0;}
#define CHECK(x) do{++assertions;if(!(x))return __LINE__;}while(0)
uint32_t get_assertions(void){return assertions;}

/* Exercise all three actual descriptors, invalid replacement preservation,
 * off/fallback and album priority. Pixel storage is never dereferenced here. */
uint32_t test_policy(void)
{
    Wallpaper_Init();BackgroundImage out;
    CHECK(!Wallpaper_Resolve(0,0,&out));CHECK(!out.pixels);
    BackgroundImage photo={.pixels=(const uint8_t*)0x20010000,.width=480,.height=480,.bytes=BACKGROUND_IMAGE_BYTES,.revision=1};
    BackgroundImage album={.pixels=(const uint8_t*)0x20020000,.width=32,.height=32,.bytes=2048,.revision=2};
    for(uint32_t i=0;i<3;++i){photo.revision=i+1;CHECK(Wallpaper_SetPhoto(i,&photo));}
    CHECK(!Wallpaper_SetPhoto(3,&photo));CHECK(!Wallpaper_SelectPhoto(3));
    for(uint32_t i=0;i<3;++i){CHECK(Wallpaper_SelectPhoto(i));CHECK(Wallpaper_Resolve(0,&album,&out));CHECK(out.revision==i+1);}
    photo.width=481;CHECK(!Wallpaper_SetPhoto(2,&photo));CHECK(Wallpaper_Resolve(0,0,&out));CHECK(out.revision==3);
    CHECK(Wallpaper_SetEnabled(0));CHECK(!Wallpaper_Resolve(0,&album,&out));
    CHECK(Wallpaper_Resolve(1,&album,&out));CHECK(out.pixels==album.pixels);
    CHECK(Wallpaper_Resolve(2,&album,&out));CHECK(out.revision==3);
    album.bytes=2047;CHECK(!Wallpaper_Resolve(1,&album,&out));
    CHECK(Wallpaper_SetEnabled(1));CHECK(Wallpaper_Resolve(1,&album,&out));CHECK(out.revision==3);
    CHECK(Wallpaper_SetPhoto(2,0));CHECK(!Wallpaper_Resolve(0,0,&out));
    CHECK(!Wallpaper_SetEnabled(2));CHECK(!Wallpaper_SetCenterBrightness(101));
    WallpaperState state;Wallpaper_GetState(&state);CHECK(state.center_percent==100);
    for(uint32_t i=0;i<=100;++i){CHECK(Wallpaper_SetCenterBrightness(i));Wallpaper_GetState(&state);CHECK(state.center_percent==i);}
    CHECK(!Wallpaper_Resolve(1,&album,0));return 0;
}

/* The complete middle band must have exactly one alpha, especially zero at
 * default brightness. Both fades are monotonic and never lighten when the
 * caller dims the center. All101 percentages and480 scanlines are checked. */
uint32_t test_shading(void)
{
    for(uint32_t p=0;p<=100;++p){
        for(uint32_t y=0;y<480;++y){
            uint32_t a=BackgroundShade_Alpha(y,p);CHECK(a<=255);
            if(y>=160&&y<=320)CHECK(a==BackgroundShade_Alpha(240,p));
            if(y&&y<=160)CHECK(a<=BackgroundShade_Alpha(y-1,p));
            if(y>320)CHECK(a>=BackgroundShade_Alpha(y-1,p));
            if(p)CHECK(a<=BackgroundShade_Alpha(y,p-1));
            if(!p)CHECK(a==255);
        }
    }
    CHECK(BackgroundShade_Alpha(240,100)==0);
    CHECK(BackgroundShade_Alpha(71,100)>=220);CHECK(BackgroundShade_Alpha(425,100)>=220);
    CHECK(BackgroundShade_Alpha(240,40)==153);return 0;
}
uint32_t test_memory_bounds(void)
{
    BackgroundImage a={.pixels=(const uint8_t*)1,.width=480,.height=480,.bytes=BACKGROUND_IMAGE_BYTES,.revision=1};
    CHECK(BackgroundImage_Valid(&a));CHECK(!BackgroundImage_Valid(0));
    a.read=DummyRead;CHECK(!BackgroundImage_Valid(&a));a.pixels=0;CHECK(BackgroundImage_Valid(&a));
    a.width=0;CHECK(!BackgroundImage_Valid(&a));a.width=480;a.bytes--;CHECK(!BackgroundImage_Valid(&a));
    a.height=0xFFFFFFFFU;CHECK(!BackgroundImage_Valid(&a));
    CHECK(BACKGROUND_CACHE_END<=BACKGROUND_SHADE_RAM);
    CHECK(BACKGROUND_SHADE_RAM+480<=BACKGROUND_IMAGE_RAM);
    CHECK(BACKGROUND_IMAGE_RAM+BACKGROUND_IMAGE_BYTES==BACKGROUND_IMAGE_B_RAM);
    CHECK(BACKGROUND_IMAGE_B_RAM+BACKGROUND_IMAGE_BYTES==0xFE000U);
    CHECK(0xFE000U+480U*8U*2U==0xFFE00U);
    CHECK(BACKGROUND_UPLOAD_CHUNK<=2048U);return 0;
}
uint32_t test_readability(void)
{
    Wallpaper_Init();
    CHECK(Wallpaper_ResolveBrightness(UI_BLANK,0,WALLPAPER_ON)==100);
    CHECK(Wallpaper_ResolveBrightness(UI_BLANK,1,WALLPAPER_ON)==20);
    CHECK(Wallpaper_ResolveBrightness(UI_BLANK,2,WALLPAPER_ON)==20);
    for(uint32_t page=0;page<=UI_SYSTEM;++page){
        CHECK(Wallpaper_ResolveBrightness(page,0,WALLPAPER_OFF)==20);
        CHECK(Wallpaper_ResolveBrightness(page,0,WALLPAPER_SUMMARY)==20);
        if(page!=UI_BLANK&&page!=UI_MUSIC)CHECK(Wallpaper_ResolveBrightness(page,0,WALLPAPER_ON)==20);
    }
    CHECK(Wallpaper_ResolveBrightness(UI_MUSIC,0,WALLPAPER_ON)==32);
    CHECK(Wallpaper_SetCenterBrightness(50));
    CHECK(Wallpaper_ResolveBrightness(UI_MUSIC,0,WALLPAPER_ON)==16);
    CHECK(Wallpaper_ResolveBrightness(UI_BLANK,0,WALLPAPER_ON)==50);
    CHECK(Wallpaper_ResolveBrightness(UI_BLANK,1,WALLPAPER_ON)==10);
    CHECK(Wallpaper_ResolveBrightness(UI_MUSIC,0,WALLPAPER_SUMMARY)==20);
    WallpaperState state;Wallpaper_GetState(&state);CHECK(state.center_percent==50);
    return 0;
}
