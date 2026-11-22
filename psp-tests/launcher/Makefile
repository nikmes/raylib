TARGET = launcher
BUILD_PRX = 1
OBJS = main.o raygui_impl.o $(patsubst samples/%.c,samples/%.o,$(wildcard samples/*.c))

CFLAGS = -Wall -I. -I../samples -DPLATFORM_PSP -DGRAPHICS_API_OPENGL_11 -DPSP_BATCH=1
ifdef AUTOLAUNCH
    CFLAGS += -DLAUNCHER_AUTOLAUNCH
endif
ifdef AUTO_INDEX
    CFLAGS += -DLAUNCHER_AUTO_INDEX=$(AUTO_INDEX)
endif

LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = raylib PSP samples

PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
LIBS += -lpspaudio -lpthread -latomic
