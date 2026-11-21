# bunnymark with counters - sprint 003.
#
# Counters are interposed at link time with --wrap, so src/ is untouched:
#   make PSP_LARGE_MEMORY=0 BUNNY_COUNT=1000     # wrapped, counts
#   make PSP_LARGE_MEMORY=0 BUNNY_COUNT=1000 WRAP=0   # unwrapped, cost reference

BUILD_PRX = 1
TARGET = bunnymark
OBJS = main.o psp_counters.o

BUNNY_COUNT ?= 1000
FRAMES ?= 600
TARGET_FPS ?= 60
SPRITE_SCALE ?= 1

CFLAGS = -Wall -DBUNNY_COUNT=$(BUNNY_COUNT) -DTOTAL_FRAMES=$(FRAMES) \
         -DTARGET_FPS=$(TARGET_FPS) -DSPRITE_SCALE=$(SPRITE_SCALE)

# Profiling arm: needs the fork's pspGL built with PSPGL_PROFILE=1.
PSPGL_PROFILE_APP ?= 0
ifeq ($(PSPGL_PROFILE_APP),1)
    CFLAGS += -DPSPGL_PROFILE_APP -I/home/psp/ref/pspgl-themiriron2
endif

# T5 mutation: draw the frame without the bunnies.
ifdef SKIP_DRAW
    CFLAGS += -DSKIP_DRAW
endif

# Sprint 010 probe: sprite texture upload format and blending, app-side only.
TEX_FMT ?= 8888
ifeq ($(TEX_FMT),5650)
    CFLAGS += -DTEX_FMT_SEL=5650
else ifeq ($(TEX_FMT),5551)
    CFLAGS += -DTEX_FMT_SEL=5551
else
    CFLAGS += -DTEX_FMT_SEL=8888
endif

ifdef BLEND_OFF
    CFLAGS += -DBLEND_OFF
endif

# Sprint 008: rotate every second sprite, i.e. force a sprite/triangle mode switch
# per sprite pair. Measures what a mixed scene costs instead of guessing.
ifdef ROTATE_ODD
    CFLAGS += -DROTATE_ODD
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

LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = bunnymark counters (raylib 6.5-dev)
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
