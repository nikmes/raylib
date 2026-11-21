# Sprint 015 T3: VRAM allocator churn probe.
#
#   make        # builds against whatever libraylib.a is staged
#
# The app calls pspguVramReport(), which libGL.a exports; no counters, no wrap.

BUILD_PRX = 1
TARGET = texture_churn
OBJS = main.o

CFLAGS = -Wall

ifdef CYCLES
    CFLAGS += -DCHURN_CYCLES=$(CYCLES)
endif

ifdef CHURN_TRACE
    CFLAGS += -DCHURN_TRACE
endif

LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = texture churn (raylib 6.5-dev)
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
