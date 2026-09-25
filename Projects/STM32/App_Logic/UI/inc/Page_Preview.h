#ifndef PAGE_PREVIEW_H
#define PAGE_PREVIEW_H
#include "Dashboard_Pages.h"
/* Development-only SRAM protocol. Write request fields first, request_id last.
 * version2: flags2=freeze at120ms, flags4=UP direction. Data comes only
 * from Development_Data or the live producer; old flags1 is rejected. No real
 * events, state, settings, radio or trip counters are mutated by a preview. */
typedef struct {uint32_t magic,version,request_id,card,selection,flags,ttl_ms,
    ack_id,result,active_id,expires_ms,now_ms;} PagePreviewMailbox;
extern volatile PagePreviewMailbox g_page_preview;
void PagePreview_Init(void);
void PagePreview_Cancel(void);
uint32_t PagePreview_Apply(const UiState *state,const UiDashboardPresentation *shell,
    const DashboardPageFacts *facts,DashboardPage *page,uint32_t now);
#endif
