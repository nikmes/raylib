# P11-S12 helper-layer probe (headless).
#
#   make PSP_VFPU_HELPERS=1   # VFPU/SDK arm
#   make PSP_VFPU_HELPERS=0   # scalar fallback arm

BUILD_PRX = 1
TARGET = math_probe
OBJS = main.o

PSP_VFPU_HELPERS ?= 1
PSP_VFPU_GUM ?= 0
PSP_VFPU_COPY ?= 0
CFLAGS = -Wall -O2 -DPLATFORM_PSP -DPSP_VFPU_HELPERS=$(PSP_VFPU_HELPERS) \
         -DRLPSP_VFPU_GUM=$(PSP_VFPU_GUM) -DRLPSP_VFPU_COPY=$(PSP_VFPU_COPY) \
         -fno-strict-aliasing -I../../src

LIBS = -lpspmath -lpspgum -lpspvfpu -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = math probe (P11-S12)

PSP_LARGE_MEMORY = 0
PSP_HEAP_SIZE_KB = 1024

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
