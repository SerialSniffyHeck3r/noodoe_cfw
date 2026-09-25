#ifndef GRAPHICS_BACKGROUND_H
#define GRAPHICS_BACKGROUND_H
#include <stdint.h>

/* Two resident RGB565 textures support native480x480 cross-fades. Font/icon
 * cache, alpha strip, both photos, and the capture stripe never overlap. */
#define BACKGROUND_CACHE_END 0x1C000U
#define BACKGROUND_SMALL_RAM 0x1C000U
#define BACKGROUND_SMALL_BYTES 2048U
#define BACKGROUND_SHADE_RAM 0x1C800U
#define BACKGROUND_IMAGE_RAM 0x1D000U
#define BACKGROUND_IMAGE_B_RAM 0x8D800U
#define BACKGROUND_IMAGE_BYTES (480U*480U*2U)
#define BACKGROUND_UPLOAD_CHUNK 1024U
/* Resident SDRAM pixels bypass the small callback buffer; at42MHz this
 * bounds one direct transfer to about3.2ms instead of seconds per photo. */
#define BACKGROUND_DIRECT_UPLOAD_CHUNK 16384U
#define BACKGROUND_FADE_MS 240U
/* Optional file/NOR producer. Must return immediately:1 filled exactly count
 * bytes,0 temporarily busy,-1 failed. All calls run on the graphics owner,
 * outside EVE SPI ownership. No sleeping/retry loop is allowed in a provider. */
typedef int32_t (*BackgroundRead)(void *context,uint32_t offset,uint8_t *dst,uint32_t count);
typedef struct {
    const uint8_t *pixels;
    uint32_t width,height,bytes,revision;
    BackgroundRead read;
    void *context;
} BackgroundImage;

/* Tightly packed little-endian RGB565,1..480px each dimension. The source
 * owner retains immutable readable storage until deselected/replaced. Supply
 * exactly one of pixels or read; callback sources need no full MCU buffer. A new
 * revision is mandatory when the same address contains different pixels. */
uint32_t BackgroundImage_Valid(const BackgroundImage *image);
uint32_t BackgroundShade_Alpha(uint32_t y,uint32_t center_percent);
/* Graphics owner task only, outside an active draw burst. SetImage records a
 * request without I/O. Process sends at most16KiB of direct pixels or1KiB from a provider per call; ready is a
 * completion indication, not just acceptance. NULL removes the background. */
void GraphicsBackground_Init(void);
uint32_t GraphicsBackground_SetImage(const BackgroundImage *image);
uint32_t GraphicsBackground_SetCenterBrightness(uint32_t percent);
/* Owner-task, no I/O: hold photo, brightness and all animation clocks exactly.
 * SetImage/brightness requests are rejected while held. Resume preserves the
 * last frame's values and excludes the held interval from elapsed time. */
void GraphicsBackground_HoldFrame(uint32_t hold);
void GraphicsBackground_Process(void);
/* CPU/GPU upload and blend completion only; caller must still fence the final
 * display-list swap before retaining the frame or entering low power. */
uint32_t GraphicsBackground_Settled(void);
/* CPU source lifetime only. Cached GPU textures no longer dereference SDRAM;
 * selected/in-progress uploads do. Call on the Graphics owner task. */
uint32_t GraphicsBackground_SourceInUse(const void *pixels);
typedef struct {
    uint32_t magic,version,requests,ready,loading,uploaded,total,width,height;
    uint32_t center_percent,shade_uploads,image_uploads,draws,read_error;
    uint32_t alpha,phase,display_percent,resident_width,resident_height;
} BackgroundDiagnostics;
extern volatile BackgroundDiagnostics g_background;
#endif
