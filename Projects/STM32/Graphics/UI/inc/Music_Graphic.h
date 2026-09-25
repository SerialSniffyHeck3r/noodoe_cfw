#ifndef MUSIC_GRAPHIC_H
#define MUSIC_GRAPHIC_H
#include "lvgl.h"
typedef struct {lv_obj_t *root;int32_t slide_y;uint32_t alpha,ratio,playing,known,text_ready,text_revision,text_key,text_fence,text_wait,call_color,panel,reply_index,reply_count;lv_image_dsc_t text_image;} MusicGraphic;
uint32_t MusicGraphic_Panel(MusicGraphic *view,uint32_t key,uint32_t reply_index,uint32_t reply_count);
uint32_t MusicGraphic_Text(MusicGraphic *view,uint32_t key);
uint32_t MusicGraphic_Marquee(MusicGraphic *view,uint32_t key);
uint32_t MusicGraphic_Number(MusicGraphic *view,const char *text);
uint32_t MusicGraphic_Create(MusicGraphic *view,lv_obj_t *body);
void MusicGraphic_Update(MusicGraphic *view,uint32_t show,uint32_t known,uint32_t playing,uint32_t ratio);
void MusicGraphic_Alpha(MusicGraphic *view,uint32_t alpha);
void MusicGraphic_Hints(uint32_t playing,uint32_t scope,uint32_t alpha,uint32_t now_ms);
#endif
