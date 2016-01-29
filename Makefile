# The project is in c++ for now because it makes using imgui a lot easier.
# Tools are mostly in c.
CFLAGS= -std=c99 -g -Wall
CXXFLAGS= -std=c++11 -g -Wall -Ilib -Ilib/imgui -Isrc

all: aissac

aissac: FORCE
	clang++ $(CXXFLAGS) -dynamiclib -undefined dynamic_lookup src/aissac.cpp -o bin/libaissac.dylib
	clang++ $(CXXFLAGS) `pkg-config --cflags sdl2` `pkg-config --libs sdl2` -framework OpenGL -framework CoreGraphics -framework IOSurface -framework CoreFoundation src/osx_aissac.cpp -o bin/aissac

# @TODO: Make this work different for aissac code vs osx_aissac vs tools?
check-syntax:
	clang++ -O /dev/null $(CXXFLAGS) -include "aissac.h" -S ${CHK_SOURCES}

tags:
	etags --declarations src/*.h src/*.c src/*.cpp lib/imgui/imgui.h

FORCE:
