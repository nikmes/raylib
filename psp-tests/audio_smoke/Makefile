# P9-S2 audio smoke test: generated 1 kHz tone through raylib's public audio API.
#
# No asset and no Memory Stick I/O: the sine is generated at load, then played
# through the silent-then-real PSP backend.

BUILD_PRX = 1
TARGET = audio_smoke
OBJS = main.o

CFLAGS = -Wall

# Runtime libs must be appended after build.mak so its SDK stubs are linked first.
# -lpspaudio and -latomic are not in build.mak; -lpthread is needed by miniaudio.
LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = audio smoke (P9-S2)

# PSP-1000: 32 MB. Never request the 40 MB partition the PSPSDK default asks for.
PSP_LARGE_MEMORY = 0
PSP_HEAP_SIZE_KB = 1024

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
LIBS += -lpspaudio -lpthread -latomic
