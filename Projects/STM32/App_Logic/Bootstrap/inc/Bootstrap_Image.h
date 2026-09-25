#ifndef NOODOE_BOOTSTRAP_IMAGE_H
#define NOODOE_BOOTSTRAP_IMAGE_H
#include <stdint.h>
/* Canonical APP-only source for the recovery verifier. Read calls are bounded
 * to4096bytes. A backwards seek restarts the streaming inflater; no external
 * RAM, file, radio or allocator is required. Returns0 on success. */
int BootstrapImage_Read(void *context,uint32_t offset,void *data,uint32_t bytes);
void BootstrapImage_Reset(void);
#endif
