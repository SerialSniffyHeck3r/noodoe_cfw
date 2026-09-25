#ifndef BOOTSTRAP_RESOURCES_H
#define BOOTSTRAP_RESOURCES_H
#include "Bootstrap_Storage.h"
uint32_t BootstrapResources_Content(BootstrapStorage *);
uint32_t BootstrapResources_Select(BootstrapStorage *);
uint32_t BootstrapResources_Prepare(BootstrapStorage *);
uint32_t BootstrapResources_Audit(BootstrapStorage *);
uint32_t BootstrapResources_Write(BootstrapStorage *);
/* Actual work span for the current resource phase. In particular, checking
 * the old whole container is 1MiB and final slot readback is 512KiB, not FAT. */
static inline uint32_t BootstrapResources_ProgressTotal(const BootstrapStorage *s)
{
 if(s->state==BS_RESEED_CHECK)return 1048576U;
 if(s->state==BS_WRITING&&s->phase==0)return sizeof(s->metadata);
 if(s->state==BS_AUDITING&&s->phase==3)return s->resource_total;
 return s->bytes;
}
#endif
