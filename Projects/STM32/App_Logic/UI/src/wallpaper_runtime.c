#include "Wallpaper_Runtime.h"
#include "Ui_OffStages.h"
#include "Wallpaper.h"
#include "PhotoService.h"
#include "PhoneVisual.h"
#include <string.h>
static uint8_t album_pixels[PHONE_ART_BYTES];
static uint32_t album_revision;
static PhoneVisualImage borrowed[2];
static uint32_t borrowed_mask;

/* The provisional second has no visible side effects, including brightness.
 * Only the confirmed shutdown's normal Process request starts the fade. */
void WallpaperRuntime_HoldShutdown(uint32_t hold)
{
    GraphicsBackground_HoldFrame(hold);
}

/* Phone presentation scratch is overwritten on the next tick. Own just the
 * small incoming32px album so bounded multi-tick GPU upload never dereferences
 * a transient page or races its producer. Compare content as well as source
 * revision: two simultaneously connected phones can reuse revision values. */
void WallpaperRuntime_Process(const DashboardPage *page,const UiState *state)
{
    if(!page||!state)return;
    if(state->power==IGN_STOPPING&&state->off_phase==UI_OFF_DELAY)return;
    /* Skipped display-hold stages never upload an invisible photograph. */
    if(state->power==OFF_BT_HOLD||state->power==OFF_DEEP_SLEEP)return;
    /* A warm ON reversal is already returning to the active page. Treating
     * every non-IGN_ON state as OFF would replace music artwork even when the
     * provisional second was cancelled and the ride never ended. */
    uint32_t off=state->power==IGN_STOPPING||Ui_OffStages_IsOff(state->power)||(state->power==IGN_STARTING&&state->welcome_active);
    uint32_t phase=off?WALLPAPER_OFF:WALLPAPER_ON;
    if(state->power==IGN_STOPPING&&state->off_phase==UI_OFF_SUMMARY)phase=WALLPAPER_SUMMARY;
    /* Get copies an atomic descriptor. Retired pixel buffers remain immutable
     * until the graphics owner releases them after its last CPU upload. */
    for(uint32_t i=0;i<PHOTO_SLOTS;++i){PhotoImage photo;
        if(PhotoService_Get(i,&photo)){BackgroundImage image={.pixels=photo.pixels,.width=photo.width,.height=photo.height,.bytes=photo.bytes,.revision=photo.revision};
            (void)Wallpaper_SetPhoto(i,&image);}
        else (void)Wallpaper_SetPhoto(i,NULL);
    }
    BackgroundImage album={0},source;
    PhoneVisualImage visual={0};
    if(!off&&page->kind==UI_MUSIC&&page->known&&PhoneVisual_AcquireArt(page->visual_key,&visual)){
        if(borrowed_mask&(1U<<visual.bank))PhoneVisual_ReleaseArt(visual.bank);
        else{borrowed[visual.bank]=visual;borrowed_mask|=1U<<visual.bank;}
        album=(BackgroundImage){.pixels=visual.pixels,.width=visual.width,.height=visual.height,.bytes=visual.bytes,.revision=visual.revision};
    }
    if(!album.pixels&&!page->visual_key&&!off&&page->kind==UI_MUSIC&&page->known&&page->art_valid&&page->art){
        if(!album_revision||memcmp(album_pixels,page->art,sizeof(album_pixels))){
            memcpy(album_pixels,page->art,sizeof(album_pixels));if(!++album_revision)++album_revision;
        }
        album=(BackgroundImage){.pixels=album_pixels,.width=PHONE_ART_SIDE,.height=PHONE_ART_SIDE,
            .bytes=PHONE_ART_BYTES,.revision=album_revision};
    }
    uint32_t valid=Wallpaper_Resolve(state->power==OFF_DISPLAY_HOLD?3U:off?2U:page->kind==UI_MUSIC,&album,&source);
    (void)GraphicsBackground_SetCenterBrightness(
        Wallpaper_ResolveBrightness(page->kind,page->selection,phase));
    (void)GraphicsBackground_SetImage(valid?&source:NULL);
    GraphicsBackground_Process();
    for(uint32_t i=0;i<2;i++)if((borrowed_mask&(1U<<i))&&!GraphicsBackground_SourceInUse(borrowed[i].pixels)){
        PhoneVisual_ReleaseArt(i);borrowed_mask&=~(1U<<i);}
    for(uint32_t i=0;i<PHOTO_SLOTS;++i){const void *old=PhotoService_Retiring(i);
        if(old&&!GraphicsBackground_SourceInUse(old))PhotoService_Release(i,old);}
}
