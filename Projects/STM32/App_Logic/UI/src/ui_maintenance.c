#include "Ui_Number.h"
#include "Ui_DashboardPresentation.h"

/* Calculate fraction without overflowing used*1000 at UINT64_MAX. Long
 * division of the remainder produces exactly three decimal digits, bounded. */
uint32_t UiDashboard_Remaining(uint64_t used,uint64_t total,uint32_t *remaining)
{
    if(!total||!remaining)return 0U;
    if(used>=total){*remaining=0U;return 1U;}
    uint64_t left=total-used;uint32_t ratio=0U;
    if(left==total){*remaining=1000U;return 1U;}
    /* Repeated addition modulo total avoids a64-bit multiplication by10. */
    for(uint32_t digit=0;digit<3U;++digit){
        uint64_t remainder=0;uint32_t quotient=0;
        for(uint32_t n=0;n<10U;++n){
            if(remainder>=total-left){remainder-=total-left;++quotient;}
            else remainder+=left;
        }
        ratio=ratio*10U+quotient;left=remainder;
    }
    *remaining=ratio;return 1U;
}

/* Calendar/IGN lifetime calculations remain in Settings. Auxiliary text is
 * intentionally absent from every footer; only the oil arc is rendered. */
void UiDashboard_PresentMaintenance(const UiDashboardMaintenance *m,UiDashboardPresentation *out)
{
    if(!out)return;
    out->maintenance=out->footer>=UI_OIL&&out->footer<=UI_SERV;
    out->remaining_valid=0U;out->remaining_permille=0U;
    out->auxiliary_title[0]=out->auxiliary_value[0]=0;
    if((!out->maintenance||out->footer==UI_OIL)&&m&&
        (m->valid_mask&1U)&&m->remaining_permille[0]<=1000U){
        out->remaining_valid=1U;out->remaining_permille=m->remaining_permille[0];
    }
}
