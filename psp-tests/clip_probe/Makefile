BUILD_PRX = 1
TARGET = clip_probe
OBJS = main.o
EDGE ?= 150
MESH3D ?= 0
CFLAGS = -Wall -O2 -DEDGE_X=$(EDGE)
ifeq ($(MESH3D),1)
    CFLAGS += -DMESH3D
endif
ifeq ($(NEAR),1)
    CFLAGS += -DNEAR_VERT
endif
LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = clip probe
PSP_LARGE_MEMORY = 0
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
