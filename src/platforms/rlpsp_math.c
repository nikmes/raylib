/*
 * rlpsp_math.c - VFPU context lifecycle for the internal helper layer (P11-S12)
 *
 * The math helpers are header-only; this TU owns the single per-process context so
 * multiple including TUs do not each create one. Call rlpsp_vfpu_init() once per
 * thread before the first helper; it also sets the calling thread's VFPU attribute
 * (pspvfpu.h) and reserves matrices 6-7, leaving GUM's 0-3 alone.
 */

#include "../rlpsp_math.h"

#if RLPSP_VFPU || RLPSP_VFPU_SINCOS
static struct pspvfpu_context *rlpspVfpuContext = NULL;

void rlpsp_vfpu_init(void)
{
    if (rlpspVfpuContext == NULL)
    {
        rlpspVfpuContext = pspvfpu_initcontext();
        if (rlpspVfpuContext != NULL) pspvfpu_use_matrices(rlpspVfpuContext, VMAT6 | VMAT7, 0);
    }
}

void rlpsp_vfpu_shutdown(void)
{
    if (rlpspVfpuContext != NULL)
    {
        pspvfpu_deletecontext(rlpspVfpuContext);
        rlpspVfpuContext = NULL;
    }
}
#endif
