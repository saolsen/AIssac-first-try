/*
  This is the OSX/SDL platform layer.
  
  * Debug output is displayed using SDL, OpenGL and nanovg.
  * Screen capture is done using osx CoreGraphics and IOSurface apis.
  * Game input is done using osx CoreFoundation apis.
  * @TODO: Process snapshotting/checkpointing (for livecoding) is done using TBD.
 */

#include <SDL2/SDL.h>

#include "aissac_platform.h"
#include <OpenGL/gl3.h>
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg_gl.h"


i32
main()
{
    // Setup SDL
    if(SDL_Init(SDL_INIT_VIDEO)) {
        ERROR("Error initializing SDL: %s", SDL_GetError());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window *window = SDL_CreateWindow("AIssac",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          1920,
                                          1080,
                                          SDL_WINDOW_OPENGL |
                                          SDL_WINDOW_RESIZABLE |
                                          SDL_WINDOW_ALLOW_HIGHDPI);

    if (!window) {
        ERROR("Error creating window: %s", SDL_GetError());
    }

    i32 width, height;
    SDL_GL_GetDrawableSize(window, &width, &height);
    INFO("Drawable Resolution: %d x %d", width, height);

    SDL_GLContext context = SDL_GL_CreateContext(window);

    // Use Vsync
    if (SDL_GL_SetSwapInterval(1) < 0) {
        ERROR("Unable to set VSync! SDL Error: %s", SDL_GetError());
    }

    // Setup nvg
    NVGcontext* vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
    if (vg == NULL) {
        ERROR("Could not init nanovg.");
    }

    bool running = true;
    while(running) {
        i32 w, h;
	SDL_GetWindowSize(window, &w, &h);

        i32 draw_w, draw_h;
        SDL_GL_GetDrawableSize(window, &draw_w, &draw_h);

        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            switch(event.type) {
            case SDL_QUIT:
                running = false;
                break;
            };
        }

        if(!running) {
            break;
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Run Tick

        SDL_GL_SwapWindow(window);

    }

    INFO("Shutting Down");
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
