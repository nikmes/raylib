# scene_3d - phase 10 3D corpus (sprint 060 T4).
#
#   make SCENE=flat
#   make SCENE=tex
#   make SCENE=both          (default)
#   make SCENE=grid          dedup chunker arm
#
# Counters are the bunnymark --wrap harness, one copy of the wrap symbols.

BUILD_PRX = 1
TARGET = scene_3d
OBJS = main.o psp_counters.o

SCENE ?= both
FRAMES ?= 120

CFLAGS = -Wall -O2 -I../bunnymark -DSCENE_FRAMES=$(FRAMES) -DSCENE_NAME="\"$(SCENE)\""

ifeq ($(SCENE),flat)
    CFLAGS += -DSCENE_FLAT
else ifeq ($(SCENE),tex)
    CFLAGS += -DSCENE_TEX
else ifeq ($(SCENE),both)
    CFLAGS += -DSCENE_BOTH
else ifeq ($(SCENE),grid)
    CFLAGS += -DSCENE_GRID
else
    $(error SCENE must be flat, tex, both or grid)
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

LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = scene 3d (raylib 6.5-dev)
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

psp_counters.o: ../bunnymark/psp_counters.c ../bunnymark/psp_counters.h
	$(CC) $(CFLAGS) -I../bunnymark -c $< -o $@
