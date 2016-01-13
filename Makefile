CXXFLAGS= -std=c++11 -g -Wall -Iimgui

all: aissac

aissac: FORCE
	clang++ $(CXXFLAGS) -dynamiclib -undefined dynamic_lookup aissac.cpp -o libaissac.dylib
	clang++ $(CXXFLAGS) `pkg-config --cflags sdl2` `pkg-config --libs sdl2` -framework OpenGL -framework CoreGraphics -framework IOSurface -framework CoreFoundation osx_aissac.cpp -o aissac

check-syntax:
	clang++ -O /dev/null $(CXXFLAGS) -include "aissac.h" -S ${CHK_SOURCES}

tags:
	etags --declarations ./*.h ./*.c ./*.cpp imgui/imgui.h

FORCE:
