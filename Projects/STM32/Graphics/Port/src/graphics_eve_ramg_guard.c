#include "BSP_Display.h"
#include "Graphics_Background.h"
#include "src/draw/eve/lv_draw_eve_private.h"
#include "src/draw/eve/lv_draw_eve_ram_g.h"

/* The upstream allocator hardcodes1MiB. GNU --wrap interposes only its public
 * allocation entry, keeping vendor files unchanged. Cached entries never move.
 * New allocations stop at the capture stripe, or the smaller Product cache
 * boundary. Photo/capture reservations never become ordinary cache space.
 */
bool __real_lv_draw_eve_ramg_get_addr(uint32_t *,uintptr_t,uint32_t,uint32_t);
bool __wrap_lv_draw_eve_ramg_get_addr(uint32_t *address,uintptr_t key,uint32_t size,uint32_t alignment)
{
    lv_draw_eve_ramg_t *ramg=&lv_draw_eve_unit_g->ramg;
    if(!address || !key || !alignment || (alignment&(alignment-1U))){if(address)*address=LV_DRAW_EVE_RAMG_OUT_OF_RAMG;return false;}
    /* Search cached entries before checking free space: a full cache must still
     * allow already uploaded glyphs/images. Use upstream's FNV-1a key hash and
     * linear probing so normal glyph hits retain constant-time lookup cost. */
    if(ramg->hash_table && ramg->hash_table_cell_count){
        const uint8_t *bytes=(const uint8_t*)&key;uint32_t hash=2166136261U;
        for(uint32_t i=0;i<sizeof(key);++i)hash=(hash^bytes[i])*16777619U;
        uint32_t index=hash%ramg->hash_table_cell_count;
        for(uint32_t count=0;count<ramg->hash_table_cell_count;++count){
            if(ramg->hash_table[index].key==key){*address=ramg->hash_table[index].addr;return true;}
            if(!ramg->hash_table[index].key)break;
            if(++index==ramg->hash_table_cell_count)index=0U;
        }
    }
    uint64_t start=((uint64_t)ramg->ramg_addr_end+alignment-1U)&~((uint64_t)alignment-1U);
    uint32_t limit=BSP_DISPLAY_CAPTURE_RAM_G;
#if defined(NOODOE_PRODUCT) && NOODOE_PRODUCT
    /* Product reserves two full photos, small artwork and480-byte shade.
     * Ordinary image/font entries may never enter those reserved intervals. */
    limit=BACKGROUND_CACHE_END;
#endif
    if(start+size>limit){*address=LV_DRAW_EVE_RAMG_OUT_OF_RAMG;return false;}
    return __real_lv_draw_eve_ramg_get_addr(address,key,size,alignment);
}
