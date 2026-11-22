TARGET = input_keys
OBJS = main.o psp_module.o

# 900 frames is 15 s at 60 fps: enough to move the ball in all four directions.
FRAMES ?= 900

include ../../Makefile.inc
CFLAGS += -DFRAMES=$(FRAMES)
