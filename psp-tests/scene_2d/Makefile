# scene_2d - sprint 018: the Phase 7 workload corpus.
#
#   make MODE=text LINES=1
#   make MODE=text LINES=8
#   make MODE=text LINES=24
#   make MODE=tiles [SCROLL=1]
#   make MODE=particles COUNT=500
#   make MODE=particles COUNT=2000
#   make MODE=shapes COUNT=1000
#   make MODE=mixed
#
# Counters are the bunnymark --wrap harness: one copy of the wrap symbols, so every
# test in this tree counts at the same GL boundary.

BUILD_PRX = 1
TARGET = scene_2d
OBJS = main.o psp_counters.o

MODE ?= text
FRAMES ?= 300
COUNT ?= 2000
LINES ?= 8
BUDGET_FLOOR_KB ?= 128

CFLAGS = -Wall -I../bunnymark -DSCENE_FRAMES=$(FRAMES) -DSCENE_COUNT=$(COUNT) \
         -DSCENE_LINES=$(LINES) -DBUDGET_FLOOR_KB=$(BUDGET_FLOOR_KB)

ifeq ($(MODE),text)
    CFLAGS += -DMODE_TEXT
else ifeq ($(MODE),tiles)
    CFLAGS += -DMODE_TILES
else ifeq ($(MODE),particles)
    CFLAGS += -DMODE_PARTICLES
else ifeq ($(MODE),shapes)
    CFLAGS += -DMODE_SHAPES
else ifeq ($(MODE),mixed)
    CFLAGS += -DMODE_MIXED
else
    $(error MODE must be text, tiles, particles, shapes or mixed)
endif

ifdef SCROLL
    CFLAGS += -DSCROLL
endif

WRAP ?= 1
ifeq ($(WRAP),1)
    CFLAGS += -DWRAP_COUNTERS
    LDFLAGS += -Wl,--wrap=glBegin -Wl,--wrap=glEnd \
               -Wl,--wrap=glVertex2f -Wl,--wrap=glVertex2i \
               -Wl,--wrap=glBindTexture \
               -Wl,--wrap=glDrawArrays -Wl,--wrap=glDrawElements \
               -Wl,--wrap=eglSwapBuffers
endif

# -lpspgu: pspgu talks to the GE through GU, where pspGL used libpspge.
LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = scene 2d (raylib 6.5-dev)
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# Keep this rule after the include: build.mak defines the default goal.
psp_counters.o: ../bunnymark/psp_counters.c ../bunnymark/psp_counters.h
	$(CC) $(CFLAGS) -I../bunnymark -c $< -o $@
