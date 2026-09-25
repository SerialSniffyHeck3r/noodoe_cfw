#include "Product_FuelVector.h"
/* Geometric adaptation of Google Material Icons Round local_gas_station,
 * Graphics/Assets/Icons/source/local_gas_station_round_24.svg (Apache-2.0).
 * Coordinates are quarter SVG units. The visible bounds are x4..20.5/y3..21;
 * centering on (12.25,12) removes the original padded-canvas offset.
 * The frame, hose, nozzle and fill use native LVGL primitives. Curves are
 * rounded at final pixel size; no enlarged 88px glyph or GPU allocation.
 * A bounded 10 primitives also avoids a per-scanline task/heap explosion. */
static int Scale(int q,unsigned size){return (q*(int)size+(q>=0?48:-48))/96;}
void Product_FuelVector(lv_layer_t *layer,int cx,int cy,unsigned size,
                        uint32_t color,uint8_t opacity)
{
 if(!opacity||!size)return;
 int x=cx-Scale(49,size),y=cy-Scale(48,size);
 lv_color_t ink=lv_color_hex(color);
 /* Hollow pump frame and solid lower housing retain transparent openings. */
 lv_draw_rect_dsc_t r;lv_draw_rect_dsc_init(&r);r.radius=Scale(8,size);
 r.bg_opa=0;r.border_width=Scale(8,size);r.border_color=ink;r.border_opa=opacity;
 lv_area_t a={x+Scale(16,size),y+Scale(12,size),x+Scale(56,size)-1,y+Scale(84,size)-1};
 lv_draw_rect(layer,&r,&a);
 r.border_opa=0;r.bg_color=ink;r.bg_opa=opacity;r.radius=Scale(4,size);
 a=(lv_area_t){x+Scale(16,size),y+Scale(40,size),x+Scale(56,size)-1,y+Scale(84,size)-1};
 lv_draw_rect(layer,&r,&a);
 /* Hose is a continuous 1.5 SVG-unit stroke. All lines request rounded
  * ends explicitly, including axial ones, using the existing public API. */
 static const int8_t lines[][4]={{54,51,60,51},{63,56,63,74},{79,36,79,74},{64,18,77,31}};
 lv_draw_line_dsc_t l;lv_draw_line_dsc_init(&l);l.color=ink;l.opa=opacity;
 l.width=Scale(6,size);l.round_start=l.round_end=1;
 for(unsigned i=0;i<sizeof(lines)/sizeof(lines[0]);++i){
  l.p1=(lv_point_precise_t){x+Scale(lines[i][0],size),y+Scale(lines[i][1],size)};
  l.p2=(lv_point_precise_t){x+Scale(lines[i][2],size),y+Scale(lines[i][3],size)};lv_draw_line(layer,&l);
 }
 /* Fixed outer-radius arcs reproduce the hose bend and nozzle opening. */
 static const int16_t arcs[][5]={{60,56,8,270,360},{71,74,11,0,180},{72,36,10,0,360}};
 lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);d.color=ink;d.opa=opacity;d.width=Scale(6,size);
 for(unsigned i=0;i<sizeof(arcs)/sizeof(arcs[0]);++i){
  d.center=(lv_point_t){x+Scale(arcs[i][0],size),y+Scale(arcs[i][1],size)};
  d.radius=Scale(arcs[i][2],size);d.start_angle=arcs[i][3];d.end_angle=arcs[i][4];lv_draw_arc(layer,&d);
 }
}
