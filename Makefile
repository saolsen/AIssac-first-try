CFLAGS= -std=c11 -g -Wall -O0 -fPIC -Wno-missing-braces -Inanovg/src

all: aissac

aissac: FORCE
	clang $(CFLAGS) `pkg-config --cflags sdl2` `pkg-config --libs sdl2` -framework OpenGL -Lnanovg/build -lnanovg -framework CoreGraphics -framework IOSurface -framework CoreFoundation -o aissac osx_aissac.c

check-syntax:
	clang -O /dev/null $(CFLAGS) -include "aissac.h" -S ${CHK_SOURCES}

tags:
	etags --declarations ./*.h ./*.c nanovg/src/nanovg.h

nanovg: #TODO: install this without premake4 so the only dep is sdl2 (that can be source too)


FORCE:
