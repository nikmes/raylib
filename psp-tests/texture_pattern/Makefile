# Sprint 012 T1: known-pattern textures, so a wrong texel is a wrong colour in a known
# place instead of an impression. No counters, no wrap: this measures the picture only.
BUILD_PRX = 1
TARGET = texture_pattern
OBJS = main.o

CFLAGS = -Wall

# The 33x17 checkerboard is off by default (pspGL hangs on it); enable it for the NPOT
# test. Without this line "make PATTERN_NPOT=1" silently built the default scene.
ifdef PATTERN_NPOT
    CFLAGS += -DPATTERN_NPOT
endif

# -lpspgu: the pspgu driver reaches the GE through GU (pspGL used libpspge directly)
LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = texture pattern (raylib 6.5-dev)
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
