#ifndef PHONE_VISUAL_H
#define PHONE_VISUAL_H
#include <stdint.h>
/* Versioned phone-rendered content. No per-track allocation or NOR artwork. */
#define PHONE_VISUAL_WIDTH 304U
#define PHONE_VISUAL_HEIGHT 72U
#define PHONE_VISUAL_MASK_BYTES (304U*72U/2U)
#define PHONE_PANEL_LEGACY_BYTES (288U*128U/2U)
#define PHONE_PANEL_BYTES (288U*144U/2U)
/* Returns layout128/192 (legacy),144 (large strips),145 (compact two-line
 * notification strips),162 (large replies).
 * Destination always holds PHONE_PANEL_BYTES; legacy tails are zeroed.
 * NULL destination queries the immutable completed revision without copying. */
uint32_t PhoneVisual_CopyPanel(uint32_t key,void *dst,uint32_t *revision);
/* Ordered ownership: header, replies, call, then up to ten notifications.
 * CopyPanel overlays the matching header (legacy24 / large30 / compact20 rows) only for
 * those notifications; incompatible layouts never overwrite card pixels. */
void PhoneVisual_KeepPanels(const uint32_t *keys,uint32_t count);
enum {PV_MUSIC=1,PV_ART=2,PV_PHOTO0=3,PV_PHOTO1=4,PV_PHOTO2=5,PV_PANEL=6,PV_MUSIC_TILE=7,PV_PACKED_PANEL=8,PV_PACKED_MUSIC_TILE=9};
enum {PV_IDLE,PV_RECEIVING,PV_PROCESSING,PV_READY,PV_FAILED};
typedef struct {uint32_t state,kind,key,received,bytes,result,epoch,revision;} PhoneVisualStatus;
typedef struct {const uint8_t *pixels;uint32_t width,height,bytes,revision,key,bank;} PhoneVisualImage;
extern volatile PhoneVisualStatus g_phone_visual;
/* Storage owner initializes and processes at most eight JPEG MCUs per call. */
void PhoneVisual_Process(uint32_t now);
void PhoneVisual_Session(uint32_t epoch);
uint32_t PhoneVisual_Begin(uint32_t epoch,uint32_t kind,uint32_t key,uint32_t bytes,uint32_t crc);
uint32_t PhoneVisual_Data(uint32_t epoch,uint32_t key,uint32_t offset,const void *data,uint32_t bytes);
uint32_t PhoneVisual_Finish(uint32_t epoch,uint32_t key);
/* UI copy is bounded to one mask. Album source is leased until GPU upload
 * no longer references it; a displayed/leased bank is never overwritten. */
uint32_t PhoneVisual_CopyMask(uint32_t key,void *dst,uint32_t *revision);
uint32_t PhoneVisual_MusicTrack(uint32_t key,uint32_t width,uint32_t out[7]);
void PhoneVisual_MusicVisible(uint32_t key,uint32_t now);
uint32_t PhoneVisual_MusicRevision(uint32_t key);
uint32_t PhoneVisual_CopyMusic(uint32_t key,void *dst,uint32_t *revision);
uint32_t PhoneVisual_AcquireArt(uint32_t key,PhoneVisualImage *view);
void PhoneVisual_ReleaseArt(uint32_t bank);
#endif
