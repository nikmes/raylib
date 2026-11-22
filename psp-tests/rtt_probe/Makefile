# rtt_probe - render-to-texture gate (sprint 065).
#
#   make MODE=direct
#   make MODE=rtt
#   make MODE=readback
#   make MODE=depth
#   make MODE=churn
#   make MODE=size        # 300x200 target, padded

BUILD_PRX = 1
TARGET = rtt_probe
OBJS = main.o

MODE ?= rtt

CFLAGS = -Wall -O2

ifeq ($(MODE),direct)
    CFLAGS += -DMODE_DIRECT
else ifeq ($(MODE),rtt)
    CFLAGS += -DMODE_RTT
else ifeq ($(MODE),readback)
    CFLAGS += -DMODE_READBACK
else ifeq ($(MODE),depth)
    CFLAGS += -DMODE_DEPTH
else ifeq ($(MODE),churn)
    CFLAGS += -DMODE_CHURN
else ifeq ($(MODE),size)
    CFLAGS += -DMODE_RTT -DTARGET_W=300 -DTARGET_H=200
else
    $(error MODE must be direct, rtt, readback, depth, churn or size)
endif

LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = rtt probe
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
