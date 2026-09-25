#ifndef DASH_TEST_TASK_H
#define DASH_TEST_TASK_H
/* Host fixture lock boundary, with no actual RTOS scheduling. */
extern unsigned mock_locks;
#define taskENTER_CRITICAL() (++mock_locks)
#define taskEXIT_CRITICAL() (--mock_locks)
#endif
