# Sprint 017 T1: upload/swizzle matrix.
#
#   make MATRIX=size
#   make MATRIX=fmt
#
# Run the same binary on the default 8888 driver and on --texfmt=auto.

BUILD_PRX = 1
TARGET = texture_matrix
OBJS = main.o

MATRIX ?= size
CFLAGS = -Wall
ifeq ($(MATRIX),size)
    CFLAGS += -DMATRIX_SIZE
else ifeq ($(MATRIX),fmt)
    CFLAGS += -DMATRIX_FMT
else
    $(error MATRIX must be size or fmt)
endif

LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = texture matrix (raylib 6.5-dev)
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
