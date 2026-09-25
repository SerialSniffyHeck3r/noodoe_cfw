#include "Ui_Number.h"
#include "SpeedHome_Model.h"
#include <string.h>

/* Integer distance formatter: at most six digits, without leading zeroes.
 * Keep unknown/overflow markers; the fixed view box owns right alignment. */
static void Odometer(char out[7],uint32_t km,uint32_t miles,uint32_t valid)
{
    uint64_t value=miles?((uint64_t)km*1000000ULL/1609344ULL):km;
    out[6]=0;
    if(!valid||value>999999U){memset(out,valid?'#':'-',6U);return;}
    for(int i=5;i>=0;--i){out[i]=(char)('0'+value%10U);value/=10U;}
    UiNumber_RemoveLeadingZeros(out);
}

/* Solid arc color follows relative speed. Interpolate channels at0/60/85/100%
 * instead of requesting unsupported EVE gradient/layer operations. */
static uint32_t Color(uint32_t value)
{
    const uint32_t colors[]={0x38A8FFU,0xFFB000U,0xFF4040U};
    uint32_t a,b,t,span;
    if(value<6000U){a=colors[0];b=colors[1];t=value;span=6000U;}
    else if(value<8500U){a=colors[1];b=colors[2];t=value-6000U;span=2500U;}
    else return colors[2];
    uint32_t rgb=0U;
    for(uint32_t shift=0;shift<=16U;shift+=8U){
        int32_t ca=(a>>shift)&255U,cb=(b>>shift)&255U;
        uint32_t c=(uint32_t)(ca+(cb-ca)*(int32_t)t/(int32_t)span);rgb|=c<<shift;
    }
    return rgb;
}

void SpeedHome_OverrideArc(SpeedHomeModel *m,uint32_t value,uint32_t now)
{
    if(!m)return;
    if(value>10000U)value=10000U;
    m->arc_value=m->from=m->target=value;m->transition_ms=now;
    m->arc_color=Color(value);m->arc_override=1U;
}

void SpeedHome_RecordDisplayed(SpeedHomeModel *m)
{
    if(m->session_valid&&m->speed_valid&&!m->arc_override&&m->arc_value>=m->peak_ratio){
        m->peak_ratio=m->arc_value;m->peak_color=m->arc_color;
    }
}

/* Model setup has no allocation and never starts a synthetic speed sweep. */
void SpeedHome_InitModel(SpeedHomeModel *m,uint32_t now,uint32_t max)
{
    memset(m,0,sizeof(*m));m->max_kph=max>=20U&&max<=400U?max:200U;
    m->last_now=m->transition_ms=now;m->arc_color=0x65717CU;
    memcpy(m->clock,"--:--",6);memcpy(m->odo,"------",7);memcpy(m->unit,"km",3);
}

/* Called by the owner with a fresh cached snapshot. Smooth the angle only;
 * validity changes immediately. Retarget from the current interpolated angle
 * so repeated100ms UART packets cannot restart from an old endpoint. */
void SpeedHome_UpdateModel(SpeedHomeModel *m,const SpeedHomeInput *in)
{
    if(!m||!in)return;
    m->arc_override=0;
    if(m->session_generation!=in->session_generation||!in->session_valid){
        m->peak_ratio=0;m->peak_color=Color(0);
        m->session_generation=in->session_generation;
    }
    uint32_t elapsed=in->now_ms-m->transition_ms;
    if(elapsed>=SPEED_HOME_INTERPOLATION_MS)m->arc_value=m->target;
    else m->arc_value=(uint32_t)((int32_t)m->from+
        ((int32_t)m->target-(int32_t)m->from)*(int32_t)elapsed/(int32_t)SPEED_HOME_INTERPOLATION_MS);
    uint32_t valid=(in->valid_mask&1U)!=0U;
    if(valid){
        uint32_t value=in->speed_kph>=m->max_kph?10000U:in->speed_kph*10000U/m->max_kph;
        if(value!=m->target){m->from=m->arc_value;m->target=value;m->transition_ms=in->now_ms;}
    }else{
        /* Freeze at last drawn angle immediately when RX becomes stale. */
        m->from=m->target=m->arc_value;m->transition_ms=in->now_ms;
    }
    m->speed_valid=valid;m->odo_valid=(in->valid_mask&2U)!=0U;m->units=!!in->units;
    m->arc_color=valid?Color(m->arc_value):0x65717CU;
    Odometer(m->odo,in->odometer_km,m->units,m->odo_valid);
    memcpy(m->unit,m->units?"mile":"km\0\0",5);
    m->clock_valid=in->clock_valid&&in->hour<24U&&in->minute<60U;
    if(m->clock_valid){
        uint32_t hour=in->hour; /* Input is already converted to the selected local timezone. */
        m->clock[0]=(char)('0'+hour/10U);m->clock[1]=(char)('0'+hour%10U);m->clock[2]=':';
        m->clock[3]=(char)('0'+in->minute/10U);m->clock[4]=(char)('0'+in->minute%10U);m->clock[5]=0;
    }else memcpy(m->clock,"--:--",6);
    m->session_valid=in->session_valid;
    m->average_ratio=in->session_average_kph10>=m->max_kph*10U?10000U:in->session_average_kph10*1000U/m->max_kph;
    m->last_now=in->now_ms;m->initialized=1U;
}
