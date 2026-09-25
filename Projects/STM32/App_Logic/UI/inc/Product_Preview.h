#ifndef PRODUCT_PREVIEW_H
#define PRODUCT_PREVIEW_H
#include "Ui_DashboardPresentation.h"
/* Development-only SWD display mailbox. Host writes payload then request_id
 * last; waits for ack_id before another request. No button/domain/RTC/storage
 * state changes. Every preview explicitly says TEST DATA and auto-expires. */
typedef struct {
    uint32_t magic,version,request_id,footer,remaining_permille,valid,ttl_ms;
    uint32_t ack_id,active_id,result,expires_ms,last_ms;
} ProductPreviewMailbox;
extern volatile ProductPreviewMailbox g_product_preview;
void ProductPreview_Init(void);
void ProductPreview_Cancel(void);
/* Owner task only. Returns1 when replacing only the presentation with test data.
 * Distance modes: OIL fraction. RESV:0.1km test distance.
 * OIL:0.1h test elapsed time; BELT/SERV:test elapsed days.
 * Version2 retains the48byte layout; retired OBD IDs0x100..0x107 are rejected.
 * UINT32_MAX cancels; ttl1000..180000, ratio<=1000. DATA_DEBUG=0 disables
 * preview execution and acknowledges requests as unavailable. */
uint32_t ProductPreview_Apply(const UiState *state,uint32_t now,UiDashboardPresentation *out);
#endif
