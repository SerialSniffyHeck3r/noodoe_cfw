#include "Product_Preview.h"
#include <stddef.h>
char *strcpy(char *out,const char *in){char *start=out;while((*out++=*in++));return start;}
#define C(x) do{if(!(x))return __LINE__;}while(0)
/* A rendering override must not emit actions, mutate the real state, accept
 * invalid input, survive expiry, or leave a preview active after a cancel. */
unsigned test_preview(void)
{
    UiState s;Ui_Init(&s,0,0);UiDashboardPresentation out;
    ProductPreview_Init();C(g_product_preview.magic==0x50565031U);
    g_product_preview.footer=UI_OIL;g_product_preview.remaining_permille=750;
    g_product_preview.valid=1;g_product_preview.ttl_ms=1000;g_product_preview.request_id=1;
    C(ProductPreview_Apply(&s,20,&out));C(out.footer==UI_OIL&&out.maintenance&&!out.auxiliary_title[0]);
    C(s.dashboard.footer==UI_ODO&&s.power==UI_BOOT&&s.effect_count==0);
    C(g_product_preview.ack_id==1&&g_product_preview.active_id==1);
    C(ProductPreview_Apply(&s,1019,&out));C(!ProductPreview_Apply(&s,1020,&out));
    g_product_preview.request_id=2;g_product_preview.remaining_permille=1001;
    C(!ProductPreview_Apply(&s,1030,&out)&&g_product_preview.result==2);
    g_product_preview.request_id=3;g_product_preview.remaining_permille=250;g_product_preview.valid=0;
    C(ProductPreview_Apply(&s,0xfffffff0U,&out)&&!out.remaining_valid);
    C(ProductPreview_Apply(&s,983,&out));C(!ProductPreview_Apply(&s,984,&out));
    g_product_preview.request_id=4;C(ProductPreview_Apply(&s,1000,&out));
    ProductPreview_Cancel();C(!ProductPreview_Apply(&s,1001,&out));
    g_product_preview.request_id=5;C(ProductPreview_Apply(&s,2000,&out));
    g_product_preview.footer=UINT32_MAX;g_product_preview.request_id=6;
    C(!ProductPreview_Apply(&s,2001,&out)&&g_product_preview.ack_id==6);
    g_product_preview.footer=UI_RESV;g_product_preview.remaining_permille=200;
    g_product_preview.valid=1;g_product_preview.request_id=7;
    C(ProductPreview_Apply(&s,3000,&out)&&!out.reserve_distance_warning&&out.footer_valid);
    g_product_preview.remaining_permille=201;g_product_preview.request_id=8;
    C(ProductPreview_Apply(&s,3001,&out)&&out.reserve_distance_warning);
    g_product_preview.valid=0;g_product_preview.request_id=9;
    C(ProductPreview_Apply(&s,3002,&out)&&!out.reserve_distance_warning&&!out.footer_valid);
    g_product_preview.footer=0x100;g_product_preview.valid=1;g_product_preview.request_id=10;
    C(!ProductPreview_Apply(&s,3003,&out)&&g_product_preview.result==2);
    C(s.dashboard.card==UI_BLANK&&s.dashboard.footer==UI_ODO&&!s.links&&!s.effect_count);
    g_product_preview.valid=0;g_product_preview.request_id=11;
    C(!ProductPreview_Apply(&s,3004,&out)&&g_product_preview.result==2);
    g_product_preview.footer=0x108;g_product_preview.request_id=12;
    C(!ProductPreview_Apply(&s,3005,&out)&&g_product_preview.result==2);
    ProductPreview_Cancel();
    return 0;
}
