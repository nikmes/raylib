# clear_probe - P10-S12 T2: clear-only work with/without GU_FAST_CLEAR_BIT.
BUILD_PRX = 1
TARGET = clear_probe
OBJS = main.o psp_counters.o

CFLAGS = -Wall -O2 -I../bunnymark -DWRAP_COUNTERS
LDFLAGS += -Wl,--wrap=glBegin -Wl,--wrap=glEnd \
           -Wl,--wrap=glVertex2f -Wl,--wrap=glVertex2i \
           -Wl,--wrap=glBindTexture \
           -Wl,--wrap=glDrawArrays -Wl,--wrap=glDrawElements \
           -Wl,--wrap=eglSwapBuffers

LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = clear probe (P10-S12 T2)
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

psp_counters.o: ../bunnymark/psp_counters.c ../bunnymark/psp_counters.h
	$(CC) $(CFLAGS) -I../bunnymark -c $< -o $@
