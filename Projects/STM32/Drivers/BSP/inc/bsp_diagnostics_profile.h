#ifndef BSP_DIAGNOSTICS_PROFILE_H
#define BSP_DIAGNOSTICS_PROFILE_H
#ifndef NOODOE_PRODUCT
#define NOODOE_PRODUCT 0
#endif
#ifndef NOODOE_DIAGNOSTIC
#define NOODOE_DIAGNOSTIC 0
#endif
/* Product keeps normal drivers and cached state. Electrical experiments are
 * selected explicitly by the standalone Diagnostic build; Integrated bench
 * builds retain their original laboratory APIs. No runtime escape enables
 * these experiments in a production image. */
#ifndef NOODOE_DEEP_DIAGNOSTICS
#define NOODOE_DEEP_DIAGNOSTICS (!NOODOE_PRODUCT || NOODOE_DIAGNOSTIC)
#endif
#endif
