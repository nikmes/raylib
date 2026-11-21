# Sprint 017 T2/T3: measurement-gated texture paths, raw GU.
#
#   make MODE=clut
#   make MODE=mipmap MIPS=0
#   make MODE=mipmap MIPS=1

BUILD_PRX = 1
TARGET = gated_features
OBJS = main.o

MODE ?= clut
CFLAGS = -Wall -O2
ifeq ($(MODE),clut)
    CFLAGS += -DMODE_CLUT
else ifeq ($(MODE),mipmap)
    CFLAGS += -DMODE_MIPMAP
    ifeq ($(MIPS),1)
        CFLAGS += -DMIPS_ON
    endif
else
    $(error MODE must be clut or mipmap)
endif

LIBS = -lpspgu -lpspge -lpspdisplay -lpspvfpu -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = gated texture features (raylib 6.5-dev)
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
