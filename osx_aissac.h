#ifndef _osx_aissic_h
#define _osx_aissic_h

#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>

#include <unistd.h>
#include <dispatch/dispatch.h>
#include <CoreGraphics/CoreGraphics.h>
#include <IOSurface/IOSurface.h>
#include <libkern/OSAtomic.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#include "imgui.h"
#include "imgui_draw.cpp"
#include "imgui.cpp"
#pragma clang diagnostic pop

typedef struct frame {
    struct frame *next_frame;
    volatile i32 is_ready_for_processing;

    i32 frame_width;
    i32 frame_height;
    i32 pixel_components;
    u8 *data;
} Frame;

typedef struct {
    i32 is_active;
    
    Frame *head;

    Frame *free_frames_list;
    Frame *frames_storage;
    u8 *frames_data_storage; // Data is stored seperately so it's not loaded when jumping down
                             // pointers in Frame lists.
} FrameQueue;

#endif
