# gu_state_probe - b37 rendering-guide gates.
#
#   make MODE=depthmask
#   make MODE=clipplanes
#   make MODE=dither
#
# PSPGU_LIB links the worktree's driver archive directly, so the A/B does not depend on
# whichever libGL.a another session last installed in $PSPDEV.
#
#   make MODE=clipplanes PSPGU_LIB=../../tools/pspgu/libGL.a

BUILD_PRX = 1
MODE ?= depthmask
TARGET = gu_state_probe_$(MODE)
OBJS = main.o

CFLAGS = -Wall -O2
ifeq ($(MODE),depthmask)
    CFLAGS += -DMODE_DEPTHMASK
else ifeq ($(MODE),clipplanes)
    CFLAGS += -DMODE_CLIPPLANES
else ifeq ($(MODE),dither)
    CFLAGS += -DMODE_DITHER
else
    $(error MODE must be depthmask, clipplanes or dither)
endif

PSPGU_LIB ?= ../../tools/pspgu/libGL.a
LIBS = -lraylib -lpng -lz -lglut -lGLU $(PSPGU_LIB) -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = gu state probe (b37)
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
