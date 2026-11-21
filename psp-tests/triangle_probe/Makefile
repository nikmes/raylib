BUILD_PRX = 1
TARGET = triangle_probe
OBJS = main.o
CFLAGS = -Wall
LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = triangle probe
PSP_LARGE_MEMORY = 0
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
