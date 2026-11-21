# Shared build for the Phase 13 sample ports.
#
# Each sample directory sets TARGET and includes this file:
#     TARGET = basic_window
#     include ../../Makefile.inc
#
# The link line is the one psp-tests/basic_shapes uses: pspgu reaches the GE
# through GU, so -lpspgu is required even though the app only calls raylib.

BUILD_PRX = 1
OBJS ?= main.o

# -I$(CURDIR)/.. finds shared headers in the category directory (currently
# shapes/raygui.h). build.mak prepends its own include directories.
CFLAGS = -Wall -I$(CURDIR)/.. -DPLATFORM_PSP -DGRAPHICS_API_OPENGL_11

LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP

# PSP-1000: 32 MB. Never request the 40 MB partition the PSPSDK default asks for.
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
