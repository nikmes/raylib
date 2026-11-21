# pspgu - the GE driver that supplies the GL 1.1 subset raylib's GL11 path needs.
#
# Installed over $PSPDEV/psp/lib/libGL.a by tools/pspgu-build.sh, so the A/B against pspGL
# is one archive swap and the same binaries. Nothing in src/ knows which one it got.
#
#   make                      # libGL.a in this directory
#   make PSP_GU_DEPTH=0       # 2D-only build, 272 KiB of VRAM back
#   make PSP_GU_TRACE=1       # log the unimplemented symbols a scene still calls

TARGET_LIB = libGL.a
OBJS = egl.o gl_state.o gl_matrix.o gl_draw.o gl_stub.o

PSPSDK ?= $(shell psp-config --pspsdk-path)

# -O2: the whole point of sprint 008's lesson. pspGL is built -Os.
CFLAGS = -O2 -G0 -Wall -Wno-unused-parameter -fno-strict-aliasing \
         -I$(PSPSDK)/include -I. $(EXTRA_CFLAGS)

ifdef PSP_GU_TRACE
    CFLAGS += -DPSP_GU_TRACE
endif
ifdef PSP_GU_DEPTH
    CFLAGS += -DPSP_GU_DEPTH=$(PSP_GU_DEPTH)
endif

all: $(TARGET_LIB)

$(TARGET_LIB): $(OBJS)
	psp-ar rcs $@ $^

%.o: %.c pspgu_internal.h
	psp-gcc -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(OBJS) $(TARGET_LIB)

.PHONY: all clean
