#ifndef MUSIC_TILES_H
#define MUSIC_TILES_H
#include <stdint.h>
#define MUSIC_TILE_WIDTH 384U
#define MUSIC_TITLE_HEIGHT 36U
#define MUSIC_ARTIST_HEIGHT 28U
#define MUSIC_VIEW_WIDTH 304U
#define MUSIC_VIEW_HEIGHT 72U
#define MUSIC_TITLE_BYTES (MUSIC_TILE_WIDTH*MUSIC_TITLE_HEIGHT/2U)
#define MUSIC_ARTIST_BYTES (MUSIC_VIEW_WIDTH*MUSIC_ARTIST_HEIGHT/2U)
#define MUSIC_TILE_BYTES (16U+MUSIC_TITLE_BYTES+MUSIC_ARTIST_BYTES)
/* Two title tiles and one fixed artist, never a complete scrolling strip.
 * Caller serializes worker/UI access; this core has no hardware or UI owner. */
typedef struct {
 uint32_t key,width,view,active,index[2],valid,artist_valid;
 uint32_t offset,phase,wait_ms,last_ms,fraction,revision;
 uint8_t title[2][MUSIC_TITLE_BYTES],artist[MUSIC_ARTIST_BYTES];
} MusicTiles;
uint32_t MusicTiles_Track(MusicTiles *m,uint32_t key,uint32_t width);
void MusicTiles_Visible(MusicTiles *m,uint32_t key,uint32_t now);
void MusicTiles_Status(const MusicTiles *m,uint32_t out[7]);
uint32_t MusicTiles_Accept(MusicTiles *m,const uint8_t *data,uint32_t bytes);
uint32_t MusicTiles_Copy(const MusicTiles *m,uint32_t key,uint8_t *out,uint32_t *revision);
#endif
