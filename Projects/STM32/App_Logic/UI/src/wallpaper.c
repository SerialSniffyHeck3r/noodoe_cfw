#include "Ui_State.h"
#include "Wallpaper.h"
#include <string.h>
static WallpaperState wallpaper;

/* Empty registered slots never imply fabricated photographs. Wallpaper is
 * enabled by default but remains black until a valid source is registered. */
void Wallpaper_Init(void)
{memset(&wallpaper,0,sizeof(wallpaper));wallpaper.enabled=1;wallpaper.center_percent=100;}

/* Validate before modifying an existing slot; rejected data preserves the
 * previous selection/source. Three descriptors do not allocate three frames. */
uint32_t Wallpaper_SetPhoto(uint32_t slot,const BackgroundImage *photo)
{
    if(slot>=WALLPAPER_PHOTO_COUNT||(photo&&!BackgroundImage_Valid(photo)))return 0;
    if(photo)wallpaper.photos[slot]=*photo;else memset(&wallpaper.photos[slot],0,sizeof(BackgroundImage));
    return 1;
}
uint32_t Wallpaper_SelectPhoto(uint32_t slot)
{if(slot>=WALLPAPER_PHOTO_COUNT)return 0;wallpaper.selected=slot;return 1;}
uint32_t Wallpaper_SetEnabled(uint32_t enabled)
{if(enabled>1U)return 0;wallpaper.enabled=enabled;return 1;}
uint32_t Wallpaper_SetCenterBrightness(uint32_t percent)
{if(percent>100U)return 0;wallpaper.center_percent=percent;return 1;}
uint32_t Wallpaper_SetOffPhoto(uint32_t choice)
{if(choice>3U)return 0;wallpaper.off_photo=choice;return 1;}
void Wallpaper_GetState(WallpaperState *state)
{if(state)*state=wallpaper;}

/* Centralize source priority here so future photo/storage/phone providers
 * cannot each invent a different meaning for off or missing album artwork. */
uint32_t Wallpaper_Resolve(uint32_t music,const BackgroundImage *album,BackgroundImage *out)
{
    if(!out)return 0;
    memset(out,0,sizeof(*out));
    wallpaper.fallback=0;
    if(music==1U&&BackgroundImage_Valid(album)){*out=*album;return 1;}
    if(music==3U&&wallpaper.off_photo){
        uint32_t slot=wallpaper.off_photo-1U;
        if(BackgroundImage_Valid(&wallpaper.photos[slot])){*out=wallpaper.photos[slot];return 1;}
        wallpaper.fallback=1;
    }
    if((wallpaper.enabled||music>=2U)&&wallpaper.selected<WALLPAPER_PHOTO_COUNT&&BackgroundImage_Valid(&wallpaper.photos[wallpaper.selected])){
        *out=wallpaper.photos[wallpaper.selected];return 1;
    }if(music==3U)wallpaper.fallback=2;return 0;
}

/* Image content never affects shade. OFF/summary retain20% brightness;
 * the user central dimmer remains separate on ON. */
uint32_t Wallpaper_ResolveBrightness(uint32_t page,uint32_t selection,uint32_t phase)
{
    if(phase==WALLPAPER_SUMMARY)return 20U;
    if(phase==WALLPAPER_OFF)return 20U;
    uint32_t factor=page==UI_BLANK&&selection<2U?100U:page==UI_MUSIC?32U:20U;
    return wallpaper.center_percent*factor/100U;
}
