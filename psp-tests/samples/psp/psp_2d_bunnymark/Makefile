TARGET = psp_2d_bunnymark
FRAMES ?= 200
VSYNC ?= 1
COUNT ?= 7000
MODE ?= 1
SCALE ?= 32

OBJS = main.o psp_counters.o
include ../../Makefile.inc

# The shared file assigns CFLAGS and LDFLAGS, so the demo's own switches come after it.
CFLAGS += -DSHOWCASE_FRAMES=$(FRAMES) -DVSYNC=$(VSYNC) -DCOUNT=$(COUNT) -DMODE=$(MODE) \
          -DSCALE=$(SCALE).0f -I../../../bunnymark -I$(CURDIR)/../../../../src
CFLAGS += -DWRAP_COUNTERS
LDFLAGS += -Wl,--wrap=glBegin -Wl,--wrap=glEnd \
           -Wl,--wrap=glVertex2f -Wl,--wrap=glVertex2i \
           -Wl,--wrap=glBindTexture \
           -Wl,--wrap=glDrawArrays -Wl,--wrap=glDrawElements \
           -Wl,--wrap=eglSwapBuffers
