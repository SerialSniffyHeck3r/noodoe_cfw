#include "Product_FontBinding.h"
#include "Resources.h"
__attribute__((noinline)) uint32_t Product_FontsBindTable(const ProductFontBinding *table,uint32_t count)
{
 for(uint32_t i=0;i<count;i++){
  ResourceView view;
  if(!Resources_Get(table[i].resource,&view))return 0;
  table[i].descriptor->glyph_bitmap=view.data;
 }
 return 1;
}
