TARGET = psp_showcase
FRAMES ?= 0

OBJS = main.o psp_counters.o
include ../../Makefile.inc

# The shared file assigns CFLAGS and LDFLAGS, so the demo's own switches come after it.
CFLAGS += -DSHOWCASE_FRAMES=$(FRAMES) -I../../../bunnymark -DWRAP_COUNTERS
ifdef GRID
CFLAGS += -DGRID=$(GRID)
endif
LDFLAGS += -Wl,--wrap=glBegin -Wl,--wrap=glEnd \
           -Wl,--wrap=glVertex2f -Wl,--wrap=glVertex2i \
           -Wl,--wrap=glBindTexture \
           -Wl,--wrap=glDrawArrays -Wl,--wrap=glDrawElements \
           -Wl,--wrap=eglSwapBuffers
