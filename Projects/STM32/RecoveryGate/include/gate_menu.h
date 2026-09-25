#ifndef NOODOE_GATE_MENU_H
#define NOODOE_GATE_MENU_H
#include <stdint.h>
typedef struct {uint32_t page,selection,help,down,edge_ms,released,held,hold_ms,hold_started;} GateMenu;
/* Keys: bit0 UP, bit1 O, bit2 DOWN. Navigation is edge-triggered/debounced;
 * recovery approval requires a new press AFTER opening its confirm page. */
void GateMenu_Init(GateMenu *,uint32_t keys,uint32_t now);
uint32_t GateMenu_Process(GateMenu *,uint32_t keys,uint32_t now);
typedef struct {uint32_t verified,reason,reset,fault,failed,code,log;} GateMenuFacts;
void GateBoard_Menu(const GateMenu *,const GateMenuFacts *);
#endif
