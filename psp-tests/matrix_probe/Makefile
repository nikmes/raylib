# matrix_probe - sprint 070 (P10-S8): camera and matrix CPU cost.
#
#   make MODE=api                     # per-call cost of every helper (T1)
#   make MODE=calls COUNT=100         # per-call vs cached world-to-screen (T2)
#   make MODE=scene COUNT=100 [FRAMES=120]   # scene attribution (T3)
#
# MODE=scene reads the PSP_MATRIX_PROBE counters, so the library must be built with
# PSP_MATRIX_PROBE=1 for them to be present; without it the probe still times the frame
# and prints "counters off".

BUILD_PRX = 1
TARGET = matrix_probe
OBJS = main.o psp_counters.o

MODE ?= api
COUNT ?= 100
FRAMES ?= 120

CFLAGS = -Wall -O2 -I../bunnymark -DCOUNT=$(COUNT) -DFRAMES=$(FRAMES)
ifeq ($(MODE),api)
    CFLAGS += -DMODE_API
else ifeq ($(MODE),calls)
    CFLAGS += -DMODE_CALLS
else ifeq ($(MODE),scene)
    CFLAGS += -DMODE_SCENE
else
    $(error MODE must be api, calls or scene)
endif

# STAT/SPLIT needs the swap timing wrappers, same ones bunnymark uses.
PROBE ?= 1

ifeq ($(MODE),scene)
    CFLAGS += -DWRAP_COUNTERS
    ifeq ($(PROBE),1)
        CFLAGS += -DPSP_MATRIX_PROBE
    endif
    LDFLAGS += -Wl,--wrap=glBegin -Wl,--wrap=glEnd \
               -Wl,--wrap=glVertex2f -Wl,--wrap=glVertex2i \
               -Wl,--wrap=glBindTexture \
               -Wl,--wrap=glDrawArrays -Wl,--wrap=glDrawElements \
               -Wl,--wrap=eglSwapBuffers
endif

LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = matrix probe (raylib 6.5-dev)
PSP_LARGE_MEMORY = 0
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# Keep this after the include: build.mak defines the default goal.
psp_counters.o: ../bunnymark/psp_counters.c ../bunnymark/psp_counters.h
	$(CC) $(CFLAGS) -c $< -o $@

