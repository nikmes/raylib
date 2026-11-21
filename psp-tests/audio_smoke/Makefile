# P9-S4 audio smoke test: Music, AudioStream and the S3 Sound modes.
#
#   make MODE=tone  COUNT=1  FRAMES=600            # S2 control: one tone
#   make MODE=api                                  # Sound/Wave API matrix
#   make MODE=mix   COUNT=16                       # 500 ms waves (16), 250 ms (32)
#   make MODE=pitch PITCH=0.5
#   make MODE=levels
#   make MODE=heap  COUNT=32
#   make MODE=music                                # OGG baseline + Music API matrix
#   make MODE=music FRAMES=18000 COUNT=8           # optional long music run
#   make MODE=musicmix COUNT=8 FRAMES=18000        # 5-minute soak
#   make MODE=stream STREAM_ARM=update|callback|processor
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
MUSIC ?= /p9s4_target.ogg
MUSIC_WAV ?= /p9s4_sound.wav
STREAM_ARM ?= update

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
else ifeq ($(MODE),music)
    CFLAGS += -DMODE_MUSIC
else ifeq ($(MODE),musicmix)
    CFLAGS += -DMODE_MUSICMIX
else ifeq ($(MODE),stream)
    CFLAGS += -DMODE_STREAM
else
    $(error MODE must be tone, sound, api, mix, pitch, levels, heap, music, musicmix or stream)
endif

ifeq ($(STREAM_ARM),update)
    CFLAGS += -DSTREAM_UPDATE
else ifeq ($(STREAM_ARM),callback)
    CFLAGS += -DSTREAM_CALLBACK
else ifeq ($(STREAM_ARM),processor)
    CFLAGS += -DSTREAM_PROCESSOR
else
    $(error STREAM_ARM must be update, callback or processor)
endif

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
else ifeq ($(MODE),musicmix)
    WAVE_MS ?= 500
    PSP_HEAP_SIZE_KB ?= 4096
else ifeq ($(MODE),music)
    WAVE_MS ?= 1000
    PSP_HEAP_SIZE_KB ?= 2048
else ifeq ($(MODE),api)
    WAVE_MS ?= 1000
    PSP_HEAP_SIZE_KB ?= 2048
else
    WAVE_MS ?= 1000
    PSP_HEAP_SIZE_KB ?= 1024
endif

CFLAGS += -DAUDIO_COUNT=$(COUNT) -DAUDIO_FRAMES=$(FRAMES) \
          -DAUDIO_PITCH=$(PITCH) -DAUDIO_WAVE_MS=$(WAVE_MS) \
          -DMUSIC_PATH=\"$(MUSIC)\" -DMUSIC_WAV_PATH=\"$(MUSIC_WAV)\"

LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = audio smoke (P9-S4)

# PSP-1000: 32 MB. Never request the 40 MB partition the PSPSDK default asks for.
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
LIBS += -lpspaudio -lpthread -latomic
