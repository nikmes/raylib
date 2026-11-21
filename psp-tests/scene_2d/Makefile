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
OFFSCREEN ?= 50
LINES ?= 8
BUDGET_FLOOR_KB ?= 128

# Sprint 019: text implementation arm and the font-atlas probe.
TEXT_IMPL ?= api
FONT_PATH ?= host0:/fonts/KAISG.ttf
FONT_SIZE ?= 32
FONT_SIZE2 ?= 12
FONT_GLYPHS ?= 95
FONT_ORDER ?= default

# Sprint 020: oversized texture and wide tile atlas.
WIDE ?= 1024
HIGH ?= 128
ROTATE ?= 0
REF ?= 0
ATLAS_WIDE ?= 0
ATLAS_REF ?= 0

CFLAGS = -Wall -O2 -I../bunnymark -DSCENE_FRAMES=$(FRAMES) -DSCENE_COUNT=$(COUNT) \
         -DSCENE_OFFSCREEN_PCT=$(OFFSCREEN) \
         -DSCENE_LINES=$(LINES) -DBUDGET_FLOOR_KB=$(BUDGET_FLOOR_KB) \
         -DFONT_PATH="\"$(FONT_PATH)\"" -DFONT_SIZE=$(FONT_SIZE) -DFONT_SIZE2=$(FONT_SIZE2) -DFONT_GLYPHS=$(FONT_GLYPHS)

ifeq ($(TEXT_IMPL),api)
    CFLAGS += -DTEXT_IMPL_API
else ifeq ($(TEXT_IMPL),walk)
    CFLAGS += -DTEXT_IMPL_WALK
else ifeq ($(TEXT_IMPL),direct)
    # The app-side run-batched arm uses the same inline batcher as the library.
    # ../../src comes first so the header with the PSP batch section wins over the
    # installed copy, which predates it.
    CFLAGS += -DTEXT_IMPL_DIRECT -DPLATFORM_PSP -DGRAPHICS_API_OPENGL_11 -DPSP_BATCH=1 -I../../src
else
    $(error TEXT_IMPL must be api, walk or direct)
endif

ifeq ($(FONT_ORDER),reverse)
    CFLAGS += -DFONT_ORDER_REVERSE
endif

ifeq ($(ATLAS_WIDE),1)
    CFLAGS += -DATLAS_WIDE
endif
ifeq ($(ATLAS_REF),1)
    CFLAGS += -DATLAS_REF
endif
ifeq ($(REF),1)
    CFLAGS += -DOVERSIZE_REF
endif
CFLAGS += -DOVERSIZE_W=$(WIDE) -DOVERSIZE_H=$(HIGH) -DOVERSIZE_ROTATE=$(ROTATE)

ifeq ($(MODE),text)
    CFLAGS += -DMODE_TEXT
else ifeq ($(MODE),tiles)
    CFLAGS += -DMODE_TILES
else ifeq ($(MODE),particles)
    CFLAGS += -DMODE_PARTICLES
else ifeq ($(MODE),shapes)
    CFLAGS += -DMODE_SHAPES
else ifeq ($(MODE),sprites)
    CFLAGS += -DMODE_SPRITES
else ifeq ($(MODE),mixed)
    CFLAGS += -DMODE_MIXED
else ifeq ($(MODE),font)
    CFLAGS += -DMODE_FONT
else ifeq ($(MODE),oversize)
    CFLAGS += -DMODE_OVERSIZE
else
    $(error MODE must be text, tiles, particles, shapes, mixed, font or oversize)
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
