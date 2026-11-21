# pad_api - portable raylib gamepad API sample (sprint 041, P8-S1).
#
# The C source contains no PSP headers or symbols. This Makefile is the only
# PSP-aware part, and is modelled on psp-tests/basic_shapes/Makefile.

BUILD_PRX = 1
TARGET = pad_api
OBJS = main.o psp_module.o

CFLAGS = -Wall

LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = pad api (raylib 6.2)

# PSP-1000: 32 MB. Never request the 40 MB partition the PSPSDK default asks for.
PSP_LARGE_MEMORY = 0

FRAMES ?= 1800
CFLAGS += -DFRAMES=$(FRAMES)

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
