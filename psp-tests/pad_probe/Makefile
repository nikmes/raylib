# pad_probe - raw nub measurement tool (sprint 043, P8-S3).
#
# PSP-only: main.c reads sceCtrl directly with Peek. It links raylib only for
# drawing, and the Makefile is modelled on psp-tests/pad_api/Makefile.

BUILD_PRX = 1
TARGET = pad_probe
OBJS = main.o psp_module.o

CFLAGS = -Wall

LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = pad probe (raylib 6.2)

# PSP-1000: 32 MB. Never request the 40 MB partition the PSPSDK default asks for.
PSP_LARGE_MEMORY = 0

RUN ?= 1
IDLE_SECS ?= 0
MANUAL_SECS ?= 0
CFLAGS += -DRUN=$(RUN) -DIDLE_SECS=$(IDLE_SECS) -DMANUAL_SECS=$(MANUAL_SECS)

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
