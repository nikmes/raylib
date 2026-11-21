# Sprint 016: residency and eviction probe.
#
#   make MODE=fit
#   make MODE=over
#   make MODE=priority [PRIORITY_SWAP=1]
#   make MODE=frag
#
# Default 8888; the policy is format-agnostic.

BUILD_PRX = 1
TARGET = texture_residency
OBJS = main.o

MODE ?= fit
CFLAGS = -Wall
ifeq ($(MODE),fit)
    CFLAGS += -DMODE_FIT
else ifeq ($(MODE),over)
    CFLAGS += -DMODE_OVER
else ifeq ($(MODE),priority)
    CFLAGS += -DMODE_PRIORITY
else ifeq ($(MODE),frag)
    CFLAGS += -DMODE_FRAG
else
    $(error MODE must be fit, over, priority or frag)
endif

ifdef PRIORITY_SWAP
    CFLAGS += -DPRIORITY_SWAP
endif

LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = texture residency (raylib 6.5-dev)
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
