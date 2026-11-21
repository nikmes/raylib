// PRX boilerplate for psp-tests/samples/core/input_gamepad.
// main.c stays portable; this is the only file that knows it is a PSP module.

#include <pspkernel.h>

PSP_MODULE_INFO("input_gamepad", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
