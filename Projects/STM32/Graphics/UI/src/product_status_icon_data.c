#include "Product_StatusIcons.h"
#include "Resources.h"
/* Google Material Icons Round, pinned source; Apache-2.0, ICON_NOTICES.txt. */

static lv_image_dsc_t icon0={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_A4,.w=88,.h=88,.stride=44},.data_size=3872,.data=NULL};

static lv_image_dsc_t icon1={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_A4,.w=24,.h=24,.stride=12},.data_size=288,.data=NULL};

static lv_image_dsc_t icon2={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_A4,.w=24,.h=24,.stride=12},.data_size=288,.data=NULL};
const lv_image_dsc_t *Product_StatusIcon(uint32_t id){return id==0?&icon0:id==1?&icon1:id==2?&icon2:0;}
uint32_t Product_StatusIconsBind(void){ResourceView v;if(!Resources_Get(RESOURCE_ICON_STATUS,&v)||v.bytes!=4448)return 0;icon0.data=v.data;icon1.data=v.data+3872;icon2.data=v.data+4160;return 1;}
