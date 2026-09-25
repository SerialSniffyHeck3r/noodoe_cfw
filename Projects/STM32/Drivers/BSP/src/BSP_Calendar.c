#include "BSP_Calendar.h"

/* Gregorian century exception:2100 is common,2000/2400 are leap years. */
uint32_t BSP_Calendar_IsLeapYear(uint32_t y)
{return y>=1U&&y<=9999U&&y%4U==0U&&(y%100U!=0U||y%400U==0U);}

/* Month lengths are used by both RTC validation and upper calendar logic. */
uint32_t BSP_Calendar_DaysInMonth(uint32_t y,uint32_t m)
{
    static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(y<1U||y>9999U||m<1U||m>12U)return 0U;
    return days[m-1U]+(m==2U&&BSP_Calendar_IsLeapYear(y));
}

/* Zero-based civil days before January1; y=10000 is the exclusive limit. */
static uint32_t BeforeYear(uint32_t y)
{--y;return 365U*y+y/4U-y/100U+y/400U;}

/* Caller validates the month/day. At most11 bounded iterations, no timers. */
static uint32_t Ordinal(uint32_t y,uint32_t m,uint32_t d)
{
    uint32_t value=BeforeYear(y)+d-1U;
    for(uint32_t n=1;n<m;++n)value+=BSP_Calendar_DaysInMonth(y,n);
    return value;
}

/* 0001-01-01 is Monday in the proleptic Gregorian calendar. */
uint32_t BSP_Calendar_Weekday(uint32_t y,uint32_t m,uint32_t d)
{
    uint32_t limit=BSP_Calendar_DaysInMonth(y,m);
    if(!limit||d<1U||d>limit)return 0U;
    return Ordinal(y,m,d)%7U+1U;
}

/* This does not accept a wrong date merely because weekday is in1..7. */
uint32_t BSP_Calendar_Normalize(BSP_CalendarDateTime *t)
{
    if(!t||t->hour>23U||t->minute>59U||t->second>59U)return 0U;
    uint32_t weekday=BSP_Calendar_Weekday(t->year,t->month,t->day);
    if(!weekday)return 0U;
    t->weekday=weekday;return 1U;
}

/* Use64-bit seconds, not2038-limited time_t. Bounds are checked before adding
 * even INT64_MIN/MAX, so neither overflow nor an implicit wrap can occur. */
uint32_t BSP_Calendar_AddSeconds(const BSP_CalendarDateTime *in,int64_t delta,BSP_CalendarDateTime *out)
{
    if(!in||!out)return 0U;
    BSP_CalendarDateTime next=*in;if(!BSP_Calendar_Normalize(&next))return 0U;
    int64_t base=(int64_t)Ordinal(next.year,next.month,next.day)*86400+
        next.hour*3600U+next.minute*60U+next.second;
    const int64_t maximum=(int64_t)3652059*86400-1;
    if(delta < -base || delta > maximum-base)return 0U;
    uint64_t total=(uint64_t)(base+delta);uint32_t day=(uint32_t)(total/86400U);
    uint32_t tod=(uint32_t)(total%86400U),lo=1U,hi=10000U;
    /* Binary search has at most14 steps across the supported year range. */
    while(lo+1U<hi){uint32_t mid=(lo+hi)/2U;if(BeforeYear(mid)<=day)lo=mid;else hi=mid;}
    next.year=lo;next.month=1U;uint32_t left=day-BeforeYear(lo);
    while(left>=BSP_Calendar_DaysInMonth(lo,next.month)){
        left-=BSP_Calendar_DaysInMonth(lo,next.month);++next.month;
    }
    next.day=left+1U;next.weekday=day%7U+1U;
    next.hour=tod/3600U;next.minute=tod%3600U/60U;next.second=tod%60U;
    *out=next;return 1U;
}

/* Days retain time-of-day;32-bit day inputs cannot overflow the64-bit product. */
uint32_t BSP_Calendar_AddDays(const BSP_CalendarDateTime *in,int32_t days,BSP_CalendarDateTime *out)
{return BSP_Calendar_AddSeconds(in,(int64_t)days*86400,out);}
