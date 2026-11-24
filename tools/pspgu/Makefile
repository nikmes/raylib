# pspgu - the GE driver that supplies the GL 1.1 subset raylib's GL11 path needs.
#
# Installed over $PSPDEV/psp/lib/libGL.a by tools/pspgu-build.sh, so the A/B against pspGL
# is one archive swap and the same binaries. Nothing in src/ knows which one it got.
#
#   make                      # libGL.a in this directory
#   make PSP_GU_DEPTH=0       # 2D-only build, 272 KiB of VRAM back
#   make PSP_GU_TRACE=1       # log the unimplemented symbols a scene still calls
#   make PSPGU_TEXSYNC_FIRST=0 # old TexFlush->TexSync order for the P10-S9 A/B

TARGET_LIB = libGL.a
OBJS = egl.o gl_state.o gl_matrix.o gl_draw.o gl_texture.o gl_immediate.o gl_query.o

PSPSDK ?= $(shell psp-config --pspsdk-path)

# -O2: the whole point of sprint 008's lesson. pspGL is built -Os.
CFLAGS = -O2 -G0 -Wall -Wno-unused-parameter -fno-strict-aliasing \
         -I$(PSPSDK)/include -I. $(EXTRA_CFLAGS)

ifdef PSP_GU_TRACE
    CFLAGS += -DPSP_GU_TRACE
endif
PSPGU_VERBOSE ?= 1
CFLAGS += -DPSPGU_VERBOSE=$(PSPGU_VERBOSE)
ifdef PSP_GU_DEPTH
    CFLAGS += -DPSP_GU_DEPTH=$(PSP_GU_DEPTH)
endif

# P10-S15 T1: serial replay - queue the frame as a GU_SEND list and wait for it with the
# CPU idle, logging CPU build and GE time per 50-frame window. Measurement build only.
ifdef PSPGU_REPLAY
    CFLAGS += -DPSPGU_REPLAY=1
endif

# P10-S15 T6a: list-local vertex memory through sceGuGetMemory (measurement arm).
ifdef PSPGU_LIST_VERTICES
    CFLAGS += -DPSPGU_LIST_VERTICES=$(PSPGU_LIST_VERTICES)
endif
ifeq ($(PSP_GU_TEXFMT),auto)
    CFLAGS += -DPSPGU_TEXFMT_AUTO=1
endif

# Guide: keep GU_CLIP_PLANES on in transform mode; =0 is the A/B arm.
PSPGU_CLIP_PLANES ?= 1
CFLAGS += -DPSPGU_CLIP_PLANES=$(PSPGU_CLIP_PLANES)

# Guide: drop dither while an additive blend is bound; =0 is the A/B arm.
PSPGU_DITHER_ADDITIVE ?= 1
CFLAGS += -DPSPGU_DITHER_ADDITIVE=$(PSPGU_DITHER_ADDITIVE)

# P10-S9 T1: the reference samples (local notes §5) put the read-out barrier before the cache
# flush. T1 measured it: it did not clear the band (656 vs 528 px), so the default
# stays 0 (the pre-P10-S9 order); =1 remains the measured arm.
PSPGU_TEXSYNC_FIRST ?= 0
CFLAGS += -DPSPGU_TEXSYNC_FIRST=$(PSPGU_TEXSYNC_FIRST)

# P10-S9 T1: corrected screen-target save for the FBO unbind. 2 is candidate (b),
# the adopted fix (0 diff at 8 frames); 0 and 1 remain the control arms.
PSPGU_FB_RESTORE ?= 2
CFLAGS += -DPSPGU_FB_RESTORE=$(PSPGU_FB_RESTORE)

all: $(TARGET_LIB)

# Vendored from the SDK's libpspvfpu.a. CP2 is unusable without the thread attribute, and
# setting it is pspvfpu_initcontext's job; keeping the object in this archive means an app
# that links -lGL needs no extra -lpspvfpu and the reference is a strong one, so the
# archive member is actually extracted.
PSPVFPU_LIB = $(PSPSDK)/lib/libpspvfpu.a
VFPU_STAGE  = .pspvfpu

$(TARGET_LIB): $(OBJS)
	rm -rf $(VFPU_STAGE) && mkdir -p $(VFPU_STAGE)
	cd $(VFPU_STAGE) && psp-ar x $(PSPVFPU_LIB)
	rm -f $@
	psp-ar rcs $@ $^ $(VFPU_STAGE)/pspvfpu.o

%.o: %.c pspgu_internal.h
	psp-gcc -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(OBJS) $(TARGET_LIB)
	rm -rf $(VFPU_STAGE)

.PHONY: all clean
