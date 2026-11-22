// PRX boilerplate for psp-tests/pad_probe (sprint 043, P8-S3).
// The raw read is PSP-specific, so main.c is PSP-only as well.

#include <pspkernel.h>

PSP_MODULE_INFO("pad_probe", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
