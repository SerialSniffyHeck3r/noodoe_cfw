#ifndef TRIP_GRAPHIC_H
#define TRIP_GRAPHIC_H
#include "lvgl.h"
typedef struct {lv_obj_t *root;uint32_t ratio,known,alpha;} TripGraphic;
/* Page body local origin is screen(72,145). One immutable object owns the
 * speed/distance glyphs and capsule. Updates allocate nothing and do no I/O. */
uint32_t TripGraphic_Create(TripGraphic *graphic,lv_obj_t *body);
void TripGraphic_Update(TripGraphic *graphic,uint32_t show,uint32_t known,uint32_t ratio);
void TripGraphic_Alpha(TripGraphic *graphic,uint32_t alpha);
#endif
