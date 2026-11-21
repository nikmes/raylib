# Sprint 020 T1: full-stride 512-pixel windows, raw GU.
#
#   make
#
# Verdict -> host0:/guwin_done.txt; capture after DONE.

BUILD_PRX = 1
TARGET = gu_window
OBJS = main.o

CFLAGS = -Wall -O2

LIBS = -lpspgu -lpspge -lpspdisplay -lpspvfpu -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = GU window probe (raylib 6.5-dev)
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
