TARGET = input_gamepad
OBJS = main.o psp_module.o

# 1800 frames is 30 s at 60 fps / 60 s at the observed 30 fps: enough for the
# full manual button and nub pass. Use `make FRAMES=600` for the standard harness.
FRAMES ?= 1800

include ../../Makefile.inc
CFLAGS += -DFRAMES=$(FRAMES)
