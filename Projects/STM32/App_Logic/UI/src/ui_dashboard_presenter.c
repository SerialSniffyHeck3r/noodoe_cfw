#include "Ui_Number.h"
#include "Ui_DashboardPresentation.h"
#include <string.h>

/* Bounded copy keeps view buffers valid; capacity includes the terminating0. */
static void Copy(char *out,uint32_t capacity,const char *text)
{uint32_t i=0;while(i+1U<capacity&&text[i]){out[i]=text[i];++i;}out[i]=0;}

/* Preserve field capacity, precision and explicit unknown/overflow markers.
 * Remove only redundant integer zeroes; the view owns fixed width/alignment. */
static void Fixed(char *out,uint64_t value,uint32_t integer_digits,uint32_t decimal,uint32_t valid)
{
    uint32_t digits=integer_digits+decimal,length=digits+decimal;
    uint64_t limit=1;for(uint32_t i=0;i<digits;++i)limit*=10U;
    uint32_t overflow=value>=limit;
    out[length]=0;
    for(uint32_t i=length;i>0U;){--i;
        if(decimal&&i==integer_digits){out[i]='.';continue;}
        out[i]=!valid?'-':overflow?'#':(char)('0'+value%10U);value/=10U;
    }
    UiNumber_RemoveLeadingZeros(out);
}

/* Convert large cumulative millimetres without overflow in mm*10. Each mode
 * has its own established number of integer/fractional digits. ODO always
 * comes from actual dashboard telemetry, never a caller-supplied trip slot. */
static void Footer(const UiState *s,const UiDashboardDistances *dist,uint32_t odo,
    uint32_t valid,uint32_t miles,UiDashboardPresentation *out)
{
    uint32_t f=s->dashboard.footer<UI_FOOTER_COUNT?s->dashboard.footer:UI_ODO;
    uint32_t decimal=(f==UI_TRIP1||f==UI_TRIP2||f==UI_RESV||f==UI_OIL);
    uint32_t digits=f==UI_ODO?6U:f==UI_RESV?3U:decimal?4U:5U;
    uint64_t mm=(uint64_t)odo*1000000ULL;
    if(f!=UI_ODO){valid=dist&&(dist->valid_mask&UI_BIT(f));mm=dist?dist->distance_mm[f]:0;}
    uint64_t unit=miles?1609344ULL:1000000ULL;
    uint64_t value=decimal?(mm/unit)*10U+((mm%unit)*10U)/unit:mm/unit;
    Copy(out->footer_title,sizeof(out->footer_title),UiDashboard_FooterTitle(f));
    Copy(out->unit,sizeof(out->unit),miles?"mi":"km");
    Fixed(out->footer_value,value,digits,decimal,!!valid);
    out->footer=f;out->footer_valid=!!valid;
    /* Reserve is distance travelled since low fuel, never estimated range.
     * Compare raw millimetres before formatting/rounding or mile conversion. */
    out->reserve_distance_warning=f==UI_RESV&&valid&&mm>20000000ULL;
}

/* Parent overlays preempt the child, but never change its selected card.
 * Unimplemented commands remain visible as unavailable; no fake successful
 * pairing/reset/save is shown. Renderer uses number[] with the numeric face. */
static uint32_t Parent(const UiState *s,UiDashboardPresentation *out)
{
    if(s->power==UI_FAULT){Copy(out->title,sizeof(out->title),"UI error");return 1;}
    if(s->warning){
        static const char *const title[]={"","Low fuel","Oil service","Belt service","Service due"};
        Copy(out->title,sizeof(out->title),title[s->warning]);
        Copy(out->line,sizeof(out->line),s->warning==UI_WARN_FUEL?"Reserve trip active":"Maintenance reminder");return 1;
    }
    /* SettingsView draws the actual menu/editor. Keep only the shared shell
     * and avoid a second set of labels or confirmation controls underneath. */
    if(s->menu||s->modal)return 1;
    return 0;
}

/* Produce the child's mini-mode page from facts, independent of LVGL/BSP.
 * Phone-dependent pages stay navigable with explicit offline text. Missing
 * phone/BT data never becomes zero notifications or a fabricated song title. */
void UiDashboard_Present(const UiState *s,const UiDashboardDistances *dist,
    uint32_t odo,uint32_t odo_valid,uint32_t units,uint32_t uart_live,UiDashboardPresentation *out)
{
    if(!s||!out)return;
    memset(out,0,sizeof(*out));out->card=s->dashboard.card;
    Footer(s,dist,odo,odo_valid,!!units,out);if(Parent(s,out))return;
    Copy(out->title,sizeof(out->title),UiDashboard_CardTitle(s->dashboard.card));
    switch(s->dashboard.card){
    case UI_TRIP:
        Copy(out->line,sizeof(out->line),uart_live?"Dashboard connected":"Waiting for dashboard");
        Copy(out->hint,sizeof(out->hint),"UP/DOWN: select trip");break;
    case UI_NOTIFICATIONS:
        Copy(out->line,sizeof(out->line),s->links&UI_LINK_PHONES?"Waiting for notifications":"No phone connected");break;
    case UI_MUSIC:
        Copy(out->line,sizeof(out->line),s->links&UI_LINK_PHONES?"Waiting for media":"No phone connected");
        if(s->links&UI_LINK_PHONES){static const char *const choice[]={"Play / pause","Next track","Previous track"};
            Copy(out->hint,sizeof(out->hint),choice[s->dashboard.selection%3U]);}break;
    case UI_SYSTEM:Copy(out->line,sizeof(out->line),"Hold ENTER to open");break;
    default:break;
    }
}
