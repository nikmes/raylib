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
# -O2 on purpose: without it the samples build at -O0 while libraylib is -O2, so a demo-side
# loop is measured unoptimised against optimised library code. Sprint 100 found this while
# comparing raylib's DrawTexturePro (library, -O2) with the same sequence emitted by a demo
# (-O0): 45.4 vs 79.9 ms at 7,000 sprites, which made the arm useless as an instrument.
CFLAGS = -Wall -O2 -I$(CURDIR)/.. -I$(CURDIR)/../.. -DPLATFORM_PSP -DGRAPHICS_API_OPENGL_11 -DPSP_BATCH=1

LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP

# PSP-1000: 32 MB. Never request the 40 MB partition the PSPSDK default asks for.
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# Runtime libs must come after build.mak so its SDK stubs are linked first.
LIBS += -lpspaudio -lpthread -latomic
