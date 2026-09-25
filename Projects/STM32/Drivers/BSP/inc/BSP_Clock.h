#ifndef BSP_CLOCK_H
#define BSP_CLOCK_H
#include <stdint.h>
typedef struct { uint32_t year,month,day,weekday,hour,minute,second,valid; } BSP_Clock_Time;
typedef struct { uint32_t magic,ready,error,reads,sets,alarms,backup19; BSP_Clock_Time time; } BSP_Clock_Diagnostics;
extern volatile BSP_Clock_Diagnostics g_bsp_clock;
typedef enum {
    BSP_CLOCK_OK=0U, BSP_CLOCK_NOT_READY=1U, BSP_CLOCK_ARGUMENT=2U,
    BSP_CLOCK_IO=3U, BSP_CLOCK_BUSY=4U, BSP_CLOCK_CONTEXT=5U,
    BSP_CLOCK_TIMEOUT=6U
} BSP_Clock_Status;
/* Attach retained RTC or start an unselected LSE RTC without resetting backup
 * or writing a date. ready(counter) is separate from time.valid(calendar). */
uint32_t BSP_Clock_Init(void);
/* Serialized privileged Thread calls require interrupts enabled. Read always
 * unlocks date shadows; valid=0 distinguishes an unset/invalid calendar from
 * I/O success. Failed HAL reads invalidate rather than publish partial data. */
uint32_t BSP_Clock_Read(BSP_Clock_Time *time);
/* Explicit UTC write,2000..2099. Weekday is calculated from Y/M/D.
 * Weekday input0..7 is accepted; a supplied day never overrides the calendar.
 * Pure arithmetic for1..9999 is in BSP_Calendar.h. No century storage is invented.
 * Explicit UTC write. Date and time are separate HAL transactions: later
 * failure may leave a partial change. No automatic rollback or domain reset. */
uint32_t BSP_Clock_Set(const BSP_Clock_Time *time);
/* Alarm A only. Successful install enables IRQ; failure restores entry state.
 * Pending enabled Alarm B survives shared EXTI/NVIC cleanup. */
uint32_t BSP_Clock_SetDailyAlarm(uint32_t hour,uint32_t minute,uint32_t second);
/* Returns actual BUSY/TIMEOUT/I/O status and restores entry IRQ enable state. */
uint32_t BSP_Clock_CancelAlarm(void);
#endif
