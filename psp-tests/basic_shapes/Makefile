# Basic shapes smoke test for raylib 6.2 on PSP.
#
# Derived from raylib4Psp/samples/shapes/basic_shapes (5.5-era). The only source
# change is that DrawCircleGradient() takes a Vector2 centre in 6.x. Kept in our
# tree rather than editing the reference corpus.

BUILD_PRX = 1
TARGET = basic_shapes
OBJS = main.o

CFLAGS = -Wall

# Only libraries build.mak does not already link.
LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = basic shapes (raylib 6.2)

# PSP-1000: 32 MB. Never request the 40 MB partition the PSPSDK default asks for.
PSP_LARGE_MEMORY = 0

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
