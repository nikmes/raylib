# colour_gate - P10-S13 T2.
#
# For a worktree library, override RAYLIB_LIB:
#   make RAYLIB_LIB=/home/psp/raylib-psp-p10s13/src/libraylib.a
# The default uses the installed -lraylib.

BUILD_PRX = 1
TARGET = colour_gate
OBJS = main.o

CFLAGS = -Wall -O2 -DPLATFORM_PSP -DGRAPHICS_API_OPENGL_11 -DPSP_BATCH=1

RAYLIB_LIB ?= -lraylib
LIBS = $(RAYLIB_LIB) -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = colour gate (P10-S13)
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
LIBS += -lpspaudio -lpthread -latomic
