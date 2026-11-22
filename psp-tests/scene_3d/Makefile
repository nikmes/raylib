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
FAR ?= 0
CAMREL ?= 0
JITTER ?= 0
INSTANCES ?= 1
LIGHT ?= none
FOG ?= none
TEXENV ?= modulate

ifeq ($(JITTER),1)
    FRAMES := 16
endif

CFLAGS = -Wall -O2 -I../bunnymark -DSCENE_FRAMES=$(FRAMES) -DSCENE_NAME="\"$(SCENE)\"" \
         -DFAR_VALUE=$(FAR) -DINSTANCES=$(INSTANCES)

ifeq ($(JITTER),1)
    CFLAGS += -DJITTER_ARM
endif
ifeq ($(CAMREL),1)
    CFLAGS += -DCAMREL_ARM
endif
ifeq ($(LIGHT),ge)
    CFLAGS += -DLIGHT_GE
else ifeq ($(LIGHT),bake)
    CFLAGS += -DLIGHT_BAKE
else ifneq ($(LIGHT),none)
    $(error LIGHT must be ge, bake or none)
endif
ifeq ($(FOG),linear)
    CFLAGS += -DFOG_LINEAR
else ifneq ($(FOG),none)
    $(error FOG must be linear or none)
endif
ifeq ($(TEXENV),modulate)
    CFLAGS += -DTEXENV_MODULATE
else ifeq ($(TEXENV),replace)
    CFLAGS += -DTEXENV_REPLACE
else ifeq ($(TEXENV),decal)
    CFLAGS += -DTEXENV_DECAL
else ifeq ($(TEXENV),add)
    CFLAGS += -DTEXENV_ADD
else
    $(error TEXENV must be modulate, replace, decal or add)
endif

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
