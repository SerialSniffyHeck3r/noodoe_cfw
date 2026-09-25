#ifndef GRAPHICS_EVE_TRANSPORT_H
#define GRAPHICS_EVE_TRANSPORT_H
#include "bsp_eve_bus.h"
typedef struct {
    uint32_t magic,version,bursts,bytes,space_reads,waits,timeouts,faults;
    uint32_t last_space,min_space,max_burst,active,credit;
} GraphicsEveTransportDiagnostics;
extern volatile GraphicsEveTransportDiagnostics g_eve_transport;
BSP_EVE_Status GraphicsEveTransport_Select(void);
BSP_EVE_Status GraphicsEveTransport_Send(const void *data,uint32_t length);
BSP_EVE_Status GraphicsEveTransport_Receive(void *data,uint32_t length);
BSP_EVE_Status GraphicsEveTransport_Deselect(void);
#endif
