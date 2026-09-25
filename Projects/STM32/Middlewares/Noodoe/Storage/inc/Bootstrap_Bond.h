#ifndef BOOTSTRAP_BOND_H
#define BOOTSTRAP_BOND_H
#include "Bootstrap_Storage.h"
/* Storage-owner only. No keys are exposed through NDCP or diagnostics. */
typedef struct { uint32_t phase,error,address,generation; } BootstrapBond;
uint32_t BootstrapBond_Restore(BootstrapStorage *s);
uint32_t BootstrapBond_Begin(BootstrapBond *b,BootstrapStorage *s);
/* Returns true once complete, including failure; one NOR operation per call. */
uint32_t BootstrapBond_Process(BootstrapBond *b,BootstrapStorage *s);
#endif
