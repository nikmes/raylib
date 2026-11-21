// PRX boilerplate for psp-tests/pad_api (sprint 041, P8-S1).
// This is the only file in the sample that knows it is a PSP module; main.c stays
// portable and builds against the plain raylib gamepad API.

#include <pspkernel.h>

PSP_MODULE_INFO("pad_api", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
