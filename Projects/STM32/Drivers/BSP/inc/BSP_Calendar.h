#ifndef BSP_CALENDAR_H
#define BSP_CALENDAR_H
#include <stdint.h>
/* Pure proleptic Gregorian arithmetic, years1..9999, Monday1..Sunday7.
 * No hardware/RTOS/heap/timezone/DST access. Seconds are0..59; no leap seconds.
 * RTC's two-digit year storage is a separate2000..2099 adapter constraint. */
typedef struct {uint32_t year,month,day,weekday,hour,minute,second;} BSP_CalendarDateTime;
uint32_t BSP_Calendar_IsLeapYear(uint32_t year);
/* Invalid year/month returns0; dates are never silently clamped. */
uint32_t BSP_Calendar_DaysInMonth(uint32_t year,uint32_t month);
/* Invalid date returns0. Weekday is derived, never supplied by a user. */
uint32_t BSP_Calendar_Weekday(uint32_t year,uint32_t month,uint32_t day);
/* Validate civil date and24-hour time; ignores input weekday and recomputes it.
 * Failure leaves the caller's struct unchanged. Success returns1. */
uint32_t BSP_Calendar_Normalize(BSP_CalendarDateTime *time);
/* Signed, bounded arithmetic through midnight/month/year/century transitions.
 * Failure (NULL, invalid input, outside1..9999) leaves output unchanged.
 * Input/output may alias; output weekday is always derived. */
uint32_t BSP_Calendar_AddSeconds(const BSP_CalendarDateTime *in,int64_t seconds,BSP_CalendarDateTime *out);
uint32_t BSP_Calendar_AddDays(const BSP_CalendarDateTime *in,int32_t days,BSP_CalendarDateTime *out);
#endif
