# P9-S1 audio smoke test: raudio present + silent PSP backend.
#
# No audio asset and no sound: this proves the module links, the custom backend
# starts, and miniaudio's worker is paced rather than busy-spinning.

BUILD_PRX = 1
TARGET = audio_smoke
OBJS = main.o

CFLAGS = -Wall

# Runtime libs must be appended after build.mak so its SDK stubs are linked first.
# -lpspaudio and -latomic are not in build.mak; -lpthread is needed by miniaudio.
LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = audio smoke (P9-S1)

# PSP-1000: 32 MB. Never request the 40 MB partition the PSPSDK default asks for.
PSP_LARGE_MEMORY = 0
PSP_HEAP_SIZE_KB = 1024

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
LIBS += -lpspaudio -lpthread -latomic
