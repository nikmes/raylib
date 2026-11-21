# Deterministic render check for the PSP_BATCH arms - sprint 007 T7.
#
#   make                      # build against whatever libraylib.a is staged
#   WRAP=0 make               # same scene, no link-time counters
#
# Counters come from the bunnymark harness: one copy of the wrap symbols, so the
# two tests always count the same way.

BUILD_PRX = 1
TARGET = batch_check
OBJS = main.o psp_counters.o

CFLAGS = -Wall

WRAP ?= 1
# The counters header is needed either way; only the --wrap flags are WRAP's business
CFLAGS += -I../bunnymark
ifeq ($(WRAP),1)
    CFLAGS += -DWRAP_COUNTERS
    LDFLAGS += -Wl,--wrap=glBegin -Wl,--wrap=glEnd \
               -Wl,--wrap=glVertex2f -Wl,--wrap=glVertex2i \
               -Wl,--wrap=glBindTexture \
               -Wl,--wrap=glDrawArrays -Wl,--wrap=glDrawElements \
               -Wl,--wrap=eglSwapBuffers
endif

# Log every draw call with its vertex count and every texture bind
ifdef TRACE_DRAWS
    CFLAGS += -DTRACE_DRAWS
endif

# 2D -> 3D (DrawCube + DrawMesh) -> 2D: the ordering test for the array-draw paths
ifdef MESH_SCENE
    CFLAGS += -DMESH_SCENE
endif

# -lpspgu: the pspgu driver talks to the GE through GU, where pspGL used libpspge
# directly. The archive supplies the GL symbols; GU comes from the SDK.
LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = batch check (raylib 6.5-dev)
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# NOTE: keep this rule after the include - build.mak defines the default goal, and a
# rule placed above it would become the default target instead.
psp_counters.o: ../bunnymark/psp_counters.c ../bunnymark/psp_counters.h
	$(CC) $(CFLAGS) -c $< -o $@
