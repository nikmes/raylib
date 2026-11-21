# P9-S3 audio smoke test: Sound/Wave API, mixing, pitch and heap modes.
#
#   make MODE=tone  COUNT=1  FRAMES=600            # S2 control: one tone
#   make MODE=api                                  # API matrix, 1 s wave
#   make MODE=mix   COUNT=16                       # 100 ms waves
#   make MODE=mix   COUNT=32
#   make MODE=pitch PITCH=0.5                      # duration arm
#   make MODE=pitch PITCH=2.0
#   make MODE=heap  COUNT=32                       # free-memory before/after
#
# Runtime libs must be appended after build.mak so its SDK stubs are linked first.

BUILD_PRX = 1
TARGET = audio_smoke
OBJS = main.o

CFLAGS = -Wall

MODE ?= tone
COUNT ?= 16
FRAMES ?= 600
PITCH ?= 1.0

ifeq ($(MODE),tone)
    CFLAGS += -DMODE_TONE
else ifeq ($(MODE),sound)
    CFLAGS += -DMODE_SOUND
else ifeq ($(MODE),api)
    CFLAGS += -DMODE_API
else ifeq ($(MODE),mix)
    CFLAGS += -DMODE_MIX
else ifeq ($(MODE),pitch)
    CFLAGS += -DMODE_PITCH
else ifeq ($(MODE),levels)
    CFLAGS += -DMODE_LEVELS
else ifeq ($(MODE),heap)
    CFLAGS += -DMODE_HEAP
else
    $(error MODE must be tone, sound, api, mix, pitch, levels or heap)
endif

# Mix/heap use short waves so 32 sounds fit the 4 MiB heap.
ifeq ($(MODE),mix)
    ifeq ($(shell test $(COUNT) -le 16 && echo yes),yes)
        WAVE_MS ?= 500
    else
        WAVE_MS ?= 250
    endif
    PSP_HEAP_SIZE_KB ?= 4096
else ifeq ($(MODE),heap)
    WAVE_MS ?= 100
    PSP_HEAP_SIZE_KB ?= 4096
else ifeq ($(MODE),api)
    WAVE_MS ?= 1000
    PSP_HEAP_SIZE_KB ?= 2048
else
    WAVE_MS ?= 1000
    PSP_HEAP_SIZE_KB ?= 1024
endif

CFLAGS += -DAUDIO_COUNT=$(COUNT) -DAUDIO_FRAMES=$(FRAMES) \
          -DAUDIO_PITCH=$(PITCH) -DAUDIO_WAVE_MS=$(WAVE_MS)

LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = audio smoke (P9-S3)

# PSP-1000: 32 MB. Never request the 40 MB partition the PSPSDK default asks for.
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
LIBS += -lpspaudio -lpthread -latomic
