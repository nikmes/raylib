# ge_probe - P10-S15 T1: GE vs CPU time, raw GU in GU_SEND lists.
BUILD_PRX = 1
TARGET = ge_probe
OBJS = main.o

CFLAGS = -Wall -O2
LIBS = -lpspgum -lpspgu -lpspge -lpspdisplay -lpspvfpu -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = GE/CPU probe (P10-S15 T1)
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
