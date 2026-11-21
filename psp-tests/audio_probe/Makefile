# Pure pspsdk: no raylib, no libGL, so the probe is independent of which driver is installed.
BUILD_PRX = 1
TARGET = audio_probe
OBJS = main.o
CFLAGS = -Wall
LIBS = -lpspaudio -lm
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = audio probe
PSP_LARGE_MEMORY = 0
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
