#ifndef WALLPAPER_H
#define WALLPAPER_H
#include "Graphics_Background.h"
#define WALLPAPER_PHOTO_COUNT 3U
typedef struct {
    BackgroundImage photos[WALLPAPER_PHOTO_COUNT];
    uint32_t enabled,selected,center_percent,off_photo,fallback;
} WallpaperState;
/* App policy is independent of Bluetooth, files and renderer objects. These
 * nonblocking APIs are UI-owner-task calls (not ISR/cross-task publishers).
 * Photo slots borrow immutable RGB565 storage; NULL unregisters a slot.
 * This is not a persistent file import or a flash/NOR write API. */
void Wallpaper_Init(void);
uint32_t Wallpaper_SetPhoto(uint32_t slot,const BackgroundImage *photo);
uint32_t Wallpaper_SelectPhoto(uint32_t slot);
uint32_t Wallpaper_SetEnabled(uint32_t enabled);
uint32_t Wallpaper_SetCenterBrightness(uint32_t percent);
/* 0 follows the configured riding photo; 1..3 select slots0..2. */
uint32_t Wallpaper_SetOffPhoto(uint32_t choice);
void Wallpaper_GetState(WallpaperState *state);
/* source policy:0 normal photo,1 music priority,2 forced selected photo,
 * 3 display-hold photo (with selected-photo/plain fallback diagnostics).
 * Policy2 bypasses wallpaper disabled without changing the saved preference. */
uint32_t Wallpaper_Resolve(uint32_t policy,const BackgroundImage *album,BackgroundImage *out);
enum {WALLPAPER_ON,WALLPAPER_OFF,WALLPAPER_SUMMARY};
uint32_t Wallpaper_ResolveBrightness(uint32_t page,uint32_t selection,uint32_t phase);
#endif
