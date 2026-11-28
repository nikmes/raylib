TARGET = psp_2d_showcase
FRAMES ?= 0
VSYNC ?= 1
SPRITES ?= 1300
STARS ?= 250

OBJS = main.o psp_counters.o
include ../../Makefile.inc

# The shared file assigns CFLAGS and LDFLAGS, so the demo's own switches come after it.
CFLAGS += -DSHOWCASE_FRAMES=$(FRAMES) -DSHOWCASE_VSYNC=$(VSYNC) -DSPRITES=$(SPRITES) -DSTARS=$(STARS) -I../../../bunnymark
CFLAGS += -DWRAP_COUNTERS
LDFLAGS += -Wl,--wrap=glBegin -Wl,--wrap=glEnd \
           -Wl,--wrap=glVertex2f -Wl,--wrap=glVertex2i \
           -Wl,--wrap=glBindTexture \
           -Wl,--wrap=glDrawArrays -Wl,--wrap=glDrawElements \
           -Wl,--wrap=eglSwapBuffers
