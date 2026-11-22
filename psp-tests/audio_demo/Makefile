# P9-S8 listening demo: one clean, loud MP3, beginning to end, looped twice.
#
#   make                -> audio_demo.prx (stage host0:/country.mp3)
#
# Runtime libs are appended after build.mak so its SDK stubs are linked first.

BUILD_PRX = 1
TARGET = audio_demo
OBJS = main.o

DEMO_MUSIC ?= /country.mp3
DEMO_LOOPS ?= 2

CFLAGS = -Wall -DDEMO_MUSIC_PATH=\"$(DEMO_MUSIC)\" -DDEMO_LOOPS=$(DEMO_LOOPS)

LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = audio demo (P9-S8)

# PSP-1000: 32 MB. Never request the 40 MB partition the PSPSDK default asks for.
PSP_LARGE_MEMORY = 0
PSP_HEAP_SIZE_KB = 2048

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
LIBS += -lpspaudio -lpthread -latomic
