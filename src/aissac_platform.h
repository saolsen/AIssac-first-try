/*
  Things the bot code needs to be able to do.
  Get the current image of the screen.
  Send keyboard / joystick input.
  Draw debug things to the screen.

  ProcessFrame will be called inline in sdl because it will be able to write to the screen.
  Need to start screen capture from the platform layer. Passing screens to ProcessFrame.
 */
#ifndef _aissac_platform_h
#define _aissac_platform_h

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

// libs available to everyone
#include "imgui.h"

// Set up some type aliases I like.

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float    r32;
typedef double   r64;

// Common macros
#define LEN(e) (sizeof(e) / sizeof(e[0]))
#define INFO(format, ...) fprintf(stderr, "[INFO]  (%s:%d) " format "\n", \
                                      __FILE__, __LINE__, ##__VA_ARGS__)
#define ERROR(format, ...) fprintf(stderr, "[ERROR] (%s:%d) " format "\n", \
                                      __FILE__, __LINE__, ##__VA_ARGS__)
#define ASSERT(c) assert(c)

// Types for communication between platform code and AI code.

// Input for the game from the AI.
typedef struct {
    u32 a;
} GameInput;

// Platform specific functions for the AI code to use.
typedef struct {
    i32 nah;
} PlatformAPI;

// Memory for the AI to use.
// Storing things here instead of allocating directly allows us to reload the AI at any time
// because it's stateless.
typedef struct {
    u64 persistent_storage_size;
    u8* persistent_storage;

    bool executable_reloaded;
    PlatformAPI platform_api;
} Memory;

// Input to the AI.
typedef struct {
    u32 a;

    // @TODO: Figure out what all goes in here. I think I really want timing info
    // like ticks since last frame and frame latency info and stuff like that.
    u32 screen_width;
    u32 screen_height;
    u8* screen_data;
} GameState;

// Take the AI memory and the current gamestate and return input to be sent to the game.
typedef GameInput (*ProcessFrame)(Memory *memory, GameState);

// @TODO: Will probably need some more utility API stuff in here too so the AI can load files
//        and start threads and things.

typedef struct {
    u64 memory_needed;
    ProcessFrame process_frame;
} AI;

extern AI aissac;

#ifdef __cplusplus
}
#endif
    
#endif
